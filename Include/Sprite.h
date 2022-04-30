// Sprite.h - 2D quad with optional texture or animation

#ifndef SPRITE_HDR
#define SPRITE_HDR

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <glad.h>
#endif
#include <time.h>
#include <vector>
#include "VecMat.h"

using std::string;

// Sprite Class

class Sprite {
public:
	vec2 position, scale = vec2(1, 1), mouseDown, oldMouse;
	float rotation = 0;
	int winWidth = 0, winHeight = 0;
	GLuint textureUnit = 0, matUnit = 0;
	// for animation:
	GLuint frame = 0, nFrames = 0;
	std::vector<GLuint> textureNames;
	float frameDuration = .25f;
	time_t change;
	GLuint textureName = 0, matName = 0;
	mat4 ptTransform, uvTransform;
	bool Intersect(Sprite &s);
	void UpdateTransform();
	void Initialize(const char *imageFile, const char *matFile = NULL);
	void Initialize(const char *imageFile, const char *matFile, int textureUnit);
	void Initialize(string imageFile, string matFile = "");
	void Initialize(std::vector<string> &imageFiles, const char *matFile = NULL);
	void Initialize(std::vector<string> &imageFiles, string matFile = "");
	void SetPosition(vec2 p);
	vec2 GetPosition();
	void MouseDown(vec2 mouse);
	vec2 MouseDrag(vec2 mouse);
	void MouseWheel(double spin);
	vec2 GetScale();
	void SetScale(vec2 s);
	mat4 GetPtTransform();
	void SetPtTransform(mat4 m);
	void SetUvTransform(mat4 m);
	void Display(mat4 *view = NULL);
	void Release();
	void SetFrameDuration(float dt); // if animating
	Sprite(vec2 p = vec2(), float s = 1) : position(p), scale(vec2(s, s)) { UpdateTransform(); }
	Sprite(vec2 p, vec2 s) : position(p), scale(s) { UpdateTransform(); }
};

#endif
