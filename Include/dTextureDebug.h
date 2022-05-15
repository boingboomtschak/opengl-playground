// dTextureDebug.h : Debugging textures

#ifndef DTEXDEBUG_HDR
#define DTEXDEBUG_HDR

#include <stdexcept>
#include <vector>
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::runtime_error;

namespace {

GLuint quadProgram = 0;
GLuint quadVArray = 0;
GLuint quadVBuffer = 0;

vector<vec2> quadPoints{
    {-1, -1}, {1, -1}, {1, 1},
    {1, 1}, {-1, 1}, {-1, -1}
};

const char* quadVert = R"(
    #version 410 core
    in vec2 point;
    out vec2 uv;
    void main() {
        uv = vec2(point.x / 2 + 0.5, point.y / 2 + 0.5);
        gl_Position = vec4(point, 0, 1);
    }
)";
const char* quadFrag = R"(
    #version 410 core
    in vec2 uv;
    out vec4 color;
    uniform sampler2D tex;
    void main() {
        color = vec4(vec3(texture(tex, uv).r), 1.0);
    }
)";

}

namespace dTextureDebug {
    void show(GLuint texture, int x, int y, int w, int h) {
        if (!quadProgram)
            if (!(quadProgram = LinkProgramViaCode(&quadVert, &quadFrag)))
                throw runtime_error("Failed to compile debug quad program!");
        if (!quadVArray) {
            glGenVertexArrays(1, &quadVArray);
            glBindVertexArray(quadVArray);
            glGenBuffers(1, &quadVBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBuffer);
            glBufferData(GL_ARRAY_BUFFER, (GLsizei)(quadPoints.size() * sizeof(vec2)), quadPoints.data(), GL_STATIC_DRAW);
            VertexAttribPointer(quadProgram, "point", 2, 0, 0);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        GLint _viewport[4];
        glGetIntegerv(GL_VIEWPORT, _viewport);
        glViewport(x, y, w, h);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(quadProgram);
        glBindVertexArray(quadVArray);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        SetUniform(quadProgram, "tex", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
    }
}

#endif