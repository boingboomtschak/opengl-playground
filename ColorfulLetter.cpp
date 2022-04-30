// 4-ColorfulLetter.cpp: draw multiple triangles to form a colorful letter

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <OpenGL/gl3.h>
#else
#include <glad.h>
#endif
#include <glfw3.h>
#include <stdio.h>
#include "GLXtras.h"
#include <vector>
#include <time.h>

// shader program

GLuint vArray = 0;
GLuint vBuffer = 0; // GPU vertex buffer ID, valid if > 0
GLuint iBuffer = 0;
GLuint program = 0; // GLSL program ID, valid if > 0

const char *vertexShader = R"(
	#version 410 core
	in vec2 point;
	in vec3 color;
	out vec4 vColor;
	uniform float radAng = 0;
	uniform float time = 0;
	vec2 Rotate2D(vec2 v) {
		float c = cos(radAng), s = sin(radAng);
		return vec2(c*v.x-s*v.y, s*v.x+c*v.y);
	}
	void main() {
		vec2 point_r = Rotate2D(point);
		point_r.x *= (sin(time));
		point_r.y *= (sin(time));
		gl_Position = vec4(point_r, 0, 1);
		vColor = vec4(sin(color.x + point.x + time), cos(color.y + point.y + time - 5), (cos(color.x + time)+sin(color.y + time))/2, 1);
	}
)";

const char *pixelShader = R"(
	#version 410 core
	in vec4 vColor;
	out vec4 pColor;
	void main() {
		pColor = vColor;
	}
)";

const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 250;

// the letter T: 8 vertices, 8 colors, triangulation
//float points[][2] = {{-.25f, -.75f}, {-.25f, .3f}, {-.75f, .3f}, {-.75f,  .75f}, { .75f,  .75f}, { .75f, .3f}, { .25f, .3f}, { .25f, -.75f}};
//float colors[][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
//int triangles[][3] = {{0, 7, 1}, {7, 6, 1}, {1, 3, 2}, {1, 4, 3}, {1, 6, 4}, {6, 5, 4}}; // good
//int triangles[][3] = {{0, 7, 1}, {7, 6, 1}, {2, 4, 3}, {2, 5, 4}}; // bad

/* the letter B: 10 vertices, 10 colors, 9 triangles

float points[][2] = {{-.15f, .125f}, {-.5f, -.75f}, {-.5f,   .75f}, {.17f, .75f}, {.38f, .575f},
					 {.38f,   .35f}, {.23f, .125f}, {.5f,  -.125f}, {.5f,  -.5f}, {.25f, -.75f}};

float colors[][3] = {{1, 1, 1}, {1, 0, 0}, {.5f, 0, 0}, {1, 1, 0}, {.5f, 1, 0},
					 {0, 1, 0}, {0, 1, 1}, {0,   0, 1}, {1, 0, 1}, {.5f, 0, .5f}};

int triangles[][3] = {{0, 1, 2}, {0, 2, 3}, {0, 3, 4}, {0, 4, 5},
					  {0, 5, 6}, {0, 6, 7}, {0, 7, 8}, {0, 8, 9}, {0, 9, 1}}; */

int len_points = 64;
float points[][2] = { {200.0f, 50.0f}, {150.0f, 75.0f}, {250.0f, 75.0f}, {250.0f, 50.0f}, {275.0f, 50.0f}, {275.0f, 200.0f}, {250.0f, 200.0f}, {250.0f, 125.0f}, {200.0f, 150.0f}, {150.0f, 125.0f}, {200.0f, 125.0f}, {200.0f, 75.0f}, {225.0f, 100.0f}, {175.0f, 100.0f},
//  D                        0                1                2                3                4                5                  6                 7                8                  9                10                11               12                13         
					  {400.0f, 50.0f}, {370.0f, 65.0f}, {350.0f, 100.0f}, {370.0f, 135.0f}, {400.0f, 150.0f}, {430.0f, 135.0f}, {450.0f, 110.0f}, {450.0f, 65.0f}, {380.0f, 110.0f}, {420.0f, 110.0f}, {400.0f, 130.0f}, {380.0f, 90.0f}, {420.0f, 90.0f}, {400.0f, 70.0f}, {450.0f, 80.0f}, {450.0f, 90.0f},
//  E                        14               15               16                17                18                19                20                21               22                23                24                25               26               27               28               29
					  {590.0f, 50.0f}, {610.0f, 50.0f}, {600.0f, 70.0f}, {570.0f, 150.0f}, {550.0f, 150.0f}, {630.0f, 150.0f}, {650.0f, 150.0f},
//  V                        30               31               32               33                34                35                36
					  {800.0f, 50.0f}, {770.0f, 60.0f}, {750.0f, 100.0f}, {770.0f, 140.0f}, {800.0f, 150.0f}, {830.0f, 140.0f}, {850.0f, 100.0f}, {830.0f, 60.0f}, {800.0f, 70.0f}, {780.0f, 80.0f}, {770.0f, 100.0f}, {780.0f, 120.0f}, {800.0f, 130.0f}, {820.0f, 120.0f}, {830.0f, 100.0f}, {820.0f, 80.0f},
//  O                        37               38               39                40                41                42                43                44               45               46               47                48                49                50                51                52
					  {950.0f, 50.0f}, {950.0f, 150.0f}, {980.0f, 150.0f}, {980.0f, 50.0f}, {980.0f, 130.0f}, {1000.0f, 150.0f}, {1000.0f, 135.0f}, {1020.0f, 130.0f}, {1050.0f, 130.0f}, {1050.0f, 50.0f}, {1020.0f, 50.0f} };
