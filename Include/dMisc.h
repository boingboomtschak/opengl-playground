// dMisc.h - Miscellaneous utilities

#ifndef DMISC_HDR
#define DMISC_HDR

#include <string>
#include <stdexcept>
#include <vector>
#include "stb_image.h"
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::runtime_error;
using std::string;

inline GLuint loadTexture(string filename, bool mipmap = true, GLint min_filter = GL_LINEAR, GLint mag_filter = GL_NEAREST) {
	int w, h;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc* data = stbi_load(filename.c_str(), &w, &h, 0, STBI_rgb_alpha);
	if (!data) 
		throw runtime_error("Failed to read '" + filename + "'" + string(stbi_failure_reason()));
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	if (mipmap) glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	stbi_image_free(data);
	return texture;
}

#endif
