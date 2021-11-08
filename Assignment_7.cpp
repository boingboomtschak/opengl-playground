// Assignment_7.cpp
// (c) Devon McKee 2021

#include <glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include "Camera.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Misc.h"
#include "Widgets.h"
#include "VecMat.h"

// display parameters
int         winWidth = 800, winHeight = 600;
Camera      camera(winWidth, winHeight, vec3(0, 0, 0), vec3(0, 0, -7));

// shading
GLuint      program = 0;
int			textureName = 0, textureUnit = 0;
const char *textureFilename = "textures/skyrim.tga";

// interaction
vec3        light(1.5f, 1.5f, 1);
void       *picked = NULL;
Mover       mover;
time_t startTime = clock();

// vertex shader (no op)
const char *vShaderCode = R"(
	#version 130
	void main() {
		gl_Position = vec4(0); // no-op
	}
)";

// tessellation evaluation
const char *teShaderCode = R"(
	#version 400
	layout (quads, equal_spacing, ccw) in;
	uniform mat4 modelview, persp;
	uniform sampler2D textureMap;
	out vec3 point, normal;
	out vec2 uv;
	float heightFactor = 0.15;
	float PI = 3.141592;
	vec3 PtFromSphere(float u, float v) {
		// u is longitude
		// v is latitude (PI/2 = N. pole, 0 = equator, -PI/2 = S. pole)
		float elevation = PI*v-PI/2;
		float eFactor = cos(elevation);
		float y = sin(elevation);
		float angle = 2*PI*(1-u);
		float x = eFactor*cos(angle), z = eFactor*sin(angle);
		return vec3(x, y, z);
	}
	vec3 PtFromCyl(float u, float v) {
		float elevation = PI*v-PI/2;
		float eFactor = cos(elevation);
		float y = sin(elevation);
		float angle = 2*PI*(1-u);
		float x = cos(angle), z = sin(angle);
		return vec3(x, y, z);
	}
	void main() {
		uv = gl_TessCoord.st;
		//vec3 p = PtFromCyl(uv.s, uv.t);
		vec3 p = vec3(uv.s, heightFactor*(texture(textureMap, uv).r), uv.t);
		vec3 n = p;
		point = (modelview*vec4(p, 1)).xyz;
		normal = (modelview*vec4(n, 0)).xyz;
		gl_Position = persp*vec4(point, 1);
	}
)";

// pixel shader
const char *pShaderCode = R"(
	#version 130
	in vec3 point, normal;
	in vec2 uv;
	uniform sampler2D textureMap;
	uniform vec3 light;
	void main() {
		vec3 N = normalize(normal);             // surface normal
		vec3 L = normalize(light-point);        // light vector
		vec3 E = normalize(point);              // eye vertex
		vec3 R = reflect(L, N);                 // highlight vector
		float dif = max(0, dot(N, L));          // one-sided diffuse
		float spec = pow(max(0, dot(E, R)), 50);
		float ad = clamp(.15+dif, 0, 1);
		vec3 texColor = texture(textureMap, uv).rgb;
		gl_FragColor = vec4(ad*texColor+vec3(spec), 1);
	}
)";

// display

void Display() {
	// background, blending, zbuffer
	glClearColor(.6f, .6f, .6f, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);
	// update matrices
	SetUniform(program, "modelview", camera.modelview);
	SetUniform(program, "persp", camera.persp);
	//float dt = (float)(clock() - startTime) / CLOCKS_PER_SEC; // duration since start
	//SetUniform(program, "time", (float)dt);
	// transform light and send to pixel shader
	vec4 hLight = camera.modelview*vec4(light, 1);
	glUniform3fv(glGetUniformLocation(program, "light"), 1, (float *) &hLight);
	// set texture
	SetUniform(program, "textureMap", textureUnit);
	glActiveTexture(GL_TEXTURE0+textureUnit);       // active texture corresponds with textureUnit
	glBindTexture(GL_TEXTURE_2D, textureName);      // bind active texture to textureName
	// tessellate patch
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	float res = 100, outerLevels[] = {res, res, res, res}, innerLevels[] = {res, res};
	glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerLevels);
	glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerLevels);
	glDrawArrays(GL_PATCHES, 0, 4);
	// light
	glDisable(GL_DEPTH_TEST);
	UseDrawShader(camera.fullview);
	Disk(light, 12, vec3(1, 0, 0));
	glFlush();
}

// mouse

void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
	double x, y;
	glfwGetCursorPos(w, &x, &y);
	y = winHeight-y; // invert y for upward-increasing screen space
	picked = NULL;
	if (action == GLFW_RELEASE)
		camera.MouseUp();
	if (action == GLFW_PRESS) {
		if (MouseOver(x, y, light, camera.fullview)) {
			mover.Down(&light, (int) x, (int) y, camera.modelview, camera.persp);
			picked = &mover;
		}
		else {
			picked = &camera;
			camera.MouseDown(x, y);
		}
	}
}

void MouseMove(GLFWwindow *w, double x, double y) {
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		y = winHeight-y;
		if (picked == &mover)
			mover.Drag((int) x, (int) y, camera.modelview, camera.persp);
		if (picked == &camera)
			camera.MouseDrag((int) x, (int) y, Shift());
	}
}

void MouseWheel(GLFWwindow *w, double ignore, double spin) {
	camera.MouseWheel(spin > 0, Shift());
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

// application

void Resize(GLFWwindow *window, int width, int height) {
	camera.Resize(winWidth = width, winHeight = height);
	glViewport(0, 0, width, height);
}

int main(int ac, char **av) {
	// init app window
	if (!glfwInit())
		return 1;
	GLFWwindow *w = glfwCreateWindow(winWidth, winHeight, "Tess", NULL, NULL);
	glfwSetWindowPos(w, 100, 100);
	glfwMakeContextCurrent(w);
	// init OpenGL, shader program, texture
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	program = LinkProgramViaCode(&vShaderCode, NULL, &teShaderCode, NULL, &pShaderCode);
	textureName = LoadTexture(textureFilename, textureUnit);
	// callbacks
	glfwSetCursorPosCallback(w, MouseMove);
	glfwSetMouseButtonCallback(w, MouseButton);
	glfwSetScrollCallback(w, MouseWheel);
	glfwSetKeyCallback(w, Keyboard);
	glfwSetWindowSizeCallback(w, Resize);
	// event loop
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(w)) {
		Display();
		glfwPollEvents();
		glfwSwapBuffers(w);
	}
	glfwDestroyWindow(w);
	glfwTerminate();
}
