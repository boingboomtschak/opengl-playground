// boids-3d.cpp : Devon McKee
#define _USE_MATH_DEFINES

#include <glad.h>
#define GLFW_INCLUDE_NONE
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
#include "GeomUtils.h"

using std::vector;

GLuint program = 0;
GLuint texUnit = 0;
GLuint planeTexUnit, planeTexName;
GLuint cubeBuffer, planeBuffer;
GLuint cubeIBuffer, planeIBuffer;
GLuint cubeVAO, planeVAO;

vector<const char*> boidObjFilenames = { "./objects/fish/fish1.obj", "./objects/fish/fish2.obj", "./objects/fish/fish3.obj", "./objects/fish/fish4.obj" };
vector<const char*> boidTexFilenames = { "./textures/fish/fish1.tga", "./textures/fish/fish2.tga", "./textures/fish/fish3.tga", "./textures/fish/fish4.tga" };
vector<mat4> boidObjTransforms = { mat4(), mat4(), mat4(), mat4()};
vector<const char*> rockObjFilenames = { "./objects/rock/rock1.obj", "./objects/rock/rock2.obj", "./objects/rock/rock3.obj" };
vector<const char*> rockTexFilenames = { "./textures/rock/rock1.tga", "./textures/rock/rock2.tga", "./textures/rock/rock3.tga" };
vector<const char*> rockNormFilenames = { "./textures/rock/rock1_normal.tga", "./textures/rock/rock2_normal.tga", "./textures/rock/rock3_normal.tga" };
const char* sandTexFilename = "./textures/sand.tga";

int win_width = 800, win_height = 800;

Camera camera((float) win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

vector<dMesh> boid_meshes;
vector<dMesh> rock_meshes;
GLfloat cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} }; GLuint cube_faces[] = { 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 0, 1, 0, 3, 4, 7, 1, 2, 5, 6 };
GLfloat plane_points[][3] = { {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1} }; GLfloat plane_uvs[][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} }; GLuint plane_tris[][3] = { {0, 1, 2}, {2, 3, 0} };

vec3 lightSource = vec3(1, 1, 0);

vec3 XVec(vec3 v, mat4 m) { vec4 x = m * vec4(v); return .1f * vec3(x.x, x.y, x.z); }

const float BOID_SPEED = 0.005f;
const float BOID_SIZE = 0.06f;
const float BOID_SIZE_VARIANCE = 0.02f;
const float ROCK_SIZE = 0.15f;
const float ROCK_SIZE_VARIANCE = 0.075f;
const float BOID_PERCEPTION = 0.3f;
const float WALL_RANGE = 0.2f;
const float ALIGNMENT_WEIGHT = 1.0f;
const float COHESION_WEIGHT = 1.0f;
const float SEPARATION_WEIGHT = 1.0f;

const int STARTING_BOIDS = 150;
const int STARTING_ROCKS = 200;

const bool DRAW_REF_VECTORS = false;

bool running = true;

struct Boid;
struct Rock;
vector<Boid> flock;
vector<Rock> rocks;

