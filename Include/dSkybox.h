// dSkybox.h : Skybox rendering, taking cubemap as input

#ifndef DSKYBOX_HDR
#define DSKYBOX_HDR

#include <glad.h>
#include <vector>
#include <string>
#include <stdexcept>
#include "dRenderPass.h"
#include "VecMat.h"
#include "stb_image.h"

using std::vector;
using std::string;
using std::runtime_error;

namespace {

GLuint skyboxVArray = 0;
GLuint skyboxVBuffer = 0;

vector<vec3> skyboxPoints{
	{ -1.0f,  1.0f, -1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ 1.0f,  1.0f, -1.0f },
	{ -1.0f,  1.0f, -1.0f },
	{ -1.0f, -1.0f,  1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ -1.0f,  1.0f, -1.0f },
	{ -1.0f,  1.0f, -1.0f },
	{ -1.0f,  1.0f,  1.0f },
	{ -1.0f, -1.0f,  1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ 1.0f, -1.0f,  1.0f },
	{ 1.0f,  1.0f,  1.0f },
	{ 1.0f,  1.0f,  1.0f },
	{ 1.0f,  1.0f, -1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ -1.0f, -1.0f,  1.0f },
	{ -1.0f,  1.0f,  1.0f },
	{ 1.0f,  1.0f,  1.0f },
	{ 1.0f,  1.0f,  1.0f },
	{ 1.0f, -1.0f,  1.0f },
	{ -1.0f, -1.0f,  1.0f },
	{ -1.0f,  1.0f, -1.0f },
	{ 1.0f,  1.0f, -1.0f },
	{ 1.0f,  1.0f,  1.0f },
	{ 1.0f,  1.0f,  1.0f },
	{ -1.0f,  1.0f,  1.0f },
	{ -1.0f,  1.0f, -1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ -1.0f, -1.0f,  1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ 1.0f, -1.0f, -1.0f },
	{ -1.0f, -1.0f,  1.0f },
	{ 1.0f, -1.0f,  1.0f }
};

const char* skyboxVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	out vec3 texCoord;
	uniform mat4 view;
	uniform mat4 persp;
	void main() {
		texCoord = point;
		gl_Position = (persp * view * vec4(point, 1.0)).xyww;
	}
)";

const char* skyboxFrag = R"(
	#version 410 core
	in vec3 texCoord;
	out vec4 pColor;
	uniform samplerCube skybox;
	void main() {
		pColor = texture(skybox, texCoord);
	}
)";

RenderPass skyboxPass;

}

struct dSkybox {
	dSkybox() { };
	GLuint texture = 0, texUnit = 1;
	void setup() {
        if (!skyboxPass.program)
            skyboxPass.loadShaders(&skyboxVert, &skyboxFrag);
		if (!skyboxVArray) {
			glGenVertexArrays(1, &skyboxVArray);
			glBindVertexArray(skyboxVArray);
			glGenBuffers(1, &skyboxVBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, skyboxVBuffer);
			glBufferData(GL_ARRAY_BUFFER, skyboxPoints.size() * sizeof(vec3), skyboxPoints.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDisableVertexAttribArray(0);
		}
	}
	void cleanup() {
		if (texture)
			glDeleteTextures(1, &texture);
		if (!skyboxVArray) {
			glDeleteVertexArrays(1, &skyboxVArray);
			glDeleteBuffers(1, &skyboxVBuffer);
		}
	}
	void loadCubemap(string skyboxPath) {
		vector<string> faces{
		skyboxPath + "posx.png",
		skyboxPath + "negx.png",
		skyboxPath + "posy.png",
		skyboxPath + "negy.png",
		skyboxPath + "posz.png",
		skyboxPath + "negz.png"
		};
		loadCubemap(faces);
	}
	void loadCubemap(vector<string> faceTextures) {
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
		int w, h, c;
		stbi_set_flip_vertically_on_load(false);
		for (int i = 0; i < faceTextures.size(); i++) {
			stbi_uc* data = stbi_load(faceTextures[i].c_str(), &w, &h, &c, 0);
			if (!data)
				throw runtime_error("Failed to load texture '" + faceTextures[i] + "'!");
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
	void draw(vec3 view_dir, mat4 persp) {
		glDepthMask(GL_FALSE);
        skyboxPass.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
		// Recreating camera view matrix without translation
		mat4 m = LookAt(vec3(0, 0, 0), view_dir, vec3(0, 1, 0));
        skyboxPass.set("view", m);
        skyboxPass.set("persp", persp);
        skyboxPass.set("skybox", 0);
		glBindVertexArray(skyboxVArray);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glDepthMask(GL_TRUE);
	}
};

#endif

