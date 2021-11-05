// plantGrowth.cpp : Devon McKee

#define _USE_MATH_DEFINES

#include <glad.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include "VecMat.h"
#include "Camera.h"
#include "GLXtras.h"

using std::vector;
using std::min;
using std::max;

GLuint cubeBuffer = 0, plantBuffer = 0;
GLuint program = 0;

int win_width = 800, win_height = 800;
Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

float cube_points[][3] = { {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1} }; int cube_faces[][4] = { {0, 1, 2, 3}, {2, 3, 4, 5}, {4, 5, 6, 7}, {6, 7, 0, 1}, {0, 3, 4, 7}, {1, 2, 5, 6} };

const float GROWTH_INTERVAL = 0.05f;
const float STALK_WIDTH = 0.05f;
const int SAMPLE_NUM = 20;

float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX / (max - min)); }
float dist(vec3 p1, vec3 p2) { return (float)sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2) + pow(p2.z - p1.z, 2)); }
float dist_to_segment(vec3 p, vec3 v, vec3 w) {
	float l = dist(v, w);
	if (l == 0.0) return dist(p, v);
	float t = max(0.0f, min(1.0f, dot(p-v, w-v)));
	vec3 proj = v + t * (w - v);
	return dist(p, proj);
}
bool point_segment_left(vec3 p, vec3 v, vec3 w) { return ((w.x - v.x) * (p.y - v.y) - (w.y - v.y) * (p.x - v.x)) > 0; }

const char* vertShader = R"(
	#version 130
	in vec3 point;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		gl_Position = persp * modelview * vec4(point, 1);
	}
)";

const char* fragShader = R"(
	#version 130
	out vec4 pColor;
	void main() {
		pColor = vec4(1);
	}
)";

// --- Util functions ---

vec3 Centroid(vector<vec3> points) {
	vec3 c = vec3(0.0f);
	for (size_t i = 0; i < points.size(); i++) {
		c += points[i];
	}
	c /= (float)points.size();
	return c;
}

vector<vec3> SampleCircle(vec3 center) {
	vector<vec3> v;
	for (int i = 0; i < SAMPLE_NUM; i++) {
		float r = STALK_WIDTH * (float)sqrt(rand_float());
		float theta = rand_float(0.0f, 2*M_PI);
		float x = center.x + r * cos(theta);
		float y = center.y + r * sin(theta);
		v.push_back(vec3(x, y, center.z));
	}
	return v;
}

vector<vec3> FindHull(vector<vec3> points, vec3 p, vec3 q) {
	vector<vec3> hull;
	// Return if no more points
	if (points.size() == 0) {
		return points;
	}
	// Find furthest point from PQ as M and add to hull
	float m_dist = 0;
	vec3 m;
	for (size_t i = 0; i < points.size(); i++) {
		float d = dist_to_segment(points[i], p, q);
		if (d > m_dist) {
			m_dist = d, m = points[i];
		}
	}
	hull.push_back(m);
	// Partition points to left of PM and to left of MQ
	vector<vec3> lpm, lmq;
	for (size_t i = 0; i < points.size(); i++) {
		if (&(points[i]) != &m) {
			if (point_segment_left(points[i], p, m)) {
				lpm.push_back(points[i]);
			} else if (point_segment_left(points[i], m, q)) {
				lmq.push_back(points[i]);
			}
		}
	}
	// Recursively call FindHull to find rest of hull from remaining points
	// Add returned points from recursive calls to hull
	vector<vec3> nhull;
	nhull = FindHull(lpm, p, m);
	hull.insert(hull.end(), nhull.begin(), nhull.end());
	nhull = FindHull(lmq, m, q);
	hull.insert(hull.end(), nhull.begin(), nhull.end());
	return hull;
}

vector<vec3> QuickHull(vector<vec3> points) {
	vector<vec3> hull;
	vec3 c = Centroid(points);
	// Find left and rightmost points
	vec3 l = c, r = c;
	for (size_t i = 0; i < points.size(); i++) {
		if (points[i].x < l.x)
			l = points[i];
		if (points[i].x > r.x)
			r = points[i];
	}
	hull.push_back(l); 
	hull.push_back(r);
	// Divide set into above and below line
	vector<vec3> above;
	vector<vec3> below;
	for (size_t i = 0; i < points.size(); i++) {
		if (point_segment_left(points[i], l, r) && &(points[i]) != &l && &(points[i]) != &r)
			above.push_back(points[i]);
		else
			below.push_back(points[i]);
	}
	// Find above and below hull points with FindHull
	vector<vec3> ahull = FindHull(above, l, r);
	vector<vec3> bhull = FindHull(below, r, l);
	// Attach above and below hulls to total hull
	hull.insert(hull.end(), ahull.begin(), ahull.end());
	hull.insert(hull.end(), bhull.begin(), bhull.end());
	return hull;
}

void PrintVectorVec3(vector<vec3> v) {
	for (size_t i = 0; i < v.size(); i++)
		printf("{%f, %f, %f} ", v[i].x, v[i].y, v[i].z);
	printf("\n");
}

// --- Callbacks ---

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

// --- Main functions ---

void InitVertexBuffer() {
	// Initializing buffer for cube
	glGenBuffers(1, &cubeBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_points), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cube_points), cube_points);
	// Initializing buffer for plant
	glGenBuffers(1, &plantBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, plantBuffer);
}

void Display() {
	glClear(GL_DEPTH_BUFFER_BIT);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	// Draw cube
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, cubeBuffer);
	VertexAttribPointer(program, "point", 3, 0, (void*)0);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "modelview", camera.modelview);
	for (int i = 0; i < 6; i++) {
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, cube_faces[i]);
	}
	// Draw plant

	glFlush();
}

int main() {
	srand((int)time(NULL));
	//
	vector<vec3> p = SampleCircle(vec3(0.0f));
	for (size_t i = 0; i < p.size(); i++)
		printf("(%f, %f), ", p[i].x, p[i].y);
	printf("\n");
	p = QuickHull(p);
	printf("hull points\n");
	for (size_t i = 0; i < p.size(); i++)
		printf("(%f, %f), ", p[i].x, p[i].y);
	printf("\n");
	//
	if (!glfwInit())
		return 1;
	GLFWwindow* window = glfwCreateWindow(win_width, win_height, "plantGrowth", NULL, NULL);
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
	glDeleteBuffers(1, &cubeBuffer);
	glDeleteBuffers(1, &plantBuffer);
	glfwDestroyWindow(window);
	glfwTerminate();
}