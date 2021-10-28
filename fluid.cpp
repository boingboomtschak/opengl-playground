// fluid.cpp - Devon McKee

#include <glad.h>
#include <GLFW/glfw3.h>
#include "GLXtras.h"
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"

GLuint vBuffer = 0;
GLuint program = 0;

int win_width = 800;
int win_height = 800;

Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

float cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} };
int cube_faces[][4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {4, 5, 6, 7}, {6, 7, 0, 1}, {0, 3, 4, 7}, {1, 2, 5, 6} };

const int GRID_NUM = 4;

enum CELL { AIR, BLOCK, WATER };

struct Cell {
	CELL type = AIR;
	float level = 0;
};

struct Grid {
	std::vector<std::vector<std::vector<Cell>>> grid;
	Grid() {
		// Initialize grid
		for (int i = 0; i < GRID_NUM; i++) {
			grid.push_back(std::vector<std::vector<Cell>>());
			for (int j = 0; j < GRID_NUM; j++) {
				grid[i].push_back(std::vector<Cell>());
				for (int k = 0; k < GRID_NUM; k++) {
					grid[i][j].push_back(Cell());
					grid[i][j][k].type = BLOCK;
				}
			}
		}
	}
	void DownwardFlow() {

	}
	void SidewaysFlow() {

	}
	void UpwardsFlow() {

	}
	void Simulate() {

	}
	void Render() {
		SetUniform(program, "persp", camera.persp);
		for (int i = 0; i < GRID_NUM; i++) {
			for (int j = 0; j < GRID_NUM; j++) {
				for (int k = 0; k < GRID_NUM; k++) {
					float interval = 2.0 / GRID_NUM;
					mat4 modelview = camera.modelview * Translate(interval * i - 1.0, interval * j - 1.0, interval * k - 1.0) * Scale(vec3(1 / GRID_NUM));
					SetUniform(program, "modelview", modelview);
					switch (grid[i][j][k].type) {
						case BLOCK:
							SetUniform(program, "color", vec4(0, 0, 0, 1));
							break;
						case WATER:
							SetUniform(program, "color", vec4(0, 0, 1, 1));
							break;
					}
					if (grid[i][j][k].type != AIR) {
						for (int f = 0; f < 6; f++) {
							glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[f]);
						}
					}
				}
			}
		}
	}
};

Grid grid;

const char* vertexShader = R"(
	#version 130
	in vec3 point;
	uniform mat4 persp;
	uniform mat4 modelview;
	void main() {
		gl_Position = persp * modelview * vec4(point, 1);
	}
)";

const char* fragmentShader = R"(
	#version 130
	uniform vec4 color;
	out vec4 pColor;
	void main() {
		pColor = color;
	}
)";

void InitVertexBuffer() {
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_points), cube_points, GL_STATIC_DRAW);
}

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

void Display() {
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glClearColor(0.6, 0.6, 0.6, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	// Draw enclosing cube
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modelview", camera.modelview);
	SetUniform(program, "color", vec4(1, 1, 1, 1));
	for (int f = 0; f < 6; f++) {
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[f]);
	}
	grid.Simulate();
	grid.Render();
	glFlush();
}

int main() {
	srand(time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "FluidSim", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(window, 100, 100);
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	if (!(program = LinkProgramViaCode(&vertexShader, &fragmentShader)))
		return 0;
	InitVertexBuffer();
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
	glDeleteBuffers(1, &vBuffer);
	glfwDestroyWindow(window);
	glfwTerminate();
}