// space.cpp : Devon McKee

#define _USE_MATH_DEFINES
#include <glad.h>
#define GLFW_INCLUDE_NONE
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <GLFW/glfw3.h>
#pragma clang diagnostic pop
#include <chrono>
#include <vector>
#include <string>
#include <stdexcept>
#include "VecMat.h"
#include "GeomUtils.h"
#include "dRenderPass.h"
#include "dMesh.h"
#include "dMisc.h"
#include "dSkybox.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using std::vector;
using std::string;
using std::runtime_error;
using time_p = std::chrono::system_clock::time_point;
using sys_clock = std::chrono::system_clock;
using float_ms = std::chrono::duration<float, std::milli>;

GLFWwindow* window;
int windowed_width = 1000, windowed_height = 800;
int win_width = windowed_width, win_height = windowed_height;
GLFWmonitor* monitor;
const GLFWvidmode* videoModes;
GLFWvidmode currentVidMode;
int videoModesCount;
bool fullscreen = false;
float dt;

RenderPass mainPass;
RenderPass mainPassInst;

const char* mainVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	layout(location = 1) in vec2 uv;
	out vec2 vUv;
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 persp;
	void main() {
		vUv = uv;
		gl_Position = persp * view * model * vec4(point, 1);
	}
)";

const char* mainVertInstanced = R"(
	#version 410 core
	layout (location = 0) in vec3 point;
	layout (location = 1) in vec2 uv;
	layout (location = 3) in mat4 transform;
	out vec2 vUv;
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 persp;
	void main() {
		vUv = uv;
		gl_Position = persp * view * transform * model * vec4(point, 1);
	}
)";

const char* mainFrag = R"(
	#version 410 core
	in vec2 vUv;
	out vec4 pColor;
	uniform sampler2D txtr;
	uniform vec4 ambient = vec4(vec3(0.1), 1);
	void main() {
		pColor = texture(txtr, vUv);
	}
)";

vector<Skybox> skyboxes;
vector<string> skyboxPaths{
	"textures/skybox/sunshine/",
	"textures/skybox/space/",
	"textures/skybox/empty-space/"
};
vector<string> skyboxNames{
	"Sunshine",
	"Space",
	"Empty Space",
};
int cur_skybox = 0;

Camera camera(win_width, win_height, 60, 0.5, 1200.0f);

Mesh rock_mesh;
vector<mat4> rock_transforms;

struct Ship {
	Mesh mesh;
	vec3 pos = vec3(0, 0, 0);
	vec3 dir = vec3(1, 0, 0);
	vec3 up = vec3(0, 1, 0);
	vec3 vel = vec3(0, 0, 0);
	float mass = 500.0f;
	float engine = 8.0f;
	float drag = 30.0f;
	mat4 transform() {
		return Translate(pos) * Orientation(dir, up) * mesh.model;
	}
	void update(float dt) {
		bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
		// Adjust roll
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
			up = vec3(RotateAxis(dir, dt * 1.25f) * vec4(up, 1));
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
			up = vec3(RotateAxis(dir, dt * -1.25f) * vec4(up, 1));
		// Adjust yaw
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			dir = vec3(RotateAxis(up, dt * 0.5f) * vec4(dir, 1));
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			dir = vec3(RotateAxis(up, dt * -0.5f) * vec4(dir, 1));
		// Adjust pitch
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			mat4 r = RotateAxis(cross(dir, up), dt * 0.75f);
			dir = vec3(r * vec4(dir, 1));
			up = vec3(r * vec4(up, 1));
		}
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			mat4 r = RotateAxis(cross(dir, up), dt * -0.75f);
			dir = vec3(r * vec4(dir, 1));
			up = vec3(r * vec4(up, 1));
		}
		vec3 force = vec3(0.0);
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			force += dir * engine * (shift ? 1.5 : 1.0);
		force -= drag * vel * length(vel);
		vec3 acc = force / mass;
		vel += dt * acc;
		pos += dt * vel;
	}
} ship;

