// particles-3d.cpp : Devon McKee

#define _USE_MATH_DEFINES

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

const vec3 GRAVITY = vec3(0.0f, -0.0025f, 0.0f);
const float LIFE_DT = 0.02f;
const float PARTICLE_SIZE = 0.03f;
const vec2 H_RANGE = vec2(-0.01f, 0.01f);
const vec2 V_RANGE = vec2(0.03f, 0.06f);
const int NUM_PARTICLES_SPAWNED = 10;

const char* texture = "textures/lava.tga";

int win_width = 800;
int win_height = 800;

Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

vec3 lightSource = vec3(1, 1, 0);

float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX / (max - min)); }
vec3 rand_vec3(float min = -1, float max = 1) { return vec3(min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min))); }

struct Particle {
	vec3 pos, vel;
	vec4 col;
	float life;
	Particle() {
		pos = vec3(0.0f), vel = vec3(0.0f), col = vec4(1.0f), life = 0.0f;
	}
	void Revive(vec3 new_pos = vec3(0.0f)) {
		life = 1.0f;
		pos = new_pos;
		vel = vec3(
			rand_float(H_RANGE.x, H_RANGE.y),
			rand_float(V_RANGE.x, V_RANGE.y),
			rand_float(H_RANGE.x, H_RANGE.y)
		);
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

float cube_points[][3] = {
	{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, // front (0, 1, 2, 3)
	{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, // back (4, 5, 6, 7)
	{-1, -1, 1}, {-1, -1, -1}, {-1, 1, -1}, {-1, 1, 1}, // left (8, 9, 10, 11)
	{1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}, // right (12, 13, 14, 15)
	{-1 , 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, // top  (16, 17, 18, 19)
	{-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}  // bottom (20, 21, 22, 23)
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
	uniform vec4 color = vec4(1.0, 1.0, 0.4, 1);
	uniform float amb = 0.6; // ambient intensity
	uniform float dif = 0.7; // diffusivity
	uniform float spc = 0.5; // specularity
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
	glClearColor(0.3f, 0.3f, 0.3f, 1.);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	SetUniform(program, "persp", camera.persp);
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	VertexAttribPointer(program, "normal", 3, 0, (void*)sizeof(cube_points));
	VertexAttribPointer(program, "uv", 2, 0, (void*)(sizeof(cube_points) + sizeof(cube_normals)));
	glActiveTexture(GL_TEXTURE0 + texUnit);
	glBindTexture(GL_TEXTURE_2D, texName);
	SetUniform(program, "texImage", (int)texUnit);
	for (int i = 0; i < num_particles; i++) {
		if (particles[i].life > 0.0f) {
			particles[i].Run();
			mat4 scale = Scale(PARTICLE_SIZE);
			mat4 trans = Translate(particles[i].pos);
			mat4 modelview = camera.modelview * trans * scale;
			SetUniform(program, "modelview", modelview);
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, cube_triangles);
		}
	}
	glFlush();
}

int main() {
	srand((int)time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "Particles 3D", NULL, NULL);
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
	texName = LoadTexture(texture, texUnit);
	for (int i = 0; i < num_particles; i++) {
		particles.push_back(Particle());
	}
	glfwSetCursorPosCallback(window, MouseMove);
	glfwSetMouseButtonCallback(window, MouseButton);
	glfwSetScrollCallback(window, MouseWheel);
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, Resize);
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(window)) {
		particles[FindNextParticle()].Revive();
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