// dlaFractal : Generating diffusion-limited aggregation (DLA) fractals, Devon McKee 2021

#define _USE_MATH_DEFINES

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <OpenGL/gl3.h>
#else
#include <glad.h>
#endif
#include <GLFW/glfw3.h>
#include <time.h>
#include <vector>
#include "Camera.h"
#include "GLXtras.h"
#include "Mesh.h"
#include "Misc.h"
#include "VecMat.h"
#include "CameraControls.h"
#include "dCube.h"
#include "GeomUtils.h"

using std::vector;

GLuint vBuffer = 0;
GLuint program = 0;

int win_width = 800;
int win_height = 800;

Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));
vec3 lightSource = vec3(1, 1, 0);
time_t start;

dCube cube;

vector<vec3> meshPts;
vector<vec3> meshNorms;
vector<vec2> meshUvs;
vector<int3> meshTris;

const char* particleObjFile = "objects/sphere.obj"; mat4 particleMeshTransform = mat4();

const float PARTICLE_SIZE = 0.05f;
const float PARTICLE_RANGE = 1.5 * PARTICLE_SIZE;
const float PARTICLE_SPEED = 0.01f;
const float PARTICLE_SPEED_STEPS = 5;
const int MAX_PARTICLES = 200;

bool simulation = true;
time_t simFinish = 0;
int particleBuffer = MAX_PARTICLES;
int particleDeaths = 0;

vec3 getCol(float n);

struct Particle;
vector<Particle> particlesAlive;
vector<Particle> particlesDead;

struct Particle {
	vec3 p, v, c; // position, color
	bool alive = true;
	Particle(vec3 xyz) {
		p = xyz, v = vec3(1, 0, 0), c = vec3(1);
	}
	void WrapWalls() {
		if (p.x < -1.0f) p.x = 1.0f;
		if (p.x > 1.0f) p.x = -1.0f;
		if (p.y < -1.0f) p.y = 1.0f;
		if (p.y > 1.0f) p.y = -1.0f;
		if (p.z < -1.0f) p.z = 1.0f;
		if (p.z > 1.0f) p.z = -1.0f;
	}
	void Run() {
		if (!alive) return;
		v = rand_dir();
		v *= PARTICLE_SPEED;
		for (int s = 0; s < PARTICLE_SPEED_STEPS; s++) {
			p += v;
			WrapWalls();
			for (size_t i = 0; i < particlesDead.size(); i++) {
				if (dist(particlesDead[i].p, p) <= PARTICLE_RANGE) {
					alive = false;
					break;
				}
			}
			if (!alive) {
				particleBuffer++;
				particleDeaths++;
				c = getCol((float)particleDeaths / 1000.0f);
				break;
			}
		}
	}
	mat4 GetModelview() {
		mat4 trans = Translate(p);
		mat4 scale = Scale(PARTICLE_SIZE);
		mat4 orient = Orientation(v, vec3(0.0f, 1.0f, 0.0f));
		return trans * scale * orient;
	}
};

const char* vertShader = R"(
	#version 410 core
	in vec3 point;
	in vec3 normal;
	out vec3 vPoint;
	out vec3 vNormal;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		vPoint = (modelview * vec4(point, 1)).xyz;
		vNormal = (modelview * vec4(normal, 0)).xyz;
		gl_Position = persp*vec4(vPoint, 1);
	}
)";

const char* fragShader = R"(
	#version 410 core
	in vec3 vPoint;
	in vec3 vNormal;
	out vec4 pColor;
	uniform vec3 light;
	uniform vec3 color;
	void main() {
		vec3 N = normalize(vNormal);       // surface normal
        vec3 L = normalize(light-vPoint);  // light vector
        vec3 E = normalize(vPoint);        // eye vector
        vec3 R = reflect(L, N);            // highlight vector
        float d = abs(dot(N, L));          // two-sided diffuse
        float s = abs(dot(R, E));          // two-sided specular
        float intensity = clamp(d+pow(s, 50), 0, 1);
		pColor = vec4(intensity*color, 1);
	}
)";

vec3 getCol(float n) {
	n *= (2 * M_PI);
	vec3 c;
	c.x = 0.5 * sin(n) + 0.5;
	c.y = 0.5 * sin(n - (2.0f * M_PI / 3.0f)) + 0.5;
	c.z = 0.5 * sin(n - (4.0f * M_PI / 3.0f)) + 0.5;
	return c;
}

void CreateParticles() {
	if (particleBuffer > 0) {
		Particle p = Particle(rand_vec3());
		particlesAlive.push_back(p);
		particleBuffer--;;
	}
}

