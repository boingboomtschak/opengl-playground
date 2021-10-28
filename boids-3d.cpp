// boids-3d.cpp : Devon McKee

#define _USE_MATH_DEFINES

#include <glad.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"
#include "Mesh.h"
#include "Draw.h"
#include "Misc.h"
#include "GLXtras.h"
#include "dMesh.h"

using std::vector;

GLuint vBuffer = 0;
GLuint cubeProgram = 0;
GLuint boidTexUnit = 1; 
GLuint rockTexUnit = 2; 

const char *boidObjFilename  = "./objects/tuna.obj";
const char *boidTexFilename  = "./textures/tuna.tga";
const char* rockObjFilename  = "./objects/rock.obj";
const char* rockTexFilename  = "./textures/rock.tga";
const char* rockNormFilename = "./textures/rock_normal.tga";

int win_width = 800;
int win_height = 800;

Camera camera((float) win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

dMesh boid_mesh;
dMesh rock_mesh;

vec3 lightSource = vec3(1, 1, 0);

float cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} };
int cube_faces[][4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {4, 5, 6, 7}, {6, 7, 0, 1}, {0, 3, 4, 7}, {1, 2, 5, 6} };

float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX / (max - min)); }
vec3 rand_vec3(float min = -1, float max = 1) { return vec3(min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min))); }
float dist(vec3 p1, vec3 p2) { return (float)sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2) + pow(p2.z - p1.z, 2)); }
mat4 Orientation(vec3 forward, vec3 up) { // JB see textbook p. 189
	vec3 z = normalize(forward);
	vec3 x = normalize(cross(z, up));
	vec3 y = normalize(cross(x, z));
	return mat4(vec4(x.x, y.x, z.x, 0), vec4(x.y, y.y, z.y, 0), vec4(x.z, y.z, z.z, 0), vec4(0, 0, 0, 1));
}
vec3 XVec(vec3 v, mat4 m) { vec4 x = m * vec4(v); return .1f * vec3(x.x, x.y, x.z); }


const float BOID_SPEED = 0.005f;
const float BOID_SIZE = 0.07f;
const float BOID_SIZE_VARIANCE = 0.02f;
const float ROCK_SIZE = 0.15f;
const float ROCK_SIZE_VARIANCE = 0.075f;
const float BOID_PERCEPTION = 0.3f;
const float WALL_RANGE = 0.1f;
const float ALIGNMENT_WEIGHT = 1.0f;
const float COHESION_WEIGHT = 1.0f;
const float SEPARATION_WEIGHT = 1.0f;

const int STARTING_BOIDS = 150;
const int STARTING_ROCKS = 200;

const bool AVOID_WALLS = false;
const bool DRAW_REF_VECTORS = false;

bool running = true;

struct Boid;
struct Rock;
vector<Boid> flock;
vector<Rock> rocks;

struct Boid {
	vec3 p, v; // position and velocity vector
	vector<int> nb;
	float size = BOID_SIZE + (rand_float(-1, 1) * BOID_SIZE_VARIANCE);
	Boid(vec3 xy, vec3 nv) {
		p = xy; v = nv; 
	}
	void FindNeighbors() {
		for (size_t i = 0; i < flock.size(); i++) {
			float d = dist(p, flock[i].p);
			if (&flock[i] != this && d < BOID_PERCEPTION && d > 0) {
				nb.push_back(i);
			}
		}
	}
	vec3 Alignment() {
		vec3 cv = vec3(0.0f);
		int nc = 0;
		for (size_t i = 0; i < nb.size(); i++) {
			size_t n = nb[i]; 
			cv += flock[n].v;
			nc++;
		}
		if (nc > 0) {
			cv /= (float)nc;
			cv = normalize(cv);
			return cv;
		} else {
			return vec3(0.0f);
		}
	}
	vec3 Cohesion() {
		vec3 cv = vec3(0.0f);
		int nc = 0;
		for (size_t i = 0; i < nb.size(); i++) { 
			size_t n = nb[i];
			cv += flock[n].p;
			nc++;
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
	vec3 Separation() {
		vec3 cv = vec3(0.0f);
		float nc = 0; 
		for (size_t i = 0; i < nb.size(); i++) { 
			size_t n = nb[i];
			vec3 iv = p - flock[n].p;
			iv = normalize(iv);
			iv /= dist(p, flock[n].p);
			cv += iv;
			nc++;
		}
		if (nc > 0) {
			cv /= nc;
			cv = normalize(cv);
			return cv;
		} else {
			return vec3(0.0f);
		}
	}
	vec3 Avoidance() {
		p += v;
		if (AVOID_WALLS) {
			// Steer boids away from walls
			if (p.x < -1 + WALL_RANGE)
				return vec3(1 / (1 - p.x), 0.0f, 0.0f); // left wall
			if (p.x > 1 - WALL_RANGE)
				return vec3(1 / (-1 - p.x), 0.0f, 0.0f); // right wall
			if (p.y > 1 - WALL_RANGE)
				return vec3(0.0f, 1 / (-1 - p.y), 0.0f); // top wall
			if (p.y < -1 + WALL_RANGE)
				return vec3(0.0f, 1 / (1 - p.y), 0.0f); // bottom wall
			if (p.z < -1 + WALL_RANGE)
				return vec3(0.0f, 0.0f, 1 / (1 - p.z)); // front wall
			if (p.z > 1 - WALL_RANGE)
				return vec3(0.0f, 0.0f, 1 / (-1 - p.z)); // back wall
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
		return vec3(0.0f);
	}
	void Run() {
		FindNeighbors();
		vec3 a_vec = Alignment() * ALIGNMENT_WEIGHT;
		vec3 c_vec = Cohesion() * COHESION_WEIGHT;
		vec3 s_vec = Separation() * SEPARATION_WEIGHT;
		vec3 w_vec = Avoidance();
		v += a_vec + c_vec + s_vec + w_vec;
		v = normalize(v);
		v *= BOID_SPEED;
		// Clear neighbors
		nb.clear(); 
	}
	void Draw() {
		boid_mesh.PreDisplay();
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		mat4 trans = Translate(p);
		mat4 scale = Scale(size);
		mat4 orient = Orientation(v, vec3(0.0f, 1.0f, 0.0f));
		mat4 modelview = trans * scale * orient;
		boid_mesh.Display(camera, &modelview);
	}
};

struct Rock {
	vec3 p, r; // pos, rotation
	float size;
	Rock(vec3 pos) {
		p = pos; size = ROCK_SIZE + (rand_float(-1, 1) * ROCK_SIZE_VARIANCE), r = rand_vec3(0, 360);
	}
	void Draw() {
		rock_mesh.PreDisplay();
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		mat4 trans = Translate(p);
		mat4 scale = Scale(size);
		mat4 rot = RotateX(r.x) * RotateY(r.y) * RotateZ(r.z);
		mat4 modelview = trans * scale * rot;
		rock_mesh.Display(camera, &modelview);
	}
};

const char* cubeVertShader = R"(
	#version 130
	in vec3 point;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		gl_Position = persp * modelview * vec4(point, 1);
	}
)";

const char* cubeFragShader = R"(
	#version 130
	out vec4 pColor;
	void main() {
		pColor = vec4(1, 1, 1, 1);
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
	if (key == GLFW_KEY_J)
		running = false;
	if (key == GLFW_KEY_K)
		running = true;
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
	size_t size = sizeof(cube_points);
	glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube_points), cube_points);
}

