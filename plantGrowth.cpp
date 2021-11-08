// plantGrowth.cpp : Devon McKee

#define _USE_MATH_DEFINES

#include <glad.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"
#include "GLXtras.h"
#include "GeomUtils.h"

using std::vector;
using std::min;
using std::max;

GLuint cubeBuffer = 0;
GLuint plantProgram = 1, cubeProgram = 2;

int win_width = 800, win_height = 800, res = 25;
Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

float cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} }; int cube_faces[][4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {4, 5, 6, 7}, {6, 7, 0, 1}, {0, 3, 4, 7}, {1, 2, 5, 6} };
vec3 ctrlPts[] = {
	vec3(0.2000f,  0.0000f, 2.70000f), vec3(0.2000f, -0.1120f, 2.70000f),
	vec3(0.1120f, -0.2000f, 2.70000f), vec3(0.0000f, -0.2000f, 2.70000f),
	vec3(1.3375f,  0.0000f, 2.53125f), vec3(1.3375f, -0.7490f, 2.53125f),
	vec3(0.7490f, -1.3375f, 2.53125f), vec3(0.0000f, -1.3375f, 2.53125f),
	vec3(1.4375f,  0.0000f, 2.53125f), vec3(1.4375f, -0.8050f, 2.53125f),
	vec3(0.8050f, -1.4375f, 2.53125f), vec3(0.0000f, -1.4375f, 2.53125f),
	vec3(1.5000f,  0.0000f, 2.40000f), vec3(1.5000f, -0.8400f, 2.40000f),
	vec3(0.8400f, -1.5000f, 2.40000f), vec3(0.0000f, -1.5000f, 2.40000f)
};

const float GROWTH_INTERVAL = 0.05f;
const float STALK_WIDTH = 0.05f;
const int SAMPLE_NUM = 20;

const char* plantVertShader = "void main() { gl_Position = vec4(0); }";

const char* plantTessEvalShader = R"(
	#version 400 core
	layout (quads, equal_spacing, ccw) in;\
	uniform vec3 ctrlPts[16];
	uniform mat4 modelview;
	uniform mat4 persp;
	out vec3 point;
	out vec3 normal;
	out vec2 uv;
	vec3 BezTangent(float t, vec3 b1, vec3 b2, vec3 b3, vec3 b4) {
        float t2 = t*t;
        return (-3*t2+6*t-3)*b1+(9*t2-12*t+3)*b2+(6*t-9*t2)*b3+3*t2*b4;
    }
    vec3 BezPoint(float t, vec3 b1, vec3 b2, vec3 b3, vec3 b4) {
        float t2 = t*t, t3 = t*t2;
        return (-t3+3*t2-3*t+1)*b1+(3*t3-6*t2+3*t)*b2+(3*t2-3*t3)*b3+t3*b4;
    }
	void main() {
		vec3 spts[4], tpts[4];
		uv = gl_TessCoord.st;
		for (int i = 0; i < 4; i++) {
			spts[i] = BezPoint(uv.s, ctrlPts[4*i], ctrlPts[4*i+1], ctrlPts[4*i+2], ctrlPts[4*i+3]);
			tpts[i] = BezPoint(uv.t, ctrlPts[i], ctrlPts[i+4], ctrlPts[i+8], ctrlPts[i+12]);
		}
		vec3 p = BezPoint(uv.t, spts[0], spts[1], spts[2], spts[3]);
		vec3 tTan = BezTangent(uv.t, spts[0], spts[1], spts[2], spts[3]);
		vec3 sTan = BezTangent(uv.s, tpts[0], tpts[1], tpts[2], tpts[3]);
        vec3 n = normalize(cross(sTan, tTan));
		normal = (modelview*vec4(n, 0)).xyz;
        point = (modelview*vec4(p, 1)).xyz;
        gl_Position = persp*vec4(point, 1);
	}
)";

