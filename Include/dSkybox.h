// dSkybox.h : Skybox rendering, taking cubemap as input

#ifndef DSKYBOX_HDR
#define DSKYBOX_HDR

#include <glad.h>
#include <vector>
#include <string>
#include <stdexcept>
#include "VecMat.h"
#include "GLXtras.h"
#include "stb_image.h"

using std::vector;
using std::string;
using std::runtime_error;

struct dSkybox {
	dSkybox() { };
	GLuint texture = 0, texUnit = 1;
	void setup();
	void cleanup();
	void loadCubemap(string skyboxPath);
	void loadCubemap(vector<string> faceTextures);
	void draw(vec3 view_dir, mat4 persp);
};

#endif

