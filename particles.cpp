// particles.cpp : Devon McKee

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

const vec2 GRAVITY = vec2(0.0f, -0.0025f);
const float LIFE_DT = 0.025f;
const float PARTICLE_SIZE = 3.0f;
const float H_VARIANCE = 0.01f;
const int NUM_PARTICLES_SPAWNED = 10;

float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX / (max - min)); }

struct Particle {
	vec2 pos, vel;
	vec4 col;
	float life;
	Particle() {
		pos = vec2(0.0f), vel = vec2(0.0f), col = vec4(1.0f), life = 0.0f;
	}
	void Revive(vec2 new_pos = vec2(0.0f, 0.0f)) {
		life = 1.0f;
		pos = new_pos;
		vel = vec2(rand_float(-1 * H_VARIANCE, H_VARIANCE), rand_float(0.025f, 0.05f));
	}
	void Run() {
		if (life > 0.0f) {
			life -= LIFE_DT;
			pos += vel;
			vel += GRAVITY;
			col = vec4(1.0f, 1 - life, 0.0f, 0.0f);
		}
	}
};

int num_particles = 1000;
std::vector<Particle> particles = std::vector<Particle>();
int lastUsedParticle = 0;
bool mouseDown = false;

float vertices[][2] = { {-1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f} };
int triangles[] = { 0, 1, 2, 1, 2, 3 };

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
	uniform vec4 col;
	void main() {
		pColor = col;
	}
)";

void InitVertexBuffer() {
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
}

int FindNextParticle() {
	// Search for dead particle from last used particle
	for (int i = lastUsedParticle; i < num_particles; i++) {
		if (particles[i].life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// Search linearly through rest of particles
	for (int i = 0; i < lastUsedParticle; i++) {
		if (particles[i].life <= 0.0f) {
			lastUsedParticle = i;
			return i;
		}
	}
	// If no dead particles exist, simply take the first particle
	lastUsedParticle = 0;
	return 0;
}

void Display() {
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	VertexAttribPointer(program, "point", 2, 0, (void*)0);
	for (int i = 0; i < num_particles; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].Run();
			mat4 scale = Scale(PARTICLE_SIZE / SCREEN_WIDTH, PARTICLE_SIZE / SCREEN_HEIGHT, 0.0f);
			mat4 trans = Translate(vec3(particles[i].pos, 0.0f));
			mat4 m = trans * scale;
			SetUniform(program, "m", m);
			SetUniform(program, "col", particles[i].col);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, triangles);
		}
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glFlush();
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

void MouseButton(GLFWwindow* w, int butn, int action, int mods) {
	if (butn == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			mouseDown = true;
		}
		if (action == GLFW_RELEASE) {
			mouseDown = false;
		}
	}
}

void SpawnParticle(GLFWwindow* w) {
	double x, y;
	glfwGetCursorPos(w, &x, &y);
	vec2 m = vec2((float)x, (float)y);
	m.x = (m.x - (SCREEN_WIDTH / 2)) / (SCREEN_WIDTH / 2);
	m.y = ((m.y - (SCREEN_HEIGHT / 2)) * -1) / (SCREEN_HEIGHT / 2);
	for (int i = 0; i < NUM_PARTICLES_SPAWNED; i++) {
		int p = FindNextParticle();
		particles[p].Revive(m);
	}
}

int main() {
	srand(time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Particles", NULL, NULL);
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
	for (int i = 0; i < num_particles; i++) {
		particles.push_back(Particle());
	}
	glfwSetMouseButtonCallback(window, MouseButton);
	glfwSetKeyCallback(window, Keyboard);
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(window)) {
		Display();
		if (mouseDown) {
			SpawnParticle(window);
		}
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vBuffer);
	glfwDestroyWindow(window);
	glfwTerminate();
}