void InitSceneObjects() {
	for (int i = 0; i < STARTING_BOIDS; i++) {
		vec3 p = rand_vec3();
		vec3 v = rand_vec3();
		v *= BOID_SPEED;
		Boid b = Boid(p, v);
		flock.push_back(b);
	}
	for (int i = 0; i < STARTING_ROCKS; i++) {
		vec3 p = vec3(rand_float(-1, 1), -1.0f, rand_float(-1, 1));
		Rock r = Rock(p);
		rocks.push_back(r);
	}
}

void DrawVectors() {
	UseDrawShader(camera.modelview);
	for (size_t i = 0; i < flock.size(); i++) {
		mat4 rot = Orientation(flock[i].v, vec3(0.0f, 1.0f, 0.0f));
		vec3 xaxis = XVec(vec3(1, 0, 0), rot), yaxis = XVec(vec3(0, 1, 0), rot), zaxis = XVec(vec3(0, 0, 1), rot);
		ArrowV(flock[i].p, xaxis, camera.modelview, camera.persp, vec3(1, 0, 0));
		ArrowV(flock[i].p, yaxis, camera.modelview, camera.persp, vec3(0, 1, 0));
		ArrowV(flock[i].p, zaxis, camera.modelview, camera.persp, vec3(0, 0, 1));
	}
}

void Display() {
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glUseProgram(cubeProgram);
	glClearColor(0.3f, 0.3f, 0.4f, 1.); 
	glClear(GL_COLOR_BUFFER_BIT);
	SetUniform(cubeProgram, "persp", camera.persp);
	// Draw cube
	VertexAttribPointer(cubeProgram, "point", 3, 0, (void*)0);
	SetUniform(cubeProgram, "modelview", camera.modelview);
	for (int i = 0; i < 6; i++) {
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[i]);
	}
	glClear(GL_DEPTH_BUFFER_BIT);
	// Draw boids
	for (size_t i = 0; i < flock.size(); i++) {
		if (running) {
			flock[i].Run();
		}
		flock[i].Draw();
	}
	// Draw rocks
	for (size_t i = 0; i < rocks.size(); i++) {
		rocks[i].Draw();
	}
	// Draw reference vectors
	if (DRAW_REF_VECTORS) DrawVectors();
	glFlush();
}

int main() {
	srand((int) time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "Boids 3D", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(window, 100, 100);
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	if (!(cubeProgram = LinkProgramViaCode(&cubeVertShader, &cubeFragShader)))
		return 1;
	mat4 rot = RotateY(-90.0f);
	boid_mesh.Read((char*)boidObjFilename, (char*)boidTexFilename, boidTexUnit, &rot);
	printf("'%s' : %i vertices, %i normals, %i uvs, %i triangles\n", boidObjFilename, boid_mesh.points.size(), boid_mesh.normals.size(), boid_mesh.uvs.size(), boid_mesh.triangles.size());
	rock_mesh.Read((char*)rockObjFilename, (char*)rockTexFilename, rockTexUnit);
	printf("'%s' : %i vertices, %i normals, %i uvs, %i triangles\n", rockObjFilename, rock_mesh.points.size(), rock_mesh.normals.size(), rock_mesh.uvs.size(), rock_mesh.triangles.size());
	InitSceneObjects();
	InitVertexBuffer();
	glfwWindowHint(GLFW_SAMPLES, 4);
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


