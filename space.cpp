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

vector<dSkybox> skyboxes;
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

Camera camera(win_width, win_height, 60, 0.5, 800.0f);

struct Ship {
	Mesh mesh;
	vec3 pos = vec3(0, 0, 0);
	vec3 dir = vec3(1, 0, 0);
	vec3 up = vec3(0, 1, 0);
	vec3 vel = vec3(0, 0, 0);
	mat4 transform() {
		return Translate(pos) * Orientation(dir, up) * mesh.model;
	}
	void update(float dt) {
		// Adjust roll
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
			up = vec3(RotateAxis(dir, dt * 1.5f) * vec4(up, 1));
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
			up = vec3(RotateAxis(dir, dt * -1.5f) * vec4(up, 1));
		// Adjust yaw
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			dir = vec3(RotateAxis(up, dt * 1.0f) * vec4(dir, 1));
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			dir = vec3(RotateAxis(up, dt * -1.0f) * vec4(dir, 1));
		// Adjust pitch
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			mat4 r = RotateAxis(cross(dir, up), dt * 1.5f);
			dir = vec3(r * vec4(dir, 1));
			up = vec3(r * vec4(up, 1));
		}
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			mat4 r = RotateAxis(cross(dir, up), dt * -1.5f);
			dir = vec3(r * vec4(dir, 1));
			up = vec3(r * vec4(up, 1));
		}
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
	// Setup meshes
	ship.mesh = Mesh("objects/starsparrow.obj", "textures/starsparrow.png", mat4());
	ship.pos = vec3(2, 0, 0);
	// Setup skyboxes
	for (string path : skyboxPaths) {
		dSkybox skybox;
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
	ship.mesh.cleanup();
	for (dSkybox skybox : skyboxes)
		skybox.cleanup();
}

void draw() {
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glCullFace(GL_BACK);
	mainPass.use();
	mainPass.set("txtr", 0);
	mainPass.set("persp", camera.persp);
	mainPass.set("view", camera.view);
	mainPass.set("model", ship.transform());
	ship.mesh.render();
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.persp);
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