// boids.cpp : Devon McKee

#define _USE_MATH_DEFINES

#include <glad.h>
#include <GLFW/glfw3.h>
#include "GLXtras.h"
#include <time.h>
#include <vector>
#include "VecMat.h"

GLuint vBuffer = 0;
GLuint program = 0;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 800;

const float BOID_SIZE = 40.0f;
const float BOID_SPEED = 2.0f;
const float BOID_PERCEPTION = 75.0f;

const float ALIGNMENT_WEIGHT = 1.0f;
const float COHESION_WEIGHT = 1.0f;
const float SEPARATION_WEIGHT = 1.5f;

int num_boids = 10;

float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX/(max-min)); }

struct Boid {
	vec2 p, r;
	vec3 col;
	Boid(vec2 xy, vec2 nr) {
		p = xy, r = nr;
		col = vec3(rand_float(0.25f, 1.0f), rand_float(0.25f, 1.0f), rand_float(0.25f, 1.0f));
	}
	float dist(vec2 p1, vec2 p2) {
		return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
	}
	vec2 Alignment(std::vector<Boid>* flock) {
		vec2 cv = vec2(0.0f, 0.0f);
		int nc = 0;
		for (int i = 0; i < flock->size(); i++) {
			if (&(*flock)[i] != this && dist(p, (*flock)[i].p) < BOID_PERCEPTION) {
				cv += (*flock)[i].r;
				nc++;
			}
		}
		if (nc > 0) {
			cv /= (float)nc;
			cv = normalize(cv);
			return cv;
		} else {
			return vec2(0.0f, 0.0f);
		}
	}
	vec2 Cohesion(std::vector<Boid>* flock) {
		vec2 cv = vec2(0.0f, 0.0f);
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
			return vec2(0.0f, 0.0f);
		}
	}
	vec2 Separation(std::vector<Boid>* flock) {
		vec2 cv = vec2(0.0f, 0.0f);
		int nc = 0;
		for (int i = 0; i < flock->size(); i++) {
			if (&(*flock)[i] != this && 
				dist(p, (*flock)[i].p) < BOID_PERCEPTION && 
				dist(p, (*flock)[i].p) > 0) {
				vec2 iv = p - (*flock)[i].p;
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
			return vec2(0.0f, 0.0f);
		}
	}
	void Move() {
		// Get movement vector pointing upward
		vec4 m_vec = vec4(0.0f, BOID_SPEED, 0, 1);
		// Rotate movement vector
		mat4 rot = RotateZ(atan2(r.y, r.x) * (180 / M_PI));
		m_vec = rot * m_vec;
		// Add movement vector to position vector
		p += vec2(m_vec.x, m_vec.y);

		// Wrap around screen
		if (p.x > 400)
			p.x = -400;
		if (p.x < -400)
			p.x = 400;
		if (p.y > 400)
			p.y = -400;
		if (p.y < -400)
			p.y = 400;
	}
	void Run(std::vector<Boid>* flock) {
		vec2 a_vec = Alignment(flock) * ALIGNMENT_WEIGHT;
		vec2 c_vec = Cohesion(flock) * COHESION_WEIGHT;
		vec2 s_vec = Separation(flock) * SEPARATION_WEIGHT;
		r += a_vec + c_vec + s_vec;
		r = normalize(r);
		r *= BOID_SPEED;
		Move();
	}
};

std::vector<Boid> flock = std::vector<Boid>();
float vertices[][2] = { 
	{0.0f, (BOID_SIZE / 2) / SCREEN_HEIGHT}, 
	{(BOID_SIZE / 2) / SCREEN_WIDTH, (BOID_SIZE / 2) / SCREEN_HEIGHT * -1}, 
	{(BOID_SIZE / 2) / SCREEN_WIDTH * -1, (BOID_SIZE / 2) / SCREEN_HEIGHT * -1}
};
int triangles[] = { 0, 1, 2 };

const char* vertexShader = R"( 
	#version 130
	in vec2 point;
	uniform mat4 m;
	void main() {
		gl_Position = m * vec4(point, 0, 1);
	}
)";

const char* fragmentShader = R"(
	#version 130
	out vec4 pColor;
	uniform vec3 col;
	void main() {
		pColor = vec4(col, 1);
	}
)";

void InitVertexBuffer() {
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*6, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float)*6, vertices);
}

void MouseMove(GLFWwindow* w, double x, double y) {
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		vec2 m = vec2((float)x, (float)y);
		m.x -= (SCREEN_WIDTH / 2), m.y = (m.y - (SCREEN_HEIGHT / 2)) * -1;
		float r_ang = rand_float() * 2 * M_PI;
		vec2 r = vec2(cos(r_ang), sin(r_ang));
		Boid b = Boid(m, r);
		flock.push_back(b);
		num_boids++;
	}
}

void Display() {
	glClearColor(0.3, 0.3, 0.3, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	VertexAttribPointer(program, "point", 2, 0, (void*)0);
	for (int i = 0; i < num_boids; i++) {
		flock[i].Run(&flock);
		mat4 rot = RotateZ(atan2(flock[i].r.y, flock[i].r.x) * (180 / M_PI));
		mat4 trans = Translate(flock[i].p.x / (SCREEN_WIDTH / 2), flock[i].p.y / (SCREEN_HEIGHT / 2), 0.0f);
		mat4 m = trans * rot;
		SetUniform(program, "m", m);
		SetUniform(program, "col", flock[i].col);
		glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, triangles);
	}
	glFlush();
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

int main() {
	srand(time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Boids", NULL, NULL);
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
	// Init boids
	for (int i = 0; i < num_boids; i++) {
		float x = (rand() % SCREEN_WIDTH), y = (rand() % SCREEN_HEIGHT);
		float r_ang = rand_float() * 2 * M_PI;
		vec2 p = vec2(x, y);
		vec2 r = vec2(cos(r_ang), sin(r_ang));
		Boid b = Boid(p, r);
		flock.push_back(b);
	}
	InitVertexBuffer();
	glfwSetCursorPosCallback(window, MouseMove);
	glfwSetKeyCallback(window, Keyboard);
	glfwSwapInterval(1); // vsync
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