//  N                        53               54                55                56               57                 58                 59                 60                 61                 62                63

int triangles[][3] = { {4, 5, 6}, {4, 6, 3}, {7, 12, 2} , {7, 12, 10} , {7, 10, 8} , {8, 10, 9}, {9, 10, 13}, {9, 13, 1}, {11, 1, 13}, {0, 1, 11}, {11, 0, 2}, {2, 11, 12},
					   {28, 21, 27}, {27, 21, 14}, {27, 14, 15}, {27, 15, 25}, {15, 16, 25}, {25, 22, 23}, {25, 23, 26}, {25, 22, 16}, {16, 22, 17}, {17, 22, 24}, {24, 17, 18}, {18, 24, 19}, {24, 19, 23}, {23, 19, 20}, {20, 29, 23}, {23, 29, 26},
					   {30, 31, 32}, {30, 32, 33}, {33, 34, 30}, {32, 35, 31}, {36, 35, 31},
					   {37, 38, 45}, {45, 38, 46}, {38, 46, 39}, {39, 47, 46}, {39, 48, 47}, {39, 48, 40}, {40, 48, 41}, {41, 49, 48}, {41, 49, 42}, {49, 50, 42}, {42, 50, 51}, {51, 42, 43}, {43, 51, 44}, {51, 44, 52}, {52, 44, 45}, {45, 44, 37},
					   {53, 54, 55}, {55, 53, 56}, {57, 58, 59}, {59, 58, 60}, {58, 61, 60}, {60, 61, 63}, {62, 61, 63} };
float colors[][3] = { {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1} };

time_t startTime = clock();

void InitVertexBuffer() {
	glGenVertexArrays(1, &vArray);
	glBindVertexArray(vArray);
	// create GPU buffer, make it active, allocate memory and copy vertices
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	// allocate and fill vertex buffer
	int vsize = sizeof(points), csize = sizeof(colors);
	glBufferData(GL_ARRAY_BUFFER, vsize+csize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, vsize, points);
	glBufferSubData(GL_ARRAY_BUFFER, vsize, csize, colors);
	// allocate and fill index buffer
	glGenBuffers(1, &iBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(triangles), triangles, GL_STATIC_DRAW);
	int pt_offset = sizeof(points), ntris = sizeof(triangles)/(3*sizeof(int));
	VertexAttribPointer(program, "point", 2, 0, (void *) 0);
	VertexAttribPointer(program, "color", 3, 0, (void *)(size_t)pt_offset);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// display

void Display() {
	float dt = (float)(clock() - startTime) / CLOCKS_PER_SEC; // duration since start
	// clear screen to grey
	glClearColor(.25f, .25f, .25f, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(program);
	PrintGLErrors("Using program");
	SetUniform(program, "radAng", (float)(sin(dt*1.5)*0.1));
	SetUniform(program, "time", (float)dt);
	glBindVertexArray(vArray);
	PrintGLErrors("Binding vertex array");
	int ntris = sizeof(triangles)/(3*sizeof(int));
	glDrawElements(GL_TRIANGLES, 3*ntris, GL_UNSIGNED_INT, 0);
	PrintGLErrors("Draw elements");
	glFlush();
}

// application

void Close() {
	// unbind vertex buffer and free GPU memory
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (vBuffer >= 0)
		glDeleteBuffers(1, &vBuffer);
}

void ErrorGFLW(int id, const char *reason) {
	printf("GFLW error %i: %s\n", id, reason);
}

void Keyboard(GLFWwindow *window, int key, int scancode, int action, int mods) {
	if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main() {
	srand(time(NULL));
	glfwSetErrorCallback(ErrorGFLW);
	if (!glfwInit())
		return 1;
#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Colorful Letter", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
#ifndef __APPLE__
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
#endif
	printf("GL version: %s\n", glGetString(GL_VERSION));
	PrintGLErrors();
	if (!(program = LinkProgramViaCode(&vertexShader, &pixelShader)))
		return 0;

	for (int i = 0; i < len_points; i++) {
		// convert from ((0, screen_width), (0, screen_height)) coordinate space to ((-1, 1), (-1, 1))
		float sw = SCREEN_WIDTH / 2, sh = SCREEN_HEIGHT / 2;
		points[i][0] = (points[i][0] - sw) / sw;
		points[i][1] = (points[i][1] - sh) / sh;
		// randomize colors
		colors[i][0] = (float)rand() / RAND_MAX, colors[i][1] = (float)rand() / RAND_MAX, colors[i][2] = (float)rand() / RAND_MAX;
	}

	InitVertexBuffer();
	glfwSetKeyCallback(window, Keyboard);
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(window)) {
		Display();
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	Close();
	glfwDestroyWindow(window);
	glfwTerminate();
}
