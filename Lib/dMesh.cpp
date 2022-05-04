#include <stdio.h>
#include <vector>
#include "Mesh.h"
#include "dMesh.h"
#include "GLXtras.h"
#include "Misc.h"

using std::vector;

namespace {
	GLuint meshShader = 0;

	const char* defMeshVertShader = R"(
        #version 410 core
        in vec3 point;
        in vec3 normal;
        in vec2 uv;
        out vec3 vPoint;
        out vec3 vNormal;
        out vec2 vUv;
        uniform mat4 modelview;
        uniform mat4 persp;
        void main() {
            vPoint = (modelview*vec4(point, 1)).xyz;
            vNormal = (modelview*vec4(normal, 0)).xyz;
            gl_Position = persp*vec4(vPoint, 1);
            vUv = uv;
        }
	)";

	const char* defMeshFragShader = R"(
        #version 410 core
        in vec3 vPoint;
        in vec3 vNormal;
        in vec2 vUv;
        out vec4 pColor;
        uniform vec3 light;
        uniform sampler2D textureName;
	    uniform int useTexture = 0;
        void main() {
            vec3 N = normalize(vNormal);       // surface normal
            vec3 L = normalize(light-vPoint);  // light vector
            vec3 E = normalize(vPoint);        // eye vector
            vec3 R = reflect(L, N);            // highlight vector
            float d = abs(dot(N, L));          // two-sided diffuse
            float s = abs(dot(R, E));          // two-sided specular
            float intensity = clamp(d+pow(s, 50), 0, 1);
            vec3 color = useTexture == 1 ? texture(textureName, vUv).rgb : vec3(1);
            pColor = vec4(intensity*color, 1);
        }
	)";
}

dMesh::~dMesh() {
    if (vArray > 0) glDeleteVertexArrays(1, &vArray);
    if (vBuffer > 0) glDeleteBuffers(1, &vBuffer);
    if (iBuffer > 0) glDeleteBuffers(1, &iBuffer);
}

GLuint dMesh::GetMeshShader() {
    if (!meshShader)
        meshShader = LinkProgramViaCode(&defMeshVertShader, &defMeshFragShader);
    return meshShader;
}

GLuint dMesh::UseMeshShader() {
    GLuint s = GetMeshShader();
    GLint prog = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    if (prog != s) {
        glUseProgram(s);
    }
    return s;
}

void dMesh::Buffer() {
    int numPts = points.size(), numNorms = normals.size(), numUvs = uvs.size();
    if (!numPts || numPts != numNorms || numPts != numUvs) {
        fprintf(stderr, "dMesh : Mesh missing points, normals, or uvs\n");
        return;
    }
    GLuint shader = GetMeshShader();
    glGenVertexArrays(1, &vArray);
    glBindVertexArray(vArray);
    glGenBuffers(1, &vBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
    size_t sizePts = points.size() * sizeof(vec3);
    size_t sizeNorms = normals.size() * sizeof(vec3);
    size_t sizeUvs = uvs.size() * sizeof(vec2);
    size_t bufSize = sizePts + sizeNorms + sizeUvs;
    glBufferData(GL_ARRAY_BUFFER, bufSize, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizePts, points.data());
    glBufferSubData(GL_ARRAY_BUFFER, sizePts, sizeNorms, normals.data());
    glBufferSubData(GL_ARRAY_BUFFER, sizePts + sizeNorms, sizeUvs, uvs.data());
    glGenBuffers(1, &iBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles.size() * sizeof(int3), triangles.data(), GL_STATIC_DRAW);
    VertexAttribPointer(shader, "point", 3, 0, (void*)0);
    VertexAttribPointer(shader, "normal", 3, 0, (void*)sizePts);
    VertexAttribPointer(shader, "uv", 2, 0, (void*)(sizePts + sizeNorms));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    if (texUnit && texName) glBindTexture(GL_TEXTURE_2D, 0);
}

void dMesh::PreDisplay() {
    int numPts = points.size(), numNorms = normals.size(), numUvs = uvs.size(), numTris = triangles.size();
    if (!numPts || !numNorms || !numUvs || !numTris) {
        fprintf(stderr, "dMesh : Mesh not initialized before display call\n");
    }
    glBindVertexArray(vArray);
    int shader = UseMeshShader();
    preDisp = true;
}

void dMesh::Display(Camera camera, mat4* m) {
    int numPts = points.size(), numNorms = normals.size(), numUvs = uvs.size(), numTris = triangles.size();
    if (!numPts || !numNorms || !numUvs || !numTris) {
        fprintf(stderr, "dMesh : Mesh not initialized before display call\n");
    }
    int shader;
    if (!preDisp) {
        glBindVertexArray(vArray);
        shader = UseMeshShader();
    } else {
        shader = GetMeshShader();
        preDisp = false;
    }
    if (texUnit && texName) {
        glActiveTexture(GL_TEXTURE0 + texUnit);
        glBindTexture(GL_TEXTURE_2D, texName);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    SetUniform(shader, "useTexture", texUnit ? 1 : 0);
    if (texUnit) 
        SetUniform(shader, "textureName", (int)texName);
    SetUniform(shader, "persp", camera.persp);
    SetUniform(shader, "modelview", camera.modelview * *m * transform);
    glDrawElements(GL_TRIANGLES, triangles.size() * sizeof(int3), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

bool dMesh::Read(char* objFilename, mat4* m) {
    if (!ReadAsciiObj(objFilename, points, triangles, &normals, &uvs, nullptr, nullptr, nullptr, nullptr)) {
        fprintf(stderr, "dMesh : Can't read obj '%s'\n", objFilename);
        return false;
    }
    Normalize(points, 1.0f);
    Buffer();
    if (m)
        transform = *m;
    return true;
}

bool dMesh::Read(char* objFilename, char* texFilename, int textureUnit, mat4* m) {
    texUnit = textureUnit;
    texName = LoadTexture(texFilename, texUnit);
    if (!texName) {
        fprintf(stderr, "dMesh : Bad texture name\n");
        return false;
    }
    if (!textureUnit) {
        fprintf(stderr, "dMesh : Bad texture unit\n");
        return false;
    }
    if (!Read(objFilename, m))
        return false;
    return true;
}