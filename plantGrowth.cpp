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
GLuint program = 0;

int win_width = 800, win_height = 800;
Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

float cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} }; int cube_faces[][4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {4, 5, 6, 7}, {6, 7, 0, 1}, {0, 3, 4, 7}, {1, 2, 5, 6} };
vector<vec3> plant_points;

const float GROWTH_INTERVAL = 0.05f;
const float STALK_WIDTH = 0.05f;
const int SAMPLE_NUM = 20;



const char* vertShader = R"(
	#version 130
	in vec3 point;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		gl_Position = persp * modelview * vec4(point, 1);
	}
)";

const char* fragShader = R"(
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
	glClear(GL_DEPTH_BUFFER_BIT);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	// Draw cube
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modelview", camera.modelview);
	for (int i = 0; i < 6; i++) {
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[i]);
	}
	// Draw plant

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
	if (!(program = LinkProgramViaCode(&vertShader, &fragShader)))
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