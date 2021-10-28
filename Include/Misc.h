// Misc.h

#ifndef MISC_HDR
#define MISC_HDR

#include <glad.h>
#include <string.h>
#include <time.h>
#include "VecMat.h"

// Misc
bool KeyDown(int button);
bool Shift();
bool Control();
std::string GetDirectory();
time_t FileModified(const char *name);
bool FileExists(const char *name);

// Sphere
int LineSphere(vec3 ln1, vec3 ln2, vec3 center, float radius, vec3 &p1, vec3 &p2);
	// set points intersected by line with sphere, return # intersection
	// line defined by ln1, ln2
	// sphere defined by center and radius
	// p1 and p2 set according to # hits; return # hits

float RaySphere(vec3 base, vec3 v, vec3 center, float radius);
	// return least pos alpha of ray and sphere (or -1 if none)
	// v presumed unit length

// Image file
unsigned char *ReadTarga(const char *filename, int *width, int *height, int *bytesPerPixel = NULL);
	// allocate width*height pixels, set them from file, return pointer
	// this memory should be freed by the caller
	// expects 24 bpp
	// *** pixel data is BGR format ***

bool TargaSize(const char *filename, int &width, int &height);

bool WriteTarga(const char *filename, unsigned char *pixels, int width, int height);
	// save raster to named Targa file

bool WriteTarga(const char *filename);
	// as above but with entire application raster

// Texture
//     textureUnit is assigned by the programmer: it is the OpenGL texture resource
//         it specifies the uniform sampler2D and the active texture
//     textureName is assigned by OpenGL: it is needed by glBindTexture to bind
//         to an active texture and by glDeleteBuffers

GLuint LoadTexture(const char *filename, GLuint textureUnit, bool mipmap = true);
	// for arbitrary image format, load image file into given texture unit; return texture name

GLuint LoadTargaTexture(const char *targaFilename, GLuint textureUnit, bool mipmap = true);
	// load .tga file into given texture unit; return texture name (id)

GLuint LoadTexture(unsigned char *pixels, int width, int height, int bpp, GLuint textureUnit, bool bgr = false, bool mipmap = true);
	// bpp is bytes per pixel
	// load pixels as given texture unit; return texture name (ID)

void LoadTexture(unsigned char *pixels, int width, int height, int bpp, GLuint textureUnit, GLuint textureName, bool bgr, bool mipmap);

// Bump map
unsigned char *GetNormals(unsigned char *depthPixels, int width, int height);
	// return normal pixels (3 bytes/pixel) that correspond with depth pixels (presumed 3 bytes/pixel)
	// the memory returned should be freed by the caller

#endif
