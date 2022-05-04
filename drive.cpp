// drive.cpp : Devon McKee

#define _USE_MATH_DEFINES
#include <glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <stdexcept>
#include "VecMat.h"
#include "GLXtras.h"
#include "GeomUtils.h"
#include "Mesh.h"
#include "Misc.h"

using std::vector;
using std::string;
using std::runtime_error;

GLFWwindow* window;
int win_width = 800, win_height = 800;
GLuint textureUnits = 1;

const char* meshCtrVert = R"(
	#version 410 core
	in vec3 point;
	in vec3 normal;
	in vec2 uv;
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vUv;
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 persp;
	void main() {
		vPoint = (view * model * vec4(point, 1)).xyz;
		vNormal = (view * model * vec4(normal, 0)).xyz;
		vUv = uv;
		gl_Position = persp * vec4(vPoint, 1);
	}
)";

const char* meshCtrFrag = R"(
	#version 410 core
	in vec3 vPoint;
	in vec3 vNormal;
	in vec2 vUv;
	out vec4 pColor;
	uniform vec3 light = vec3(0, 1, 0);
	uniform vec4 color = vec4(1, 1, 1, 1);
	uniform float amb = 0.4;
	uniform float dif = 0.4;
	uniform float spc = 0.7;
	uniform sampler2D txtr;
	void main() {
		vec3 N = normalize(vNormal);
		vec3 L = normalize(light - vPoint);
		vec3 E = normalize(vPoint);
		vec3 R = reflect(L, N);
		float d = dif*max(0, dot(N, L)); 
		float h = max(0, dot(R, E)); 
		float s = spc*pow(h, 100); 
        float intensity = clamp(amb+d+s, 0, 1);
		pColor = vec4(intensity * color.rgb, 1) * texture(txtr, vUv);
	}
)";

struct Camera {
	float fov = 60;
	int width, height;
	vec3 loc, look;
	mat4 view;
	mat4 persp;
	Camera() {
		width = win_width;
		height = win_height;
		loc = vec3(-1.5, 1, 0);
		look = vec3(1, 0, 0);
		view = LookAt(loc, look, vec3(0, 1, 0));
		persp = Perspective(fov, width / height, 0.1f, 10.0f);
	}
	void moveTo(vec3 _loc) {
		loc = _loc;
		view = LookAt(_loc, look, vec3(0, 1, 0));
	}
	void lookAt(vec3 _look) {
		look = _look;
		view = LookAt(loc, _look, vec3(0, 1, 0));
	}
	void adjustFov(float _fov) {
		fov = _fov;
		persp = Perspective(fov, win_width / win_height, 0.1f, 10.0f);
	}
	void resize(int _width, int _height) {
		width = _width;
		height = _height;
		persp = Perspective(fov, width / height, 0.1f, 10.0f);
	}
} camera;

