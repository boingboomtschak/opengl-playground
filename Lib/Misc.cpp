// Misc.cpp (c) 2019-2022 Jules Bloomenthal

#include <glad.h>
#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include "Draw.h"
#include "Misc.h"
#include <sys/stat.h>

#define STB_IMAGE_IMPLEMENTATION
#include "STB_Image.h"

// Misc

bool KeyDown(int button) {
	static int nShortBits = 8*sizeof(SHORT);
	static SHORT shortMSB = 1 << (nShortBits-1);
	return (GetKeyState(button) & shortMSB) != 0;
}

bool Shift() { return KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT); }

bool Control() { return KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL); }

std::string GetDirectory() {
	char buf[256];
	GetCurrentDirectoryA(256, buf);
	for (size_t i = 0; i < strlen(buf); i++)
		if (buf[i] == '\\') buf[i] = '/';
	return std::string(buf)+std::string("/");
}

time_t FileModified(const char *name) {
	struct stat info;
	stat(name, &info);
	return info.st_mtime;
}

bool FileExists(const char *name) {
	return fopen(name, "r") != NULL;
}

// Sphere

int LineSphere(vec3 ln1, vec3 ln2, vec3 center, float radius, vec3 &p1, vec3 &p2) {
	// set points intersected by line ln1,ln2 with sphere, return # intersection
	vec3 base = ln1, v = normalize(ln2-ln1);
	vec3 q = base-center;
	float vDot = dot(v, q);
	float sq = vDot*vDot-dot(q, q)+radius*radius;
	if (sq < 0)
		return 0;
	float root = sqrt(sq), a1 = -vDot-root, a2 = -vDot+root;
	p1 = base+a1*v;
	p2 = base+a2*v;
	return root > -FLT_EPSILON && root < FLT_EPSILON? 1 : 2;
}

float RaySphere(vec3 base, vec3 v, vec3 center, float radius) {
	// return least pos alpha of ray and sphere (or -1 if none)
	vec3 q = base-center;
	float vDot = dot(v, q);
	float sq = vDot*vDot-dot(q, q)+radius*radius;
	if (sq < 0)
		return -1;
	float root = sqrt(sq), a = -vDot-root;
	return a > 0? a : -vDot+root;
}

// Image File

unsigned char *ReadTarga(const char *filename, int *width, int *height, int *bytesPerPixel) {
	// open targa file, read header, return pointer to pixels
	FILE *in = fopen(filename, "rb");
	if (in) {
		char tgaHeader[18]; // short tgaHeader[9];
		fread(tgaHeader, sizeof(tgaHeader), 1, in);
		int w = (int) *((short *) (tgaHeader+12));
		int h = (int) *((short *) (tgaHeader+14));
		// allocate, read pixels
		*width = w;
		*height = h;
		int bitsPerPixel = tgaHeader[16];
		int bytesPP = bitsPerPixel/8;
		if (bytesPerPixel)
			*bytesPerPixel = bytesPP;
		int bytesPerImage = w*h*(bytesPP);
		unsigned char *pixels = new unsigned char[bytesPerImage];
		fread(pixels, bytesPerImage, 1, in);
		fclose(in);
		return pixels;
	}
	printf("can't open %s\n", filename);
	return NULL;
}

bool TargaSize(const char *filename, int &width, int &height) {
	FILE *in = fopen(filename, "rb");
	if (in) {
		char tgaHeader[18];
		fread(tgaHeader, sizeof(tgaHeader), 1, in);
		width = (int) *((short *) (tgaHeader+12));
		height = (int) *((short *) (tgaHeader+14));
		fclose(in);
		return true;
	}
	return false;
}

bool WriteTarga(const char *filename, unsigned char *pixels, int width, int height) {
	FILE *out = fopen(filename, "wb");
	if (!out) {
		printf("can't save %s\n", filename);
		return false;
	}
	short tgaHeader[9];
	tgaHeader[0] = 0;
	tgaHeader[1] = 2;
	tgaHeader[2] = 0;
	tgaHeader[3] = 0;
	tgaHeader[4] = 0;
	tgaHeader[5] = 0;
	tgaHeader[6] = width;
	tgaHeader[7] = height;
	tgaHeader[8] = 24; // *** assumed bits per pixel
	fwrite(tgaHeader, sizeof(tgaHeader), 1, out);
	fwrite(pixels, 3*width*height, 1, out);
	fclose(out);
	return true;
}

bool WriteTarga(const char *filename) {
	int width, height;
	GetViewportSize(width, height);
	int npixels = width*height;
	unsigned char *cPixels = new unsigned char[3*npixels], *c = cPixels;
	float *fPixels = new float[3*npixels*sizeof(float)], *f = fPixels;
	glReadPixels(0, 0, width, height, GL_BGR, GL_FLOAT, fPixels);   // Targa is BGR ordered
	for (int i = 0; i < npixels; i++) {
		*c++ = (unsigned char) (255.f*(*f++));
		*c++ = (unsigned char) (255.f*(*f++));
		*c++ = (unsigned char) (255.f*(*f++));
	}
	bool ok = WriteTarga(filename, (unsigned char *) cPixels, width, height);
	delete [] fPixels;
	delete [] cPixels;
	return ok;
}

