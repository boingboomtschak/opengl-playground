// dlaFractal : Generating diffusion-limited aggregation (DLA) fractals, Devon McKee 2021

#define _USE_MATH_DEFINES

#include <glad.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"
#include "Misc.h"
#include "GLXtras.h"
#include "GeomUtils.h"
#include "CameraControls.h"
#include "dCube.h"
#include "dMesh.h"

using std::vector;

int win_width = 800;
int win_height = 800;

Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));
vec3 lightSource = vec3(1, 1, 0);
time_t start;

dCube cube;
dMesh particleMesh;

const char* particleObjFile = "objects/sphere.obj"; mat4 particleMeshTransform = mat4();

const float PARTICLE_SIZE = 0.03f;
const float PARTICLE_RANGE = 0.03f;
const float PARTICLE_SPEED = 0.01f;
const float PARTICLE_SPEED_STEPS = 5;
const int MAX_PARTICLES = 200;

bool simulation = true;
int particleBuffer = MAX_PARTICLES;

struct Particle;
vector<Particle> particlesAlive;
vector<Particle> particlesDead;

struct Particle {
	vec3 p, v; // position, color
	bool alive = true;
	Particle(vec3 xyz) {
		p = xyz, v = vec3(1, 0, 0);
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
				break;
			}
		}
	}
	void Draw() {
		mat4 trans = Translate(p);
		mat4 scale = Scale(PARTICLE_SIZE);
		mat4 orient = Orientation(v, vec3(0.0f, 1.0f, 0.0f));
		mat4 modelview = trans * scale * orient;
		particleMesh.Display(camera, &modelview);
	}
};

void CreateParticles() {
	if (particleBuffer > 0) {
		Particle p = Particle(rand_vec3());
		particlesAlive.push_back(p);
		particleBuffer--;
	}
}

void CheckSimRunning() {
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
}

void PrintSimInfo(float sec) {
	float elapsed = (clock() - start) / CLOCKS_PER_SEC;
	if (elapsed > (sec + 1.0f)) {
		sec = elapsed;
		for (int i = 0; i < 4; i++)
			printf("\033[1A\033[2K");
		printf("-- SIMULATION INFO --\n");
		printf("%.2f seconds elapsed\n", elapsed);
		printf("%i of %i particles alive\n", MAX_PARTICLES-particleBuffer, MAX_PARTICLES);
		printf("%i particles alive, %i particles dead\n", (int)particlesAlive.size(), (int)particlesDead.size());
	}

}

void InitVertexBuffers() {
	cube.loadBuffer();
}

void DelVertexBuffers() {
	cube.unloadBuffer();
}

void Display() {
	glClear(GL_DEPTH_BUFFER_BIT);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	cube.display(camera);
	for (size_t i = 0; i < particlesAlive.size(); i++) {
		particlesAlive[i].Run();
		if (particlesAlive[i].alive) {
			particlesAlive[i].Draw();
		} else {
			particlesDead.push_back(particlesAlive[i]);
			particlesAlive.erase(particlesAlive.begin() + i);
			i--;
		}
	}
	for (size_t i = 0; i < particlesDead.size(); i++) {
		particlesDead[i].Draw();
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
	particleMesh.Read((char*)particleObjFile, &particleMeshTransform);
	printf("'%s' : %i vertices, %i normals, %i triangles\n", particleObjFile, (int)particleMesh.points.size(), (int)particleMesh.normals.size(), (int)particleMesh.triangles.size());
	InitVertexBuffers();
	InitializeCallbacks(window);
	glfwSwapInterval(1);
	// Create "seed" particle
	Particle p = Particle(vec3(0, 0, 0));
	p.alive = false;
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