struct Boid {
	vec3 p, v; // position and velocity vector
	vector<int> nb, _nb; // neighbors, bucket for different meshed neighbors
	int mesh;
	float size = BOID_SIZE + (rand_float(-1, 1) * BOID_SIZE_VARIANCE);
	Boid(vec3 xyz, vec3 nv) {
		p = xyz; v = nv; mesh = rand() % boidObjFilenames.size();
	}
	void FindNeighbors() {
		for (size_t i = 0; i < flock.size(); i++) {
			float d = dist(p, flock[i].p);
			if (&flock[i] != this && d < BOID_PERCEPTION && d > 0) {
				if (mesh == flock[i].mesh)
					nb.push_back(i);
				else
					_nb.push_back(i);
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
		// Include neighbors without same mesh
		for (size_t i = 0; i < _nb.size(); i++)
			nb.push_back(_nb[i]);
		_nb.clear();
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
		// Steer boids away from walls
		/*
		if (p.x < -1 + WALL_RANGE)
			return vec3(1 / (1 - p.x), 0.0f, 0.0f); // left wall
		if (p.x > 1 - WALL_RANGE)
			return vec3(1 / (-1 - p.x), 0.0f, 0.0f); // right wall
		*/
		if (p.y > 1 - WALL_RANGE)
			return vec3(0.0f, 1 / (-1 - p.y), 0.0f); // top wall
		if (p.y < -1 + WALL_RANGE)
			return vec3(0.0f, 1 / (1 - p.y), 0.0f); // bottom wall
		/*
		if (p.z < -1 + WALL_RANGE)
			return vec3(0.0f, 0.0f, 1 / (1 - p.z)); // front wall
		if (p.z > 1 - WALL_RANGE)
			return vec3(0.0f, 0.0f, 1 / (-1 - p.z)); // back wall
		*/
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
		boid_meshes[mesh].PreDisplay();
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);
		mat4 trans = Translate(p);
		mat4 scale = Scale(size);
		mat4 orient = Orientation(v, vec3(0.0f, 1.0f, 0.0f));
		mat4 modelview = trans * scale * orient;
		boid_meshes[mesh].Display(camera, &modelview);
	}
};

struct Rock {
	vec3 p, r; // pos, rotation
	int mesh;
	float size;
	Rock(vec3 pos) {
		p = pos, mesh = rand() % rockObjFilenames.size(), size = ROCK_SIZE + (rand_float(-1, 1) * ROCK_SIZE_VARIANCE), r = rand_vec3(0, 360);
	}
	void Draw() {
		rock_meshes[mesh].PreDisplay();
		mat4 trans = Translate(p);
		mat4 scale = Scale(size);
		mat4 rot = RotateX(r.x) * RotateY(r.y) * RotateZ(r.z);
		mat4 modelview = trans * scale * rot;
		rock_meshes[mesh].Display(camera, &modelview);
	}
};

const char* vertShader = R"(
	#version 410 core
	in vec3 point;
	in vec2 uv;
	out vec2 vUv;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		gl_Position = persp * modelview * vec4(point, 1);
		vUv = uv;
	}
)";

