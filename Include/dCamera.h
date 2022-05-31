// dCamera.h

#ifndef DCAMERA_HDR
#define DCAMERA_HDR

#include "VecMat.h"

struct Camera {
	int width, height;
	float fov;
	float zNear;
	float zFar;
	vec3 loc = vec3(0, 0, 0);
	vec3 look = vec3(1, 0, 0);
    vec3 up = vec3(0, 1, 0);
	mat4 view;
	mat4 persp;
	Camera(int _width, int _height, float _fov, float _zNear, float _zFar) : width(_width), height(_height), fov(_fov), zNear(_zNear), zFar(_zFar) {
		view = LookAt(loc, look, up);
		persp = Perspective(fov, (float)width / (float)height, zNear, zFar);
	}
	void update() {
		view = LookAt(loc, look, up);
		persp = Perspective(fov, (float)width / (float)height, zNear, zFar);
	}
};

#endif