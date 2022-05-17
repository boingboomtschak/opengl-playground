// dMesh.h - Render pass agnostic mesh container

#include <vector>
#include <stdexcept>
#include "glad.h"
#include "VecMat.h"
#include "GLXtras.h"
#include "dMisc.h"

using std::vector;
using std::runtime_error;

/* dMesh does a number of things to be render pass agnostic:
 - Creates its own internal VAO, VBO, and EBO
 - Maps vertex attributes (points, normals, uvs) to specific locations (TODO, and then write out location values later)
 - Binds its VAO to render
 - Loads a texture from file using stb_image
 - Binds its texture to GL_TEXTURE_2D on unit 0
 - Unbinds its VAO and texture after rendering
 - Cleans up its own containers and texture on deallocation

Render passes using it should:
 - Expect <ATTRIBUTE> at location <LOCATION INDEX> (TODO)
 - Expect its texture to be bound to unit 0
*/

inline struct dMesh {
    mat4 model = mat4();
    GLuint vArray = 0;
    GLuint vBuffer = 0;
    GLuint iBuffer = 0;
    GLuint texture = 0;
    ObjData objData;
    dMesh() { };
    dMesh(vector<vec3> points, vector<vec2> uvs, vector<vec3> normals, vector<int3> indices, string texFilename, bool texMipmap = true) {
        objData.points = points;
        objData.uvs = uvs;
        objData.normals = normals;
        objData.indices = indices;
        texture = loadTexture(texFilename, texMipmap);
        if (texture < 0)
            throw runtime_error("Failed to read texture '" + texFilename + "'!");
        allocate();
    }
    dMesh(string objFilename, string texFilename, mat4 modelTransform) {
        objData = readObj(objFilename);
        normalizePoints(objData.points, 1.0f);
        texture = loadTexture(texFilename);
        if (texture < 0)
            throw runtime_error("Failed to read texture '" + texFilename + "'!");
        model = modelTransform;
        allocate();
    }
    ~dMesh() {
        glDeleteVertexArrays(1, &vArray);
        glDeleteBuffers(1, &vBuffer);
        glDeleteBuffers(1, &iBuffer);
        glDeleteTextures(1, &texture);
    }
    void allocate() {
        glGenVertexArrays(1, &vArray);
        glBindVertexArray(vArray);
        size_t pSize = objData.points.size() * sizeof(vec3);
        size_t nSize = objData.normals.size() * sizeof(vec3);
        size_t uSize = objData.uvs.size() * sizeof(vec2);
        size_t vBufSize = pSize + nSize + uSize;
        glGenBuffers(1, &vBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
        glBufferData(GL_ARRAY_BUFFER, vBufSize, NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, pSize, objData.points.data());
        glBufferSubData(GL_ARRAY_BUFFER, pSize, nSize, objData.normals.data());
        glBufferSubData(GL_ARRAY_BUFFER, pSize + nSize, uSize, objData.uvs.data());
        size_t iSize = triangles.size() * sizeof(int3);
        glGenBuffers(1, &iBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, iSize, objData.indices.data(), GL_STATIC_DRAW);
        VertexAttribPointer(program, "point", 3, 0, 0); // TODO
        VertexAttribPointer(program, "normal", 3, 0, (GLvoid*)(pSize)); // TODO
        VertexAttribPointer(program, "uv", 2, 0, (GLvoid*)(pSize + nSize)); // TODO
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    void render() {
        glBindVertexArray(vArray);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawElements(GL_TRIANGLES, (GLsizei)(objData.indices.size() * sizeof(int3)), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