const char* fragShader = R"(
	#version 410 core
	in vec2 vUv;
	out vec4 pColor;
	uniform sampler2D textureUnit;
	uniform int useTexture = 0;
	void main() {
		pColor = (useTexture == 1) ? texture(textureUnit, vUv) : vec4(1);
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
	if (key == GLFW_KEY_J && action == GLFW_PRESS)
		running = !running;
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

void InitBuffers() {
	// Get attribute locations
	GLint point_id = glGetAttribLocation(program, "point");
	GLint uv_id = glGetAttribLocation(program, "uv");
	// Set up cube VAO/buffer
	glGenVertexArrays(1, &cubeVAO);
	glBindVertexArray(cubeVAO);
	glGenBuffers(1, &cubeBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_points), cube_points, GL_STATIC_DRAW);
	glGenBuffers(1, &cubeIBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_faces), cube_faces, GL_STATIC_DRAW);
	glVertexAttribPointer(point_id, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)sizeof(cube_points));
	glEnableVertexAttribArray(point_id);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	// Set up plane VAO/buffer
	glGenVertexArrays(1, &planeVAO);
	glBindVertexArray(planeVAO);
	glGenBuffers(1, &planeBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, planeBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(plane_points)+sizeof(plane_uvs), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(plane_points), plane_points);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(plane_points), sizeof(plane_uvs), plane_uvs);
	glGenBuffers(1, &planeIBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeIBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_tris), plane_tris, GL_STATIC_DRAW);	
	glVertexAttribPointer(point_id, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)sizeof(cube_points));
	glEnableVertexAttribArray(point_id);
	glVertexAttribPointer(uv_id, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(sizeof(cube_points) + sizeof(plane_points)));
	glEnableVertexAttribArray(uv_id);
	glActiveTexture(GL_TEXTURE0 + planeTexUnit);
	glBindTexture(GL_TEXTURE_2D, planeTexName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	glClearColor(0.3f, 0.3f, 0.4f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	// Draw cube
	glBindVertexArray(cubeVAO);
	glUseProgram(program);
	SetUniform(program, "useTexture", 0);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modelview", camera.modelview);
	//for (int i = 0; i < 6; i++) 
		//glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (GLvoid*)(size_t)(i * 4));
	glDrawElements(GL_LINE_LOOP, sizeof(cube_faces)/sizeof(GLuint), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	PrintGLErrors("Cube");
	// Draw floor
	glUseProgram(program);
	glBindVertexArray(planeVAO);
	SetUniform(program, "useTexture", 1);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modelview", camera.modelview);
	SetUniform(program, "textureUnit", (int)planeTexUnit);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	// Draw other side of floor
	mat4 modelview = camera.modelview * Translate(0, -1, 0) * RotateZ(180.0f) * Translate(0, 1, 0);
	SetUniform(program, "modelview", modelview);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	PrintGLErrors("Floor");
	// Draw boids
	for (size_t i = 0; i < flock.size(); i++) {
		if (running) 
			flock[i].Run();
		flock[i].Draw();
	}
	PrintGLErrors("Boids");
	// Draw rocks
	for (size_t i = 0; i < rocks.size(); i++) {
		rocks[i].Draw();
	}
	PrintGLErrors("Rocks");
	// Draw reference vectors
	if (DRAW_REF_VECTORS) DrawVectors();
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
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "Boids 3D", NULL, NULL);
	if (!window) {
		printf("Failed to create GLFW window!\n");
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(window, 100, 100);
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	if (!(program = LinkProgramViaCode(&vertShader, &fragShader)))
		return 1;
	planeTexUnit = ++texUnit;
	planeTexName = LoadTexture(sandTexFilename, planeTexUnit);
	for (size_t i = 0; i < boidObjFilenames.size(); i++) {
		boid_meshes.push_back(dMesh());
		texUnit++;
		boid_meshes[i].Read((char*)boidObjFilenames[i], (char*)boidTexFilenames[i], texUnit, &boidObjTransforms[i]);
		printf("'%s' : %i vertices, %i normals, %i uvs, %i triangles\n", boidObjFilenames[i], (int)boid_meshes[i].points.size(), (int)boid_meshes[i].normals.size(), (int)boid_meshes[i].uvs.size(), (int)boid_meshes[i].triangles.size());
	}
	for (size_t i = 0; i < rockObjFilenames.size(); i++) {
		rock_meshes.push_back(dMesh());
		texUnit++;
		printf("  Creating and pushing back dMesh...\n");
		rock_meshes[i].Read((char*)rockObjFilenames[i], (char*)rockTexFilenames[i], texUnit);
		printf("'%s' : %i vertices, %i normals, %i uvs, %i triangles\n", rockObjFilenames[i], (int)rock_meshes[i].points.size(), (int)rock_meshes[i].normals.size(), (int)rock_meshes[i].uvs.size(), (int)rock_meshes[i].triangles.size());
	}
	InitSceneObjects();
	InitBuffers();
	glfwSetCursorPosCallback(window, MouseMove);
	glfwSetMouseButtonCallback(window, MouseButton);
	glfwSetScrollCallback(window, MouseWheel);
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, Resize);
	glfwSwapInterval(1);
	printf("GL vendor: %s\n", glGetString(GL_VENDOR));
	printf("GL renderer: %s\n", glGetString(GL_RENDERER));
	printf("GL version: %s\n", glGetString(GL_VERSION));
	printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	while (!glfwWindowShouldClose(window)) {
		Display();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	glDeleteVertexArrays(1, &cubeVAO);	
	glDeleteBuffers(1, &cubeBuffer);
	glDeleteBuffers(1, &cubeIBuffer);
	glDeleteVertexArrays(1, &planeVAO);
	glDeleteBuffers(1, &planeBuffer);
	glDeleteBuffers(1, &planeIBuffer);
	glfwDestroyWindow(window);
	glfwTerminate();
}


