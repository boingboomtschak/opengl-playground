// 2-VersionGL.cpp: determine GL and GLSL versions

#include "glad.h"
#include <glfw3.h>
#include <stdio.h>

int main() {
	if (!glfwInit()) return 1;
	GLFWwindow *w = glfwCreateWindow(1, 1, "", NULL, NULL);
	// need window to create GL context
	glfwSetWindowPos(w, 0, 0);
	glfwMakeContextCurrent(w);
	// must load OpenGL runtime subroutine pointers
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	printf("GL vendor: %s\n", glGetString(GL_VENDOR));
	printf("GL renderer: %s\n", glGetString(GL_RENDERER));
	printf("GL version: %s\n", glGetString(GL_VERSION));
	printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	glfwDestroyWindow(w);
	glfwTerminate();
	return 0;
}
