// boids-3d.cpp : Devon McKee

#define _USE_MATH_DEFINES

#include <glad.h>
#include <GLFW/glfw3.h>
#include "GLXtras.h"
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"
#include "Mesh.h"

GLuint vBuffer = 0;
GLuint program = 0;

const char* objFilename = "objects/penguin.obj";

int win_width = 800;
int win_height = 800;

Camera camera((float) win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

std::vector<vec3> obj_points;
std::vector<int3> obj_triangles;

//                               0           1           2             3            4           5           6           7
float cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} };
int cube_faces[][4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {4, 5, 6, 7}, {6, 7, 0, 1}, {0, 3, 4, 7}, {1, 2, 5, 6} };

float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX / (max - min)); }
vec3 rand_vec3(float min = -1, float max = 1) { return vec3(min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min))); }
float dist(vec3 p1, vec3 p2) { return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2) + pow(p2.z - p1.z, 2)); }

const float BOID_SPEED = 0.005f;
const float BOID_SIZE = 0.05f;
const float BOID_PERCEPTION = 0.2f;
const int STARTING_BOIDS = 75;
const bool AVOID_WALLS = true;

const float ALIGNMENT_WEIGHT = 1.0f;
const float COHESION_WEIGHT = 1.0f;
const float SEPARATION_WEIGHT = 1.5f;

struct Boid {
	vec3 p, v; // position and velocity vector
	Boid(vec3 xy, vec3 nv) {
		p = xy, v = nv;
	}
	vec3 Alignment(std::vector<Boid>* flock) {
		vec3 cv = vec3(0.0f);
		int nc = 0;
		for (int i = 0; i < flock->size(); i++) {
			if (&(*flock)[i] != this && dist(p, (*flock)[i].p) < BOID_PERCEPTION) {
				cv += (*flock)[i].v;
				nc++;
			}
		}
		if (nc > 0) {
			cv /= (float)nc;
			cv = normalize(cv);
			return cv;
		} else {
			return vec3(0.0f);
		}
	}
	vec3 Cohesion(std::vector<Boid>* flock) {
		vec3 cv = vec3(0.0f);
		int nc = 0;
		for (int i = 0; i < flock->size(); i++) {
			if (&(*flock)[i] != this && dist(p, (*flock)[i].p) < BOID_PERCEPTION) {
				cv += (*flock)[i].p;
				nc++;
			}
		}
		if (nc > 0) {
			cv /= (float)nc;
			cv -= p;
			cv = normalize(cv);
			return cv;
		} else {
			return vec3(0.0f);
		}
	}
	vec3 Separation(std::vector<Boid>* flock) {
		vec3 cv = vec3(0.0f);
		int nc = 0;
		for (int i = 0; i < flock->size(); i++) {
			if (&(*flock)[i] != this && 
				dist(p, (*flock)[i].p) < BOID_PERCEPTION && 
				dist(p, (*flock)[i].p) > 0) {
				vec3 iv = p - (*flock)[i].p;
				iv = normalize(iv);
				iv /= dist(p, (*flock)[i].p);
				cv += iv;
				nc++;
			}
		}
		if (nc > 0) {
			cv /= nc;
			cv = normalize(cv);
			return cv;
		} else {
			return vec3(0.0f);
		}
	}
	void Move() {
		p += v;
		if (AVOID_WALLS) {
			// Steer boids away from walls
			if (p.x < -0.8)
				v += vec3(1 / (1 - p.x), 0.0f, 0.0f); // left wall
			if (p.x > 0.8)
				v += vec3(1 / (-1 - p.x), 0.0f, 0.0f); // right wall
			if (p.y > 0.8)
				v += vec3(0.0f, 1 / (-1 - p.y), 0.0f); // top wall
			if (p.y < -0.8)
				v += vec3(0.0f, 1 / (1 - p.y), 0.0f); // bottom wall
			if (p.z < -0.8)
				v += vec3(0.0f, 0.0f, 1 / (1 - p.z)); // front wall
			if (p.z > 0.8)
				v += vec3(0.0f, 0.0f, 1 / (-1 - p.z)); // back wall
			v = normalize(v);
			v *= BOID_SPEED;
		} else {
			// Wrap boids around edges of screen
			if (p.x > 1.0f)
				p.x = -1.0f;
			if (p.x < -1.0f)
				p.x = 1.0f;
			if (p.y > 1.0f)
				p.y = -1.0f;
			if (p.y < -1.0f)
				p.y = 1.0f;
			if (p.z > 1.0f)
				p.z = -1.0f;
			if (p.z < -1.0f)
				p.z = 1.0f;
		}
	}
	void Run(std::vector<Boid>* flock) {
		vec3 a_vec = Alignment(flock) * ALIGNMENT_WEIGHT;
		vec3 c_vec = Cohesion(flock) * COHESION_WEIGHT;
		vec3 s_vec = Separation(flock) * SEPARATION_WEIGHT;
		v += a_vec + c_vec + s_vec;
		v = normalize(v);
		v *= BOID_SPEED;
		Move();
	}
};

std::vector<Boid> flock;

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
	out vec4 pColor;
	void main() {
		pColor = vec4(1, 1, 1, 1);
	}
)";

void InitVertexBuffer() {
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	size_t size = sizeof(cube_points) + (obj_points.size() * sizeof(vec3));
	glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube_points), cube_points);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(cube_points), obj_points.size() * sizeof(vec3), &obj_points[0]);
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
	glClearColor(0.3, 0.3, 0.4, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	SetUniform(program, "persp", camera.persp);
	// Draw cube
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	SetUniform(program, "modelview", camera.modelview);
	for (int i = 0; i < 6; i++) {
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[i]);
	}
	// Draw boids
	VertexAttribPointer(program, "point", 3, 0, (void*)sizeof(cube_points));
	for (int i = 0; i < flock.size(); i++) {
		flock[i].Run(&flock);
		mat4 trans = Translate(flock[i].p);
		mat4 scale = Scale(BOID_SIZE);
		mat4 rot = LookAt(flock[i].p, flock[i].v, vec3(0.0f, 1.0f, 0.0f));
		mat4 modelview = camera.modelview * trans * scale * rot;
		SetUniform(program, "modelview", modelview);
		for (int j = 0; j < obj_triangles.size(); j++) {
			glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, &obj_triangles[j]);
		}
	}
	glFlush();
}

int main() {
	srand(time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "Boids", NULL, NULL);
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
	if (!ReadAsciiObj((char*)objFilename, obj_points, obj_triangles)) {
		printf("Failed to read obj file '%s'\n", objFilename);
		glfwDestroyWindow(window);
		glfwTerminate();
	}
	printf("'%s' : %i vertices, %i triangles\n", objFilename, obj_points.size(), obj_triangles.size());
	Normalize(obj_points, .8f);
	// Init boids
	for (int i = 0; i < STARTING_BOIDS; i++) {
		vec3 p = rand_vec3();
		vec3 v = rand_vec3();
		v *= BOID_SPEED;
		Boid b = Boid(p, v);
		flock.push_back(b);
	}

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