const char* plantFragShader = R"(
	#version 130
	in vec3 point;
	in vec3 normal;
	in vec2 uv;
	out vec4 pColor;
	uniform vec3 light = vec3(1, 1, 0);
	void main() {
		vec3 N = normalize(normal);
		vec3 L = normalize(light - point);
		vec3 E = normalize(point);
		vec3 R = reflect(L, N);
		float d = abs(dot(N, L));
		float s = abs(dot(R, E));
		float intensity = clamp(d+pow(s, 50), 0, 1);
		vec3 color = vec3(1);
		pColor = vec4(intensity*color, 1);
	}
)";

const char* cubeVertShader = R"(
	#version 130
	in vec3 point;
	uniform mat4 modelview;
	uniform mat4 persp;
	void Main() {
		gl_Position = persp * modelview * vec4(point, 1);
	}
)";

const char* cubeFragShader = R"(
	#version 130
	out vec4 pColor;
	void main() {
		pColor = vec4(1);
	}
)";

// --- Callbacks ---

void Resize(GLFWwindow* window, int width, int height) {
	camera.Resize(win_width = width, win_height = height);
	glViewport(0, 0, win_width, win_height);
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

bool Shift(GLFWwindow* w) {
	return glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
		glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}

void MouseButton(GLFWwindow* w, int butn, int action, int mods) {
	double x, y;
	glfwGetCursorPos(w, &x, &y);
	y = win_height - y;
	if (action == GLFW_PRESS)
		camera.MouseDown((int)x, (int)y);
	if (action == GLFW_RELEASE)
		camera.MouseUp();
}

void MouseMove(GLFWwindow* w, double x, double y) {
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) { // drag
		y = win_height - y;
		camera.MouseDrag((int)x, (int)y, Shift(w));
	}
}

void MouseWheel(GLFWwindow* w, double ignore, double spin) {
	camera.MouseWheel(spin > 0, Shift(w));
}

// --- Main functions ---

void InitVertexBuffer() {
	// Initializing buffer for cube
	glGenBuffers(1, &cubeBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_points), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube_points), cube_points);
}

void InitPlant() {
	// Generate two rings
	vector<vec3> r1 = SampleCircle(vec3(0.0f), SAMPLE_NUM, STALK_WIDTH);
	r1 = QuickHull(r1);
	vector<vec3> r2 = SampleCircle(vec3(0.0f, GROWTH_INTERVAL, 0.0f), SAMPLE_NUM, STALK_WIDTH);
	r2 = QuickHull(r2);
	// Generate triangles between rings

}

void Display() {
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	// Draw cube
	glUseProgram(cubeProgram);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	VertexAttribPointer(cubeProgram, "point", 3, 0, (void*)0);
	SetUniform(cubeProgram, "persp", camera.persp);
	SetUniform(cubeProgram, "modelview", camera.modelview);
	for (int i = 0; i < 6; i++) {
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[i]);
	}
	// Draw plant
	glUseProgram(plantProgram);
	SetUniform(plantProgram, "modelview", camera.modelview);
	SetUniform(plantProgram, "persp", camera.persp);
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	float r = (float)res;
	float outerLevels[] = { r, r, r, r }, innerLevels[] = { r, r };
	glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerLevels);
	glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerLevels);
	SetUniform3v(plantProgram, "ctrlPts", 16, (float*)ctrlPts);
	glDrawArrays(GL_PATCHES, 0, 4);
	glFlush();
}

int main() {
	srand((int)time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "plantGrowth", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(window, 100, 100);
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	if (!(plantProgram = LinkProgramViaCode(&plantVertShader, NULL, &plantTessEvalShader, NULL, &plantFragShader)))
		return 1;
	if (!(cubeProgram = LinkProgramViaCode(&cubeVertShader, &cubeFragShader)))
		return 1;
	InitVertexBuffer();
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwSetCursorPosCallback(window, MouseMove);
	glfwSetMouseButtonCallback(window, MouseButton);
	glfwSetScrollCallback(window, MouseWheel);
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, Resize);
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(window)) {
		Display();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &cubeBuffer);
	glfwDestroyWindow(window);
	glfwTerminate();
}