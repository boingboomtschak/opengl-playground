// dMesh.h - Render pass agnostic mesh container

#ifndef DMESH_HDR
#define DMESH_HDR

#include <vector>
#include <stdexcept>
#include "glad.h"
#include "VecMat.h"
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

struct dMesh {
    mat4 model = mat4();
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint transform_VBO = 0;
    GLuint EBO = 0;
    GLuint texture = 0;
    ObjData objData;
    GLsizei num_instances;
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
    void allocate() {
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);
        size_t pSize = objData.points.size() * sizeof(vec3);
        size_t uSize = objData.uvs.size() * sizeof(vec2);
        size_t nSize = objData.normals.size() * sizeof(vec3);
        size_t vBufSize = pSize + uSize + nSize;
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vBufSize, NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, pSize, objData.points.data());
        glBufferSubData(GL_ARRAY_BUFFER, pSize, uSize, objData.uvs.data());
        glBufferSubData(GL_ARRAY_BUFFER, pSize + uSize, nSize, objData.normals.data());
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(pSize));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(pSize + uSize));
        size_t iSize = objData.indices.size() * sizeof(int3);
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, iSize, objData.indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);
    }
    void cleanup() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (texture) glDeleteTextures(1, &texture);
        if (transform_VBO) glDeleteBuffers(1, &transform_VBO);
    }
    void render() {
        glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawElements(GL_TRIANGLES, (GLsizei)(objData.indices.size() * sizeof(int3)), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    void setupInstances(vector<mat4>& transforms) {
        // Clean up previous VArray/VBuffer
        if (transform_VBO) glDeleteBuffers(1, &transform_VBO);
        num_instances = transforms.size();
        size_t size_mat4 = sizeof(mat4);
        // Setup transform buffer in VAO
        glBindVertexArray(VAO);
        glGenBuffers(1, &transform_VBO);
        glBindBuffer(GL_ARRAY_BUFFER, transform_VBO);
        GLsizei tfSize = transforms.size() * size_mat4;
        glBufferData(GL_ARRAY_BUFFER, tfSize, NULL, GL_STATIC_DRAW);
        for (size_t i = 0; i < transforms.size(); i++) {
            mat4 t = Transpose(transforms[i]);
            glBufferSubData(GL_ARRAY_BUFFER, i * size_mat4, size_mat4, &t);
        }
        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(i + 3);
            glVertexAttribPointer(i + 3, 4, GL_FLOAT, GL_FALSE, sizeof(mat4), (GLvoid*)(i * sizeof(vec4)));
            glVertexAttribDivisor(i + 3, 1);
        } 
        glBindVertexArray(0);
    }
    void renderInstanced() {
        glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)(objData.indices.size() * sizeof(int3)), GL_UNSIGNED_INT, 0, num_instances);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
};

#endif