// Simple mesh wrapper struct
struct MeshContainer {
	mat4 model = mat4();
	GLuint program = 0, texUnit = 0, texture = 0;
	GLuint vArray = 0, vBuffer = 0, iBuffer = 0;
	vector<vec3> points;
	vector<vec3> normals;
	vector<vec2> uvs;
	vector<int3> triangles;
	MeshContainer() { }
	MeshContainer(vector<vec3> _points, vector<vec3> _normals, vector<vec2> _uvs, vector<int3> _triangles, string texFilename) {
		points = _points;
		normals = _normals;
		uvs = _uvs;
		triangles = _triangles;
		texUnit = textureUnits++;
		texture = LoadTexture(texFilename.c_str(), texUnit);
		if (texture < 0)
			throw runtime_error("Failed to read texture '" + texFilename + "'!");
		compile();
	}
	MeshContainer(string objFilename, string texFilename, mat4 mdl) {
		if (!ReadAsciiObj(objFilename.c_str(), points, triangles, &normals, &uvs))
			throw runtime_error("Failed to read mesh obj '" + objFilename + "'!");
		Normalize(points, 1.0f);
		texUnit = textureUnits++;
		texture = LoadTexture(texFilename.c_str(), texUnit);
		if (texture < 0)
			throw runtime_error("Failed to read texture '" + texFilename + "'!");
		model = mdl;
		compile();
	}
	void compile() {
		if (!(program = LinkProgramViaCode(&meshCtrVert, &meshCtrFrag)))
			throw runtime_error("Failed to compile mesh container shaders!");
	}
	void allocate() {
		glGenVertexArrays(1, &vArray);
		glBindVertexArray(vArray);
		size_t pSize = points.size() * sizeof(vec3);
		size_t nSize = normals.size() * sizeof(vec3);
		size_t uSize = uvs.size() * sizeof(vec2);
		size_t vBufSize = pSize + nSize + uSize;
		glGenBuffers(1, &vBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
		glBufferData(GL_ARRAY_BUFFER, vBufSize, NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, pSize, points.data());
		glBufferSubData(GL_ARRAY_BUFFER, pSize, nSize, normals.data());
		glBufferSubData(GL_ARRAY_BUFFER, pSize + nSize, uSize, uvs.data());
		size_t tSize = triangles.size() * sizeof(int3);
		glGenBuffers(1, &iBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, tSize, triangles.data(), GL_STATIC_DRAW);
		VertexAttribPointer(program, "point", 3, 0, 0);
		VertexAttribPointer(program, "normal", 3, 0, (GLvoid*)(pSize));
		VertexAttribPointer(program, "uv", 2, 0, (GLvoid*)(pSize + nSize));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	void draw(mat4 transform) {
		glUseProgram(program);
		glBindVertexArray(vArray);
		glActiveTexture(GL_TEXTURE0 + texUnit);
		glBindTexture(GL_TEXTURE_2D, texture);
		mat4 m = model * transform;
		SetUniform(program, "model", m);
		SetUniform(program, "view", camera.view);
		SetUniform(program, "persp", camera.persp);
		SetUniform(program, "txtr", (int)texture);
		glDrawElements(GL_TRIANGLES, triangles.size() * sizeof(int3), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
	void deallocate() {
		glDeleteVertexArrays(1, &vArray);
		glDeleteBuffers(1, &vBuffer);
		glDeleteBuffers(1, &iBuffer);
		glDeleteTextures(1, &texture);
	}
};

struct Car {
	MeshContainer mesh;
	vec3 pos; // position
	vec3 vel; // velocity
	vec3 acc; // acceleration
	Car() { };
	Car(MeshContainer _mesh) {
		mesh = _mesh;
		pos = vec3(0, 0, 0);
		vel = vec3(1, 0, 0);
		acc = vec3(0, 0, 0);
	}
	void draw() { 
		//mesh.draw(Translate(pos) * Orientation(vel, vec3(0, 1, 0))); 
		mesh.draw(Translate(pos));
	}
	void update() { }
	void accel() { }
} car;

void WindowResized(GLFWwindow* window, int width, int height) {
	camera.resize(width, height);
	glViewport(0, 0, win_width, win_height);
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	else if (key == GLFW_KEY_W && action == GLFW_PRESS)
		car.pos += vec3(1, 0, 0);
	else if (key == GLFW_KEY_S && action == GLFW_PRESS)
		car.pos -= vec3(1, 0, 0);
	else if (key == GLFW_KEY_A && action == GLFW_PRESS)
		car.pos -= vec3(0, 0, 1);
	else if (key == GLFW_KEY_D && action == GLFW_PRESS)
		car.pos += vec3(0, 0, 1);
	else if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		car.pos -= vec3(0, 1, 0);
	else if (key == GLFW_KEY_E && action == GLFW_PRESS)
		car.pos += vec3(0, 1, 0);
}

void setup() {
	// Initialize GLFW callbacks
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, WindowResized);
	glfwSwapInterval(1);
	// Setup meshes
	MeshContainer car_mesh = MeshContainer("./objects/car.obj", "./textures/car.tga", Scale(0.5f));
	car_mesh.allocate();
	car = Car(car_mesh);

	// Set some GL drawing context settings
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
}

void cleanup() {
	// Cleanup meshes
	car.mesh.deallocate();
}

void draw() {
	glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	car.draw();
	glFlush();
}

int main() {
	srand((int)time(NULL));
	if (!glfwInit())
		return 1;
	glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	window = glfwCreateWindow(win_width, win_height, "Drive", NULL, NULL);
	if (!window)
		throw runtime_error("Failed to create GLFW window!");
	glfwMakeContextCurrent(window);
	glfwSetWindowPos(window, 100, 100);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	setup();
	while (!glfwWindowShouldClose(window)) {
		draw();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}