void CheckSimRunning() {
	if (simulation) {
		for (size_t i = 0; i < particlesDead.size(); i++) {
			if (particlesDead[i].p.x > 1 - PARTICLE_RANGE ||
				particlesDead[i].p.x < -1 + PARTICLE_RANGE ||
				particlesDead[i].p.y > 1 - PARTICLE_RANGE ||
				particlesDead[i].p.y < -1 + PARTICLE_RANGE ||
				particlesDead[i].p.z > 1 - PARTICLE_RANGE ||
				particlesDead[i].p.z < -1 + PARTICLE_RANGE) {
				simulation = false;
			}
		}
	} else if (simFinish == 0.0f && particlesAlive.size() == 0) {
		simFinish = (clock() - start) / CLOCKS_PER_SEC;
	}
}

void PrintSimInfo(float sec) {
	float elapsed = (clock() - start) / CLOCKS_PER_SEC;
	if (elapsed > (sec + 1.0f)) {
		sec = elapsed;
		for (int i = 0; i < 5; i++)
			printf("\033[1A\033[2K");
		printf("-- SIMULATION INFO --\n");
		printf("%i seconds elapsed\n", (int)elapsed);
		printf("%i of %i particles alive\n", MAX_PARTICLES - particleBuffer, MAX_PARTICLES);
		printf("%i particles alive, %i particles dead\n", (int)particlesAlive.size(), (int)particlesDead.size());
		if (simulation) {
			printf("Simulation running...\n");
		} else {
			if (particlesAlive.size() > 0)
				printf("Simulation finishing...\n");
			else
				printf("Simulation completed after: %i seconds\n", (int)simFinish);
		}
	}

}

void InitVertexBuffers() {
	cube.loadBuffer();
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	size_t sizePts = meshPts.size() * sizeof(vec3);
	size_t sizeNorms = meshNorms.size() * sizeof(vec3);
	size_t bufSize = sizePts + sizeNorms;
	glBufferData(GL_ARRAY_BUFFER, bufSize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizePts, &meshPts[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizePts, sizeNorms, &meshNorms[0]);
}

void DelVertexBuffers() {
	cube.unloadBuffer();
}

void Display() {
	glClear(GL_DEPTH_BUFFER_BIT);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	cube.display(camera);
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	VertexAttribPointer(program, "normal", 3, 0, (void*)(meshPts.size() * sizeof(vec3)));
	SetUniform(program, "light", lightSource);
	SetUniform(program, "persp", camera.persp);
	for (size_t i = 0; i < particlesAlive.size(); i++) {
		particlesAlive[i].Run();
		if (particlesAlive[i].alive) {
			mat4 m = particlesAlive[i].GetModelview();
			SetUniform(program, "modelview", camera.modelview * m);
			SetUniform(program, "color", particlesAlive[i].c);
			glDrawElements(GL_TRIANGLES, 3 * meshTris.size(), GL_UNSIGNED_INT, &meshTris[0]);
		} else {
			particlesDead.push_back(particlesAlive[i]);
			particlesAlive.erase(particlesAlive.begin() + i);
			i--;
		}
	}
	for (size_t i = 0; i < particlesDead.size(); i++) {
		mat4 m = particlesDead[i].GetModelview();
		SetUniform(program, "modelview", camera.modelview * m);
		SetUniform(program, "color", particlesDead[i].c);
		glDrawElements(GL_TRIANGLES, 3 * meshTris.size(), GL_UNSIGNED_INT, &meshTris[0]);	
	}
	glFlush();
}

int main() {
	srand((int)time(NULL));
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "DLA Fractal", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwSetWindowPos(window, 100, 100);
	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	if (!(program = LinkProgramViaCode(&vertShader, &fragShader)))
		return 1;
	if (!ReadAsciiObj(particleObjFile, meshPts, meshTris, &meshNorms, &meshUvs)) {
		fprintf(stderr, "err: Failed to read obj '%s'\n", particleObjFile);
		return 1;
	}
	printf("'%s' : %i vertices, %i normals, %i triangles\n", particleObjFile, (int)meshPts.size(), (int)meshNorms.size(), (int)meshTris.size());
	InitVertexBuffers();
	InitializeCallbacks(window);
	glfwSwapInterval(1);
	// Create "seed" particle
	Particle p = Particle(vec3(0, 0, 0));
	p.alive = false;
	p.c = getCol(particleDeaths / MAX_PARTICLES);
	particlesDead.push_back(p);
	start = clock();
	float lastSec = 0;
	while (!glfwWindowShouldClose(window)) {
		if (simulation) CreateParticles();
		Display();
		glfwPollEvents();
		glfwSwapBuffers(window);
		CheckSimRunning();
		PrintSimInfo(lastSec);
		//frame++;
	}
	DelVertexBuffers();
	glfwDestroyWindow(window);
	glfwTerminate();
}