void WindowResized(GLFWwindow* window, int _width, int _height) {
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	camera.width = win_width = width;
	camera.height = win_height = height;
	glViewport(0, 0, win_width, win_height);
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_Q && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	if (key == GLFW_KEY_BACKSLASH && action == GLFW_PRESS) {
		cur_skybox++;
		if (cur_skybox >= skyboxes.size()) cur_skybox = 0;
	}
	if (key == GLFW_KEY_F && action == GLFW_PRESS) {
		if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS) {
			if (fullscreen) {
				win_width = windowed_width;
				win_height = windowed_height;
				glfwSetWindowMonitor(window, NULL, 100, 100, win_width, win_height, 0);
				fullscreen = false;
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			} else {
				glfwSetWindowMonitor(window, monitor, 0, 0, currentVidMode.width, currentVidMode.height, currentVidMode.refreshRate);
				fullscreen = true;
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			}
		}
	}
}

void setup() {
	// Initialize GLFW callbacks
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, WindowResized);
	// Load shaders into render passes
	mainPass.loadShaders(&mainVert, &mainFrag);
	mainPassInst.loadShaders(&mainVertInstanced, &mainFrag);
	// Setup meshes
	rock_mesh = Mesh("objects/rock/rock1.obj", "textures/rock/rock1.png");
	ship.mesh = Mesh("objects/sparrow/StarSparrow04.obj", "textures/starsparrow.png");
	ship.pos = vec3(2, 0, 0);
	// Setup instances
	for (int i = 0; i < 10000; i++) {
		mat4 m;
		m = RotateX(rand_float(-180, 180)) * m;
		m = RotateY(rand_float(-180, 180)) * m;
		m = RotateZ(rand_float(-180, 180)) * m;
		m = Scale(rand_float(0.5, 8.0)) * m;
		m = Translate(rand_float(-600, 600), rand_float(-600, 600), rand_float(-600, 600)) * m;
		rock_transforms.push_back(m);
	}
	rock_mesh.setupInstanceBuffer((GLsizei)rock_transforms.size());
	rock_mesh.loadInstances(rock_transforms);
	// Setup skyboxes
	for (string path : skyboxPaths) {
		Skybox skybox;
		skybox.setup();
		skybox.loadCubemap(path);
		skyboxes.push_back(skybox);
	}
	// Set some GL drawing context settings
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
}

void cleanup() {
	rock_mesh.cleanup();
	ship.mesh.cleanup();
	for (Skybox skybox : skyboxes)
		skybox.cleanup();
	mainPass.cleanup();
	mainPassInst.cleanup();
}

void draw() {
	camera.loc = ship.pos - (ship.dir * 3.0) + (ship.up * 1.0);
	camera.look = ship.pos + (ship.dir * 4.0);
	camera.up = ship.up;
	camera.fov = 60 + (length(ship.vel) * 45);
	camera.update();
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glCullFace(GL_BACK);
	mainPass.use();
	mainPass.set("txtr", 0);
	mainPass.set("persp", camera.persp);
	mainPass.set("view", camera.view);
	mainPass.set("model", ship.transform());
	ship.mesh.render();
	mainPass.set("model", rock_mesh.model);
	mainPassInst.use();
	mainPassInst.set("txtr", 0);
	mainPassInst.set("persp", camera.persp);
	mainPassInst.set("view", camera.view);
	mainPassInst.set("model", rock_mesh.model);
	rock_mesh.renderInstanced();
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.up, camera.persp);
	glFlush();
}

int main() {
	srand((int)time(NULL));
	// Initializing GLFW and creating window
	if (!glfwInit())
		return 1;
	glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	window = glfwCreateWindow(win_width, win_height, "Space", NULL, NULL);
	if (!window)
		throw runtime_error("Failed to create GLFW window!");
	// Setting up GLFW/OpenGL context 
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	glfwSetWindowPos(window, 100, 100);
	glfwGetFramebufferSize(window, &win_width, &win_height);
	glfwSwapInterval(1);
	if (!(monitor = glfwGetPrimaryMonitor()))
		throw runtime_error("Failed to get GLFW primary monitor!");
	videoModes = glfwGetVideoModes(monitor, &videoModesCount);
	currentVidMode = videoModes[videoModesCount - 1];
	setup();
	time_p lastSim = sys_clock::now();
	while (!glfwWindowShouldClose(window)) {
		time_p cur = sys_clock::now();
		float_ms since = cur - lastSim;
		dt = 1 / (1000.0f / since.count() / 60.0f);
		lastSim = cur;
		ship.update(dt);
		draw();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}