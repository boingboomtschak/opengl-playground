// textureCube.cpp : Devon McKee

#include <glad.h>
#include <GLFW/glfw3.h>
#include "GLXtras.h"
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"
#include "Mesh.h"
#include "Misc.h"

GLuint vBuffer = 0;
GLuint program = 0;
GLuint texUnit = 0;
GLuint texName;

const char* texture = "textures/grass.tga";

int win_width = 800;
int win_height = 800;

Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

vec3 lightSource = vec3(1, 1, 0);

float cube_points[][3] = {
	{-1, -1, 1},
    {1, -1, 1},
    {1, 1, 1},
    {-1, 1, 1}, // front (0, 1, 2, 3)
	{-1, -1, -1},
    {1, -1, -1},
    {1, 1, -1},
    {-1, 1, -1}, // back (4, 5, 6, 7)
	{-1, -1, 1},
    {-1, -1, -1},
    {-1, 1, -1},
    {-1, 1, 1}, // left (8, 9, 10, 11)
	{1, -1, 1},
    {1, -1, -1},
    {1, 1, -1},
    {1, 1, 1}, // right (12, 13, 14, 15)
	{-1 , 1, 1},
    {1, 1, 1},
    {1, 1, -1},
    {-1, 1, -1}, // top  (16, 17, 18, 19)
	{-1, -1, 1},
    {1, -1, 1},
    {1, -1, -1},
    {-1, -1, -1}  // bottom (20, 21, 22, 23)
};
float cube_normals[][3] = {
	{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, // front
	{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, // back
	{-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, // left
	{1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, // right
	{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, // top
	{0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}  // bottom
};
float cube_uvs[][2] = {
	{0.25, 0}, {0.5, 0}, {0.5, 0.33333}, {0.25, 0.33333},
	{0.25, 1}, {0.5, 1}, {0.5, 0.66666}, {0.25, 0.66666},
	{0, 0.33333}, {0, 0.66666}, {0.25, 0.66666}, {0.25, 0.33333},
	{0.75, 0.33333}, {0.75, 0.66666}, {0.5, 0.66666}, {0.5, 0.33333},
	{0.25, 0.33333}, {0.5, 0.33333}, {0.5, 0.666}, {0.25, 0.66666},
	{1, 0.33333}, {0.75, 0.33333}, {0.75, 0.66666}, {1, 0.66666}
};
int cube_triangles[][3] = {
	{0, 1, 2}, {2, 3, 0}, // front
	{4, 5, 6}, {6, 7, 4}, // back
	{8, 9, 10}, {10, 11, 8}, // left
	{12, 13, 14}, {14, 15, 12}, // right
	{16, 17, 18}, {18, 19, 16}, // top
	{20, 21, 22}, {22, 23, 20}  // bottom
};

const char* vertexShader = R"(
	#version 130
	in vec3 point;
	in vec3 normal;
	in vec2 uv;
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vuv;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		vPoint = (modelview * vec4(point, 1)).xyz;
		vNormal = (modelview * vec4(normal, 0)).xyz;
		gl_Position = persp * vec4(vPoint, 1);
		vuv = uv;
	}
)";

const char* fragmentShader = R"(
	#version 130
	in vec3 vPoint;
	in vec3 vNormal;
	in vec2 vuv;
	out vec4 pColor;
	uniform vec3 light = vec3(-.2, .1, -3);
	uniform vec4 color = vec4(1, 1, 1, 1);
	uniform float amb = 0.4; // ambient intensity
	uniform float dif = 0.4; // diffusivity
	uniform float spc = 0.7; // specularity
	uniform sampler2D texImage;
	void main() {
        vec3 N = normalize(vNormal);       // surface normal
        vec3 L = normalize(light-vPoint);  // light vector
        vec3 E = normalize(vPoint);        // eye vertex
        vec3 R = reflect(L, N);            // highlight vector
		float d = dif*max(0, dot(N, L)); // one-sided Lambert
		float h = max(0, dot(R, E)); // highlight term
		float s = spc*pow(h, 100); // specular term
        float intensity = clamp(amb+d+s, 0, 1);
        pColor = vec4(intensity*color.rgb, 1) * texture(texImage, vuv);
	}
)";

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

void InitVertexBuffer() {
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	size_t size = sizeof(cube_points) + sizeof(cube_normals) + sizeof(cube_uvs);
	glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube_points), cube_points);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(cube_points), sizeof(cube_normals), cube_normals);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(cube_points) + sizeof(cube_normals), sizeof(cube_uvs), cube_uvs);
}

void Display() {
	glUseProgram(program);
	glClearColor(0.4f, 0.4f, 0.4f, 1.);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modelview", camera.modelview);
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	VertexAttribPointer(program, "normal", 3, 0, (void*)sizeof(cube_points));
	VertexAttribPointer(program, "uv", 2, 0, (void*)(sizeof(cube_points) + sizeof(cube_normals)));
	glActiveTexture(GL_TEXTURE0 + texUnit);
	glBindTexture(GL_TEXTURE_2D, texName);
	SetUniform(program, "texImage", (int)texUnit);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, cube_triangles);
	glFlush();
}

int main() {
	srand((int)time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "textureCube", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(window, 100, 100);
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	if (!(program = LinkProgramViaCode(&vertexShader, &fragmentShader)))
		return 1;
	InitVertexBuffer();
	glfwWindowHint(GLFW_SAMPLES, 4);
	texName = LoadTexture(texture, texUnit);
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
	glDeleteBuffers(1, &texName);
	glfwDestroyWindow(window);
	glfwTerminate();
}