// Texture

void LoadTexture(unsigned char *pixels, int width, int height, int bpp, GLuint textureUnit, GLuint textureName, bool bgr, bool mipmap) {
	unsigned char *temp = pixels;
	if (false && bpp == 4) {
		int bytesPerImage = 3*width*height;
		temp = new unsigned char[bytesPerImage];
		unsigned char *p = pixels, *t = temp;
		for (int j = 0; j < height; j++)
			for (int i = 0; i < width; i++, p++)
				for (int k = 0; k < 3; k++)
					*t++ = *p++;
	}
	glActiveTexture(GL_TEXTURE0+textureUnit);       // active texture corresponds with textureUnit
	glBindTexture(GL_TEXTURE_2D, textureName);      // bind active texture to textureName
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);          // accommodate width not multiple of 4
	// specify target, format, dimension, transfer data
	if (bpp == 4)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, bgr? GL_BGR : GL_RGB, GL_UNSIGNED_BYTE, temp);
	if (mipmap) {
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			// average bounding pixels of bounding mipmaps - should be default but sometimes needed (else alias)
	}
	else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// if (bpp == 4)
	//	delete [] temp;
}

GLuint LoadTexture(const char *filename, GLuint textureUnit, bool mipmap) {
	int width, height, nChannels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char *data = stbi_load(filename, &width, &height, &nChannels, 0);
	if (!data) {
		printf("LoadTexture: can't open %s (%s)\n", filename, stbi_failure_reason());
		return 0;
	}
	GLuint textureName = 0;
	glGenTextures(1, &textureName);
	LoadTexture(data, width, height, nChannels, textureUnit, textureName, false, mipmap);
	stbi_image_free(data);
	return textureName;
}

GLuint LoadTexture(unsigned char *pixels, int width, int height, int bpp, GLuint textureUnit, bool bgr, bool mipmap) {
	GLuint textureName = 0;
	// allocate GPU texture buffer; copy, free pixels
	glGenTextures(1, &textureName);
	LoadTexture(pixels, width, height, bpp, textureUnit, textureName, bgr, mipmap);
	return textureName;
}

GLuint LoadTargaTexture(const char *targaFilename, GLuint textureUnit, bool mipmap) {
	int width, height, bytesPerPixel;
	unsigned char *pixels = ReadTarga(targaFilename, &width, &height, &bytesPerPixel);
	GLuint textureName = LoadTexture(pixels, width, height, bytesPerPixel, textureUnit, true, mipmap); // Targa is BGR
	delete [] pixels;
	return textureName;
}

// Bump map

unsigned char *GetNormals(unsigned char *depthPixels, int width, int height, float depthIncline) {
	class Helper { public:
		unsigned char *depthPixels, *bumpPixels;
		int width = 0, height = 0;
		float xscale = 1, yscale = 1, depthIncline = 1;
		float GetDepth(int i, int j) {
			unsigned char *v = depthPixels+3*(j*width+i); // depth pixels presumed 3 bytes/pixel, with r==g==b
			return ((float) *v)/255.f;
		}
		float Dz(int i1, int j1, int i2, int j2) {
			return GetDepth(i2, j2)-GetDepth(i1, j1);
		}
		vec3 Normal(int i, int j) {
			int i1 = i > 0? i-1 : i, i2 = i < width-1? i+1 : i;
			int j1 = j > 0? j-1 : j, j2 = j < height-1? j+1 : j;
			vec3 vx((float)(i2-i1)*xscale, 0, depthIncline*Dz(i1, j, i2, j));
			vec3 vy(0, (float)(j2-j1)*yscale, depthIncline*Dz(i, j1, i, j2));
			vec3 v = cross(vx, vy);
			v = normalize(v);
			if (v.z < 0) printf("Normal: v=(%3.2f, %3.2f, %3.2f)!\n", v.x, v.y, v.z);
			return v; // v.z < 0? -v : v;
		}
		Helper(unsigned char *depthPixels, int width, int height, float depthIncline) :
			depthPixels(depthPixels), width(width), height(height), depthIncline(depthIncline) {
				// xscale/yscale assumes quad maps to canonical (0,0) to (1,1)
				xscale = 1/(float)width;
				yscale = 1/(float)height;
				int bytesPerPixel = 3, bytesPerImage = width*height*bytesPerPixel; // returned pixels 3 bytes/pixel
				unsigned char *n = new unsigned char[bytesPerImage];
				bumpPixels = n;
				for (int j = 0; j < height; j++)                  // row
					for (int i = 0; i < width; i++) {             // column
						vec3 v = Normal(i, j);
						*n++ = (unsigned char) (127.5f*(v[0]+1)); // red in [-1,1]
						*n++ = (unsigned char) (127.5f*(v[1]+1)); // grn in [-1,1]
						*n++ = (unsigned char) (255.f*v[2]);      // blu in [0,1]
					}
		}
	} h(depthPixels, width, height, depthIncline);
	return h.bumpPixels;
}
