// drive.cpp : Devon McKee

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
#include "GLXtras.h"
#include "GeomUtils.h"
#include "Mesh.h"
#include "Misc.h"
#include "dSkybox.h"
#include "dParticles.h"

using std::vector;
using std::string;
using std::runtime_error;

GLFWwindow* window;
int win_width = 800, win_height = 800;
GLuint textureUnits = 1;

typedef std::chrono::system_clock::time_point time_p;
typedef std::chrono::system_clock sys_clock;
typedef std::chrono::duration<double, std::milli> double_ms;

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
	uniform vec3 light = vec3(0, 10, 0);
	uniform vec4 color = vec4(1, 1, 1, 1);
	uniform float amb = 0.6;
	uniform float dif = 0.6;
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

vector<dSkybox> skyboxes;
vector<string> skyboxPaths{
	"textures/skybox/maskonaive/",
	"textures/skybox/classic-land/",
	"textures/skybox/empty-space/",
	"textures/skybox/dusk-ocean/"
};
int cur_skybox = 0;

struct Camera {
	float fov = 60;
	float zNear = 0.1f;
	float zFar = 1000.0f;
	int width, height;
    vec3 loc, look;
    vec3 up = vec3(0, 1, 0);
	mat4 view;
	mat4 persp;
	Camera() {
		width = win_width;
		height = win_height;
		loc = vec3(-1.5, 2, 0);
		look = vec3(0, 0, 0);
		view = LookAt(loc, look, up);
		persp = Perspective(fov, width / height, zNear, zFar);
	}
	void moveTo(vec3 _loc) {
		loc = _loc;
		view = LookAt(_loc, look, up);
	}
	void lookAt(vec3 _look) {
		look = _look;
		view = LookAt(loc, _look, up);
	}
	void adjustFov(float _fov) {
		fov = _fov;
		persp = Perspective(fov, win_width / win_height, zNear, zFar);
	}
	void resize(int _width, int _height) {
		width = _width;
		height = _height;
		persp = Perspective(fov, width / height, zNear, zFar);
	}
} camera;

int camera_loc = 1;
// 1 - Third person chase camera
// 2 - Top-down camera of arena
// 3 - Hood camera
// 4 - Frozen free camera, moved right behind car

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
	MeshContainer(vector<vec3> _points, vector<vec3> _normals, vector<vec2> _uvs, vector<int3> _triangles, string texFilename, bool texMipmap = true) {
		points = _points;
		normals = _normals;
		uvs = _uvs;
		triangles = _triangles;
		texUnit = textureUnits++;
		texture = LoadTexture(texFilename.c_str(), texUnit, texMipmap);
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
		mat4 m = transform * model;
		SetUniform(program, "model", m);
		SetUniform(program, "view", camera.view);
		SetUniform(program, "persp", camera.persp);
		SetUniform(program, "txtr", (int)texture);
		glDrawElements(GL_TRIANGLES, (GLsizei)(triangles.size() * sizeof(int3)), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	void deallocate() {
		glDeleteVertexArrays(1, &vArray);
		glDeleteBuffers(1, &vBuffer);
		glDeleteBuffers(1, &iBuffer);
		glDeleteTextures(1, &texture);
	}
};

MeshContainer floor_mesh;
vector<vec3> floor_points = { {-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1} };
vector<vec3> floor_normals = { {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0} };
vector<vec2> floor_uvs = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
vector<int3> floor_triangles = { {2, 1, 0}, {0, 3, 2} };

MeshContainer large_tree_mesh;
vector<vec3> large_tree_positions = {
	{49, 0, 37}, {50, 0, 32}, {29, 0, 15}, {50, 0, 10},
	{-10, 0, 32}, {-34, 0, 7}, {15, 0, -13}, {6, 0, -15},
	{-0.75, 0, 0.3}
};

struct Car {
	MeshContainer mesh;
	float mass; // mass of car
	float engine; // engine force
	float roll; // rolling resistance
	float drag; // aerodynamic drag constant
	vec3 pos; // position
	vec3 dir; // direction
	vec3 vel; // velocity
	Car() { };
	Car(MeshContainer _mesh, float _mass, float _engine, float _roll, float _drag) {
		mesh = _mesh;
		mass = _mass;
		engine = _engine;
		roll = _roll;
		drag = _drag;
		pos = vec3(0, 0, 0);
		dir = vec3(1, 0, 0);
		vel = vec3(0, 0, 0);
	}
	void draw() { 
		mat4 transform = Translate(pos) * Orientation(dir, vec3(0, 1, 0));
		mesh.draw(transform); 
	}
	void collide() {
		if (pos.y < 0) pos.y = 0;
		if (pos.x > 59.5) { pos.x = 59.5; }
		else if (pos.x < -59.5) { pos.x = -59.5; }
		else if (pos.z > 59.5) { pos.z = 59.5; }
		else if (pos.z < -59.5) { pos.z = -59.5; }
		for (vec3 tree_pos : large_tree_positions) {
			float d = dist(pos, tree_pos);
			if (d < 1.5) vel *= 0.5;
			//if (d < 1.0) vel = (pos - tree_pos); hard collision with tree
		}
	}
	void update(float dt) {
		// Weight update by time delta for consistent effect of updates
		//   regardless of framerate
		float wt = 1 / (1000.0f / dt / 60.0f);
		// Check if space key pressed ("drifting")
		float drift = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) ? 0.5 : 1.0;
		// Check for car turning with A/D
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			float deg = wt * -1.5 * ((drift == 0.5) ? 1.25 : 1.0);
			vec4 dirw = RotateY(deg) * dir;
			dir = vec3(dirw.x, dirw.y, dirw.z);
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			float deg = wt * 1.5 * ((drift == 0.5) ? 1.25 : 1.0);
			vec4 dirw = RotateY(deg) * dir;
			dir = vec3(dirw.x, dirw.y, dirw.z);
		}
		vec3 force = vec3(0.0);
		// Apply engine force if pressed (F = u{vel} * engine)
		float turbo = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 1.5 : 1.0;
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			force += turbo * drift * dir * engine;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			force -= turbo * drift * dir * engine;
		// Apply aerodynamic drag (F = - C{drag} * vel * |vel|)
		force -= drag * vel * length(vel);
		// Apply rolling resistance (F = - C{roll} * vel)
        // Weigh rolling resistance by up to 2x normal based on angle difference between direction and velocity
        float roll_wt = 1 - (dot(dir, vel) / length(dir) / length(vel));
        if (roll_wt > 1) roll_wt = 1 - (roll_wt - 1);
        roll_wt = roll_wt * 4 + 1;
        if (length(vel) < 0.001) roll_wt = 1.0;
		force -= drift * roll_wt * roll * vel;
		vec3 acc = wt * force / mass;
		// Move velocity according to acceleration
		vel += acc;
		// Move position according to velocity
		pos += vel;
		
	}
} car;

