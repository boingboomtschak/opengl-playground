// 2-ClearScreen.cpp - use OpenGL shader architecture

#include <glad.h>
#define GLFW_INCLUDE_NONE
#include <glfw3.h>											// GL toolkit
#include <stdio.h>											// printf, etc.
#include "GLXtras.h"										// convenience routines

GLuint vBuffer = 0;											// GPU vert buf ID, valid if > 0
GLuint vArray = 0;
GLuint program = 0;											// shader prog ID, valid if > 0

// vertex shader: operations before the rasterizer
const char *vertexShader = R"(
	#version 410 core
	in vec2 point;											// 2D point from GPU memory
	void main() {
		// REQUIREMENT 1A) transform vertex:
		gl_Position = vec4(point, 0, 1);					// 'built-in' variable
	}
)";

// pixel shader: operations after the rasterizer
const char *pixelShader = R"(
	#version 410 core
	out vec4 pColor;
	void main() {
		if (mod(floor(gl_FragCoord.x / 50), 2) == 0) {
			pColor = vec4(0, 0, 0, 0);
			if (mod(floor(gl_FragCoord.y / 50), 2) == 0) {
				pColor = vec4(1, 1, 1, 1);
			}
		} else {
			pColor = vec4(1, 1, 1, 1);
			if (mod(floor(gl_FragCoord.y / 50), 2) == 0) {
				pColor = vec4(0, 0, 0, 1);
			}
		}
	}
)";

void InitVertexBuffer() {
	// REQUIREMENT 3A) create GPU buffer, copy 4 vertices
	float pts[][2] = {{-1,-1},{-1,1},{1,1},{-1,-1},{1,1},{1,-1}};
	glGenVertexArrays(1, &vArray);
	glBindVertexArray(vArray);
	glGenBuffers(1, &vBuffer);								// ID for GPU buffer
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);					// make it active
	glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_STATIC_DRAW);
	// REQUIREMENT 3B) set vertex feeder
	GLint id = glGetAttribLocation(program, "point");
	glEnableVertexAttribArray(id);
	glVertexAttribPointer(id, 2, GL_FLOAT, GL_FALSE, 0, (void *) 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Display() {
	glUseProgram(program);									// ensure correct program
	glBindVertexArray(vArray);
	// in subsequent code the above three lines will be replaced with
	// VertexAttribPointer(program, "point", 2, 0, (void *) 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	PrintGLErrors("Draw arrays");
	glFlush();												// flush GL ops
}

void GlfwError(int id, const char *reason) {
	printf("GFLW error %i: %s\n", id, reason);
	getchar();
}

void APIENTRY GlslError(GLenum source, GLenum type, GLuint id, GLenum severity,
						GLsizei len, const GLchar *msg, const void *data) {
	printf("GLSL Error: %s\n", msg);
	getchar();
}

int AppError(const char *msg) {
	glfwTerminate();
	printf("Error: %s\n", msg);
	getchar();
	return 1;
}

int main() {												// application entry
	glfwSetErrorCallback(GlfwError);						// init GL framework
	if (!glfwInit())
		return 1;
	// create named window of given size
	#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#endif
	GLFWwindow *w = glfwCreateWindow(400, 400, "Chessboard", NULL, NULL);
	if (!w)
		return AppError("can't open window");
	glfwMakeContextCurrent(w);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);	// set OpenGL extensions
	// following line will not compile if glad.h < OpenGLv4.3
	//glDebugMessageCallback(GlslError, NULL);
	// REQUIREMENT 2) build shader program
	if (!(program = LinkProgramViaCode(&vertexShader, &pixelShader)))
		return AppError("can't link shader program");
	InitVertexBuffer();										// set GPU vertex memory
	while (!glfwWindowShouldClose(w)) {						// event loop
		Display();
		if (PrintGLErrors())								// test for runtime GL error
			getchar();										// if so, pause
		glfwSwapBuffers(w);									// double-buffer is default
		glfwPollEvents();
	}
	glfwDestroyWindow(w);
	glfwTerminate();
}