void WindowResized(GLFWwindow* window, int width, int height) {
	win_width = width;
	win_height = height;
	camera.resize(width, height);
	glViewport(0, 0, win_width, win_height);
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		car.pos = vec3(2, 0, 0);
		car.vel = vec3(0, 0, 0);
	}
	if (key == GLFW_KEY_P && action == GLFW_PRESS)
		printf("Pos: %.2f %.2f %.2f\n", car.pos.x, car.pos.y, car.pos.z);
    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
        camera_loc = 1;
    if (key == GLFW_KEY_2 && action == GLFW_PRESS)
        camera_loc = 2;
    if (key == GLFW_KEY_3 && action == GLFW_PRESS)
        camera_loc = 3;
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
        camera_loc = 4;
        camera.moveTo(car.pos + vec3(0, 1.5, 0) + -3 * car.dir);
        camera.lookAt(car.pos + 2.5 * car.dir);
        camera.up = vec3(0, 1, 0);
    }
	if (key == GLFW_KEY_BACKSLASH && action == GLFW_PRESS) {
		cur_skybox++;
		if (cur_skybox >= skyboxes.size()) cur_skybox = 0;
	}
}

void setup() {
	// Initialize GLFW callbacks
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, WindowResized);
	glfwSwapInterval(1);
	// Setup meshes
	mat4 car_transform = Scale(0.75f) * Translate(0, 0.5, 0) * RotateY(-90);
	MeshContainer car_mesh = MeshContainer("objects/car.obj", "textures/car.png", car_transform);
	car_mesh.allocate();
	// Mesh, mass, engine force, rolling resistance, air drag
	car = Car(car_mesh, 500.0, 1.5, 15.0, 10.0);
	car.pos = vec3(2, 0, 0);
	floor_mesh = MeshContainer(floor_points, floor_normals, floor_uvs, floor_triangles, "textures/racetrack.png", false);
	floor_mesh.allocate();
	mat4 large_tree_transform = Scale(2.0) * Translate(0, 0.9, 0);
	large_tree_mesh = MeshContainer("objects/largetree.obj", "textures/largetree.png", large_tree_transform);
	large_tree_mesh.allocate();
	// Setup skyboxes
	for (string path : skyboxPaths) {
		dSkybox skybox;
		skybox.setup();
		skybox.loadCubemap(path, textureUnits++);
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
	// Cleanup meshes
	car.mesh.deallocate();
	floor_mesh.deallocate();
	large_tree_mesh.deallocate();
	for (dSkybox skybox : skyboxes)
		skybox.cleanup();
}

void draw() {
	glClearColor(0.651f, 0.961f, 0.941f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    if (camera_loc == 1) { // Third person chase camera
        vec3 cameraDir = (car.dir + 3 * car.vel) / 2;
        camera.moveTo(car.pos + vec3(0, 1.5, 0) + -5 * cameraDir);
        camera.lookAt(car.pos + 4 * cameraDir);
        camera.up = vec3(0, 1, 0);
        camera.adjustFov(60 + length(car.vel) * 50);
    } else if (camera_loc == 2) { // Top-down camera
        camera.moveTo(car.pos + vec3(0, 30, 0));
        camera.lookAt(car.pos);
        camera.up = car.dir;
        camera.adjustFov(60);
    } else if (camera_loc == 3) {
        camera.moveTo(car.pos + vec3(0, 0.5, 0) + car.dir * 0.5);
        camera.lookAt(car.pos + car.dir * 3 + vec3(0, 0.5, 0));
        camera.up = vec3(0, 1, 0);
        camera.adjustFov(60);
    }
	floor_mesh.draw(Scale(60));
	for (vec3 pos : large_tree_positions)
		large_tree_mesh.draw(Translate(pos));
	car.draw();
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.persp);
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
	time_p lastSim = sys_clock::now();
	while (!glfwWindowShouldClose(window)) {
		time_p cur = sys_clock::now();
		double_ms since = cur - lastSim;
		car.update(since.count());
		lastSim = cur;
		car.collide();
		draw();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}
