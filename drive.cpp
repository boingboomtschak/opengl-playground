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
#include "GeomUtils.h"
#include "dRenderPass.h"
#include "dMesh.h"
#include "dMisc.h"
#include "dSkybox.h"
#include "dParticles.h"
#include "dTextureDebug.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/implot.h"

using std::vector;
using std::string;
using std::runtime_error;
using time_p = std::chrono::system_clock::time_point;
using sys_clock = std::chrono::system_clock;
using double_ms = std::chrono::duration<double, std::milli>;

GLFWwindow* window;
int windowed_width = 1000, windowed_height = 800;
int win_width = windowed_width, win_height = windowed_height;
GLFWmonitor* monitor;
const GLFWvidmode* videoModes;
GLFWvidmode currentVidMode;
int videoModesCount;
bool fullscreen = false;
float dt;

const int SHADOW_DIM = 16384;
dRenderPass mainPass;
dRenderPass mainPassInst;
dRenderPass shadowPass;
dRenderPass shadowPassInst;
GLuint shadowFramebuffer = 0;
GLuint shadowTexture = 0;

float lightColor[3] = { 1.0f, 1.0f, 1.0f };
float init_time;

const int PERF_MEMORY = 50;
static bool showPerformance = false;
float fps_x[PERF_MEMORY] = { 0.0f };
float fps_y[PERF_MEMORY] = { 0.0f };
float fps_shade[PERF_MEMORY] = { 0.0f };

const char* shadowVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	uniform mat4 depth_vp;
	uniform mat4 model;
	uniform mat4 transform;
	void main() {
		gl_Position = depth_vp * transform * model * vec4(point, 1);
	}
)";

const char* shadowVertInstanced = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	layout(location = 3) in mat4 transform;
	uniform mat4 depth_vp;
	uniform mat4 model;
	void main() {
		gl_Position = depth_vp * transform * model * vec4(point, 1);
	}
)";

const char* shadowFrag = R"(
	#version 410 core
	void main() {}
)";

const char* mainVert = R"(
	#version 410 core
	layout(location = 0) in vec3 point;
	layout(location = 1) in vec2 uv;
	out vec2 vUv;
	out vec4 shadowCoord;
    uniform mat4 model;
	uniform mat4 transform;
	uniform mat4 view;
	uniform mat4 persp;
	uniform mat4 depth_vp;
	void main() {
		shadowCoord = depth_vp * transform * model * vec4(point, 1);
		vUv = uv;
		gl_Position = persp * view * transform * model * vec4(point, 1);
	}
)";

const char* mainVertInstanced = R"(
    #version 410 core
    layout (location = 0) in vec3 point;
    layout (location = 1) in vec2 uv;
    layout (location = 3) in mat4 transform;
    out vec2 vUv;
    out vec4 shadowCoord;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 persp;
    uniform mat4 depth_vp;
    void main() {
        shadowCoord = depth_vp * transform * model * vec4(point, 1);
        vUv = uv;
        gl_Position = persp * view * transform * model * vec4(point, 1);
    }
)";

const char* mainFrag = R"(
	#version 410 core
	in vec2 vUv;
	in vec4 shadowCoord;
	out vec4 pColor;
	uniform sampler2D txtr;
	uniform sampler2DShadow shadow;
	uniform vec4 ambient = vec4(vec3(0.1), 1);
	uniform vec3 lightColor;
	vec2 uniformSamples[16] = vec2[](vec2(-0.16696604, -0.09312990), vec2(0.53086904, 0.58433708), vec2(0.78145449, -0.86848999), vec2(0.74769790, 0.07194131), vec2(-0.09741326, 0.74185956), vec2(0.32706693, -0.03813042), vec2(0.73635845, 0.86372260), vec2(-0.27280913, 0.54069966), vec2(0.01584532, 0.26754421), vec2(-0.68606618, 0.53619244), vec2(-0.33333386, 0.35455430), vec2(-0.43123940, -0.60522600), vec2(0.03626988, -0.22807865), vec2(-0.53173498, 0.54256439), vec2(-0.65302623, 0.75253209), vec2(0.07282969, 0.19763551));
	float random(vec4 seed) {
		float dot_product = dot(seed, vec4(12.9898,78.233,45.164,94.673));
		return fract(sin(dot_product) * 43758.5453);
	}
    float calcShadow(vec4 shadow_coord) {
        // Perspective division and transforming shadow coord from [-1, 1] to [0, 1]
        vec3 coord = shadow_coord.xyz / shadow_coord.w;
        coord = coord * 0.5 + 0.5;
        // Calculating total texel size and value at shadow
		vec2 texelSize = 1.0 / textureSize(shadow, 0);
        float shadowVal = texture(shadow, vec3(coord.xy, coord.z)) == 0.0f ? 0.4f : 1.0f;
        // Early bailing on extra sampling if nearby values are the same (not on shadow edge)
        bool different = false;
        for (int x = -1; x <= 1; x += 2) {
            for (int y = -1; y <= 1; y += 2) {
                float diffVal = texture(shadow, vec3(coord.xy + vec2(x, y) * texelSize, coord.z)) == 0.0f ? 0.4f : 1.0f;
                if (diffVal != shadowVal) different = true;
            }
        }
        if (!different) return shadowVal == 1.0f ? 1.0f : shadowVal / 5.0;
        // If on shadow edge, sample using nearby precalculated uniform random coordinates
		for (int i = 0; i < 16; i++) {
			int ind = int(float(16)*random(vec4(gl_FragCoord.xyy, i))) % 16;
			shadowVal += texture(shadow, vec3(coord.xy + uniformSamples[ind] * texelSize, coord.z)) == 0.0f ? 0.4f : 1.0f;
		}
        return shadowVal / 21.0f;
    }
	void main() {
        pColor = ambient + texture(txtr, vUv) * vec4(lightColor, 1) * calcShadow(shadowCoord);
	}
)";

vector<dSkybox> skyboxes;
vector<string> skyboxPaths {
	"textures/skybox/maskonaive/",
	"textures/skybox/classic-land/",
	"textures/skybox/empty-space/",
	"textures/skybox/dusk-ocean/"
};
vector<string> skyboxNames {
	"Maskonaive",
	"Classic Land",
	"Empty Space",
	"Dusk Ocean"
};
int cur_skybox = 0;

dParticles particleSystem;

struct Camera {
	float fov = 60;
	float zNear = 1.0f;
	float zFar = 120.0f;
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
		persp = Perspective(fov, (float)width / (float)height, zNear, zFar);
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
		persp = Perspective(fov, (float)win_width / (float)win_height, zNear, zFar);
	}
	void resize(int _width, int _height) {
		width = _width;
		height = _height;
		persp = Perspective(fov, (float)width / (float)height, zNear, zFar);
	}
} camera;

int camera_loc = 1;
// 1 - Third person chase camera
// 2 - Top-down camera of arena
// 3 - Hood camera
// 4 - Frozen free camera, moved right behind car

dMesh floor_mesh;
vector<vec3> floor_points = { {-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1} };
vector<vec3> floor_normals = { {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0} };
vector<vec2> floor_uvs = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
vector<int3> floor_triangles = { {2, 1, 0}, {0, 3, 2} };

dMesh large_tree_mesh;
vector<vec3> large_tree_instance_positions {
	{49.00, 0, 37.00}, {50.00, 0, 32.00}, {29.00, 0, 15.00}, {50.00, 0, 10.00},
	{-10.00, 0, 32.00}, {-34.00, 0, 7.00}, {15.00, 0, -13.00}, {6.00, 0, -15.00},
	{-0.75, 0, 0.3}, {-9.11, 0, -8.64}, {-16.16, 0, -12.58}, {-27.02, 0, -15.88}, 
	{-32.39, 0, -7.18}, {-22.69, 0, -24.82}, {-15.40, 0, -31.95}, {-8.23, 0, -33.90}, 
	{-7.48, 0, -48.32}, {7.70, 0, -34.07}, {49.83, 0, -22.92}, {54.68, 0, -30.48}, 
	{52.80, 0, -38.50}, {46.54, 0, -49.25}, {36.14, 0, -50.60}, {26.13, 0, -51.73}, 
	{10.83, 0, -51.97}, {-13.78, 0, -33.34}, {-25.02, 0, -24.78}, {-31.62, 0, -16.71}, 
	{-29.06, 0, -6.59}, {7.55, 0, 10.67}, {25.84, 0, 21.15}, {23.49, 0, 34.07}, 
	{7.52, 0, 46.78}, {-2.10, 0, 54.42}, {-12.51, 0, 54.03}, {-21.23, 0, 54.79}, 
	{-32.39, 0, 54.86}, {-37.58, 0, 46.49}, {-36.75, 0, 35.87}, {-41.17, 0, 30.20}, 
	{-52.15, 0, 31.90}, {-57.76, 0, 14.15}, {-58.13, 0, 3.82}, {-58.32, 0, -4.81}, 
	{-58.50, 0, -15.75}, {-58.31, 0, -26.44}, {-58.05, 0, -38.86}, {-57.40, 0, -49.04},
	{-34.08, 0, -27.36}, {-35.94, 0, -16.43}, {-31.61, 0, -12.72}, {-22.11, 0, -10.64}, 
	{-11.82, 0, -11.64}, {-10.84, 0, -21.56}, {-17.93, 0, -28.87}, {-25.40, 0, -33.24}, 
	{-34.90, 0, -30.52}, {-8.35, 0, 9.46}, {-7.34, 0, 21.43}, {-7.45, 0, 35.74}, 
	{-16.61, 0, 39.66}, {7.60, 0, 16.11}, {12.85, 0, 11.31}, {23.16, 0, 8.94}, 
	{23.09, 0, 17.21}, {33.01, 0, 7.78}, {31.65, 0, 19.08}, {30.85, 0, 25.40}, 
	{35.05, 0, 33.10}, {29.93, 0, 34.80}, {22.32, 0, 41.41}, {6.87, 0, 36.39}, 
	{6.92, 0, 55.30}, {12.63, 0, 57.65}, {22.51, 0, 57.53}, {29.38, 0, 54.41}, 
	{41.66, 0, 56.19}, {51.12, 0, 53.26}, {55.26, 0, 46.08}, {50.39, 0, 42.98}, 
	{53.33, 0, 37.18}, {56.60, 0, 29.23}, {50.68, 0, 23.18}, {47.43, 0, 17.28}, 
	{53.80, 0, 13.17}, {57.79, 0, 2.03}, {53.03, 0, -7.96}, {54.85, 0, -20.03}, 
	{53.67, 0, -44.20}, {35.25, 0, -35.85}, {25.01, 0, 17.04}, {27.80, 0, 18.07}, 
	{30.01, 0, 21.32}, {27.01, 0, 24.33},
};
dMesh grass_mesh;
vector<vec3> grass_instance_positions {
	{7.79, 0, -5.38}, {5.27, 0, -8.41}, {-6.32, 0, -8.58}, {-9.40, 0, -5.62}, 
	{-9.49, 0, 5.47}, {-6.46, 0, 8.56}, {4.59, 0, 9.61}, {7.51, 0, 6.12},
	{12.07, 0, 5.49}, {15.67, 0, 5.87}, {20.84, 0, 5.78}, {23.66, 0, 10.43},
	{16.71, 0, 11.87}, {-7.01, 0, 17.79}, {-7.03, 0, 27.66}, {-7.15, 0, 37.66},
	{-18.52, 0, 41.08}, {-21.48, 0, 34.07}, {-20.76, 0, 22.88}, {-30.34, 0, 15.04},
	{-36.73, 0, 14.27}, {-43.83, 0, 14.90}, {-44.92, 0, 9.24}, {-35.86, 0, -6.22},
	{-25.08, 0, -8.68}, {-14.91, 0, -15.59}, {-10.73, 0, -23.47}, {-11.57, 0, -30.97},
	{-20.33, 0, -33.76}, {-31.8, 0, -28.42}, {13.06, 0, -5.47}, {20.72, 0, -5.45}, 
	{31.65, 0, -4.70}, {35.97, 0, -33.82}, {25.40, 0, -36.43}, {9.04, 0, 8.93},
	{7.82, 0, -36.36}, {5.39, 0, -23.72}, {-9.14, 0, -11.88}, {-20.57, 0, -12.46}, 
	{-32.20, 0, -20.17}, {-20.66, 0, -26.84}, {4.43, 0, -47.64}, {3.93, 0, -53.39}, 
	{0.33, 0, -57.04}, {-7.27, 0, -51.45}, {-10.98, 0, -48.05}, {16.12, 0, -50.30}, 
	{30.95, 0, -49.04}, {45.22, 0, -46.89}, {49.36, 0, -38.55}, {50.14, 0, -25.47}, 
	{51.44, 0, -17.93}, {48.32, 0, -10.58}, {52.20, 0, -5.34}, {55.19, 0, 2.71}, 
	{53.23, 0, 10.13}, {48.65, 0, 19.89}, {53.20, 0, 29.25}, {52.94, 0, 39.64}, 
	{40.97, 0, 53.80}, {33.35, 0, 55.97}, {22.45, 0, 55.02}, {12.37, 0, 52.92}, 
	{4.89, 0, 51.58}, {7.12, 0, 44.37}, {8.70, 0, 35.15}, {8.79, 0, 15.86}, 
	{8.65, 0, -8.66}, {5.71, 0, -19.49}, {-7.94, 0, -18.39}, {-12.41, 0, -13.37}, 
	{-18.98, 0, -6.11}, {-23.53, 0, -13.22}, {-18.43, 0, -21.01}, {-12.61, 0, -26.95}, 
	{-20.56, 0, -31.51}, {-25.20, 0, -27.63}, {-26.66, 0, -18.66}, {-28.89, 0, -11.67}, 
	{-35.18, 0, -10.46}, {-35.20, 0, -24.29}, {-42.69, 0, 6.82}, {-33.15, 0, 27.00}, 
	{-33.92, 0, 33.73}, {-33.91, 0, 43.06}, {-33.89, 0, 49.01}, {-37.87, 0, 51.80}, 
	{-41.86, 0, 46.81}, {-39.06, 0, 37.08}, {-37.24, 0, 30.39}, {-45.18, 0, 26.06}, 
	{-52.10, 0, 28.20}, {-51.13, 0, 35.53}, {-48.92, 0, 42.02}, {-44.19, 0, 49.47}, 
	{-48.15, 0, 51.39}, {-52.47, 0, 45.00}, {-19.89, 0, 37.80}, {-8.92, 0, 40.31}, 
	{6.92, 0, 40.09}, {21.51, 0, 40.12}, {31.11, 0, 42.29}, {34.82, 0, 36.45}, 
	{35.20, 0, 30.16}, {34.46, 0, 21.91}, {33.01, 0, 14.88}, {31.73, 0, 7.26}, 
	{18.89, 0, 8.64}, {11.94, 0, 10.72}, {6.14, 0, 13.45}, {19.11, 0, 15.25}, 
	{25.39, 0, 18.05}, {27.88, 0, 24.53}, {24.49, 0, 30.18}, {-15.33, 0, -9.27}, 
	{-17.74, 0, -15.42}, {-21.91, 0, -19.81}, {-28.09, 0, -25.55}, {-35.06, 0, -18.19}, 
	{-31.77, 0, -11.53}, {-17.96, 0, -8.67}, {-7.88, 0, -29.60}, {-14.56, 0, -29.49}, 
	{-16.12, 0, -34.21}, {-21.71, 0, 28.06}, {6.18, 0, 37.26}, {7.01, 0, 49.03}, 
	{10.97, 0, 55.62}, {17.51, 0, 56.45}, {49.75, 0, 49.19}, {53.50, 0, 33.92}, 
	{49.00, 0, 25.35}, {53.92, 0, 22.33}, {49.92, 0, 17.05}, {53.79, 0, 16.88}, 
	{56.55, 0, 13.86}, {56.15, 0, 8.20}, {50.05, 0, 6.49}, {33.86, 0, 10.72}, 
	{26.73, 0, 12.74}, {23.75, 0, 14.55}, {29.74, 0, 18.33}, {28.37, 0, 7.24}, 
	{34.98, 0, -7.36}, {26.29, 0, -5.06}, {48.05, 0, -21.12}, {52.87, 0, -23.11},
	{54.21, 0, -27.11}, {49.96, 0, -33.09}, {49.25, 0, -43.09}, {49.27, 0, -49.34}, 
	{43.54, 0, -53.94}, {34.86, 0, -55.65}, {31.18, 0, -54.53}, {23.49, 0, -47.76}, 
	{19.93, 0, -52.79}, {16.68, 0, -57.96}, {11.80, 0, -57.26}, {9.42, 0, -53.46},
	{10.55, 0, -47.26}, {16.66, 0, -47.86}, {7.30, 0, -56.81}, {0.79, 0, -55.33}, 
	{-6.02, 0, -54.50}, {-11.82, 0, -57.15}, {-22.68, 0, -58.74}, {-30.38, 0, -57.70},
	{-40.51, 0, -57.49}, {-53.80, 0, -57.32}, {-57.84, 0, -53.29}, {-58.09, 0, -44.97}, 
	{-58.10, 0, -32.31}, {-57.77, 0, -20.81}, {-57.48, 0, -11.24}, {-57.76, 0, -0.54}, 
	{-58.23, 0, 8.00}, {-57.65, 0, 20.25}, {-56.34, 0, 27.91}, {-12.13, 0, -7.25}, 
	{-10.14, 0, -17.27}, {-13.09, 0, -21.82}, {-17.47, 0, -25.97}, {-23.17, 0, -31.02}, 
	{-31.75, 0, -32.83}, {-31.41, 0, -24.12}, {-25.43, 0, -21.01}, {-19.74, 0, -23.85}, 
	{-16.54, 0, -25.51}, {-7.47, 0, -25.94}, {-9.09, 0, -20.63}, {-20.19, 0, -17.30},
	{-30.55, 0, -8.13}, {-24.77, 0, -6.63}, {-13.41, 0, -7.41}, {12.13, 0, -7.70}, 
	{48.80, 0, -29.86}, {54.54, 0, -13.45}, {55.77, 0, -6.18}, {57.66, 0, -15.36}, 
	{54.96, 0, -34.84}, {52.84, 0, -46.37}, {50.84, 0, -54.69}, {37.50, 0, -53.79}, 
	{30.17, 0, -50.93}, {25.60, 0, -55.13}, {20.31, 0, -56.20}, {15.10, 0, -53.24}, 
	{10.99, 0, -50.25}, {-0.23, 0, -53.39}, {-4.14, 0, -56.11}, {-11.35, 0, -52.83}, 
	{-33.86, 0, -35.34}, {-36.90, 0, -22.12}, {-37.35, 0, -10.74}, {-21.66, 0, 24.40}, 
	{-21.72, 0, 39.58}, {-31.84, 0, 51.97}, {-36.49, 0, 54.49}, {-34.69, 0, 56.44}, 
	{-27.99, 0, 54.64}, {-20.92, 0, 53.10}, {-16.84, 0, 56.33}, {-12.06, 0, 57.21}, 
	{-9.39, 0, 53.56}, {-4.92, 0, 54.34}, {2.20, 0, 56.54}, {8.00, 0, 52.92}, 
	{9.49, 0, 57.30}, {15.74, 0, 54.92}, {24.05, 0, 41.10}, {25.94, 0, 36.21}, 
	{27.32, 0, 32.39}, {29.15, 0, 27.36}, {32.31, 0, 21.46}, {35.16, 0, 24.48}, 
	{32.21, 0, 32.18}, {32.75, 0, 36.64}, {30.44, 0, 40.23}, {23.81, 0, 41.49}, 
	{19.42, 0, 37.20}
};

dMesh campfire_mesh;
dMesh sleeping_bag_mesh;

struct Car {
	dMesh mesh;
	float mass = 0.0; // mass of car
	float engine = 0.0; // engine force
	float roll = 0.0; // rolling resistance
	float drag = 0.0; // aerodynamic drag constant
	float last_pt = 0.0f;
	vec3 pos; // position
	vec3 dir; // direction
	vec3 vel; // velocity
	Car() { };
	Car(dMesh _mesh, float _mass, float _engine, float _roll, float _drag) {
		mesh = _mesh;
		mass = _mass;
		engine = _engine;
		roll = _roll;
		drag = _drag;
		pos = vec3(0, 0, 0);
		dir = vec3(1, 0, 0);
		vel = vec3(0, 0, 0);
	}
	mat4 transform() {
		return Translate(pos) * Orientation(dir, vec3(0, 1, 0));
	}
	void collide() {
		if (pos.y < 0) pos.y = 0;
		if (pos.x > 59.5) { pos.x = 59.5; }
		else if (pos.x < -59.5) { pos.x = -59.5; }
		else if (pos.z > 59.5) { pos.z = 59.5; }
		else if (pos.z < -59.5) { pos.z = -59.5; }
		/*for (vec3 tree_pos : large_tree_positions) {
			float d = dist(pos, tree_pos);
			if (d < 1.5) vel *= 0.5;
			//if (d < 1.0) vel = (pos - tree_pos); hard collision with tree
		}*/
	}
	void update(float dt) {
		// Weight update by time delta for consistent effect of updates
		//   regardless of framerate
		last_pt += dt;
		// Check if space key pressed ("drifting")
		float drift = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) ? 0.5 : 1.0;
		// Check for car turning with A/D
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			float deg = dt * -1.5 * ((drift == 0.5) ? 1.25 : 1.0);
			vec4 dirw = RotateY(deg) * dir;
			dir = vec3(dirw.x, dirw.y, dirw.z);
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			float deg = dt * 1.5 * ((drift == 0.5) ? 1.25 : 1.0);
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
		vec3 acc = force / mass;
		// Using last_pt to limit particles by dt
		if (last_pt > 0.5f) {
			last_pt = 0.0f;
			// Spawn drifting particles if roll wt and speed higher than threshold
			if (length(vel) > 0.16f && roll_wt > 1.6) {
				particleSystem.createParticle(pos - 0.3 * cross(dir, vec3(0, 1, 0)) - 0.5 * dir, vec3(1, 0.1, 0.1));
				particleSystem.createParticle(pos + 0.3 * cross(dir, vec3(0, 1, 0)) - 0.5 * dir, vec3(1, 0.1, 0.1));
			}
		}
		// Move velocity according to acceleration
		vel += dt * acc;
		// Move position according to velocity
		pos += dt * vel;
	}
} car;

void WindowResized(GLFWwindow* window, int _width, int _height) {
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	win_width = width;
	win_height = height;
	camera.resize(width, height);
	glViewport(0, 0, win_width, win_height);
}

void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_Q && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		car.pos = vec3(2, 0, 0);
		car.vel = vec3(0, 0, 0);
	}
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            showPerformance = !showPerformance;
        } else {
            printf("{%.2f, %.0f, %.2f}, ", car.pos.x, car.pos.y, car.pos.z);
        }
    }
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
		camera.adjustFov(70);
    }
	if (key == GLFW_KEY_BACKSLASH && action == GLFW_PRESS) {
		cur_skybox++;
		if (cur_skybox >= skyboxes.size()) cur_skybox = 0;
	}
	if (key == GLFW_KEY_F && action == GLFW_PRESS) {
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

void load_icon() {
	GLFWimage icon[2];
	icon[0].pixels = stbi_load("textures/icon/drive.png", &icon[0].width, &icon[0].height, 0, 4);
	icon[1].pixels = stbi_load("textures/icon/drive_small.png", &icon[1].width, &icon[1].height, 0, 4);
	glfwSetWindowIcon(window, 2, icon);
	stbi_image_free(icon[0].pixels);
	stbi_image_free(icon[1].pixels);
}

void update_title(time_p cur) {
	static time_p lastTitleUpdate = sys_clock::now();
	double_ms sinceTitleUpdate = cur - lastTitleUpdate;
    if (sinceTitleUpdate.count() < 200.0f) return;
    char title[30];
    snprintf(title, 30, "Drive: %.0f fps", (1.0f / dt) * 60.0f);
    glfwSetWindowTitle(window, title);
    lastTitleUpdate = cur;
}

void init_perf() {
    for (int i = 0; i < PERF_MEMORY; i++)
        fps_x[i] = (float)i;
}

void collect_perf(time_p cur) {
    static time_p lastPerfCollect = sys_clock::now();
    double_ms sincePerfCollect = cur - lastPerfCollect;
    if (sincePerfCollect.count() < 500.0f) return;
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 1; i < PERF_MEMORY; i++) {
        fps_y[i - 1] = fps_y[i];
    }
    fps_y[PERF_MEMORY - 1] = io.Framerate;
    lastPerfCollect = cur;
}

void show_performance_window() {
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
	window_flags |= ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
	window_flags |= ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 work_pos = viewport->WorkPos;
	ImVec2 work_size = viewport->WorkSize;
	ImVec2 window_pos, window_pos_pivot;
	window_pos.x = work_pos.x + work_size.x - 10.0f;
	window_pos.y = work_pos.y + 10.0f;
	window_pos_pivot.x = 1.0f;
	window_pos_pivot.y = 0.0f;
	ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
	ImGui::SetNextWindowSize(ImVec2(work_size.x / 6.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.35f);
	if (ImGui::Begin("Performance", NULL, window_flags)) {
        ImGui::Text("Initialization Time: %.2f ms", init_time);
        ImGui::Separator();
        static ImPlotFlags plot_flags = ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText;
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, {0, 0, 0, 0.3});
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
        if (ImPlot::BeginPlot("Frames Per Second", ImVec2(work_size.x / 5.0f, work_size.y / 10.0f), plot_flags)) {
            static ImPlotAxisFlags plot_x_flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines;
            static ImPlotAxisFlags plot_y_flags = ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit;
            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.35f);
            ImPlot::SetupAxes("", "", plot_x_flags, plot_y_flags);
            ImPlot::PlotShaded("", fps_x, fps_shade, fps_y, PERF_MEMORY);
            ImPlot::PlotLine("", fps_x, fps_y, PERF_MEMORY);
            ImPlot::PopStyleVar();
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor(2);
	}
	ImGui::End();
}

void render_imgui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("Drive")) {
		if (ImGui::MenuItem("Performance Window", "CTRL + P", showPerformance)) showPerformance = !showPerformance;
		if (ImGui::MenuItem("Quit", "CTRL + Q", false)) glfwSetWindowShouldClose(window, GLFW_TRUE);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Renderer")) {
		ImGui::ColorEdit3("Directional Light Color", lightColor);
		if (ImGui::BeginCombo("Skybox", skyboxNames[cur_skybox].c_str())) {
			for (int i = 0; i < skyboxes.size(); i++) {
				if (ImGui::Selectable(skyboxNames[i].c_str(), cur_skybox == i)) cur_skybox = i;
			}
			ImGui::EndCombo();
		}
		ImGui::EndMenu();
	}
    if (ImGui::BeginMenu("Settings")) {
        char videoModeComboLabel[20];
        snprintf(videoModeComboLabel, 20, "%dx%d %dHz", currentVidMode.width, currentVidMode.height, currentVidMode.refreshRate);
        if (ImGui::BeginCombo("Fullscreen Mode", videoModeComboLabel)) {
            for (size_t i = 0; i < videoModesCount; i++) {
                snprintf(videoModeComboLabel, 20, "%dx%d %dHz", videoModes[i].width, videoModes[i].height, videoModes[i].refreshRate);
                if (ImGui::Selectable(videoModeComboLabel, videoModes[i] == currentVidMode)) {
                    currentVidMode = videoModes[i];
                    if (fullscreen) glfwSetWindowMonitor(window, monitor, 0, 0, currentVidMode.width, currentVidMode.width, currentVidMode.refreshRate);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndMenu();
    }
	ImGui::EndMainMenuBar();

	if (showPerformance) show_performance_window();
    
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void setup() {
	// Initialize GLFW callbacks
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, WindowResized);
	// Load shaders into render passes
	mainPass.loadShaders(&mainVert, &mainFrag);
	mainPassInst.loadShaders(&mainVertInstanced, &mainFrag);
	shadowPass.loadShaders(&shadowVert, &shadowFrag);
	shadowPassInst.loadShaders(&shadowVertInstanced, &shadowFrag);
    // Set up shadowmap resources
    glGenFramebuffers(1, &shadowFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
    glGenTextures(1, &shadowTexture);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_DIM, SHADOW_DIM, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw runtime_error("Failed to set up shadow framebuffer!");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Setup meshes
	mat4 car_transform = Scale(0.75f) * RotateY(-90);
	dMesh car_mesh = dMesh("objects/car.obj", "textures/car.png", car_transform);
	// Mesh, mass, engine force, rolling resistance, air drag
	car = Car(car_mesh, 500.0, 3, 10.0, 10.0);
	car.pos = vec3(2, 0, 0);
	floor_mesh = dMesh(floor_points, floor_uvs, floor_normals, floor_triangles, "textures/racetrack.png");
    mat4 large_tree_transform = Scale(2.0);
	large_tree_mesh = dMesh("objects/largetree.obj", "textures/largetree.png", large_tree_transform);
	grass_mesh = dMesh("objects/grass.obj", "textures/grass.png", mat4());
	campfire_mesh = dMesh("objects/campfire.obj", "textures/campfire.png", Scale(0.5f));
	sleeping_bag_mesh = dMesh("objects/sleeping_bag.obj", "textures/sleeping_bag.png", Translate(0, 0.05, 0));
    // Setup instance render buffers
	vector<mat4> large_tree_instance_transforms;
	for (vec3 pos : large_tree_instance_positions)
		large_tree_instance_transforms.push_back(Translate(pos) * RotateY(rand_float(-180.0f, 180.0f)));
	large_tree_mesh.setupInstances(large_tree_instance_transforms);
	vector<mat4> grass_instance_transforms;
	for (vec3 pos : grass_instance_positions)
		grass_instance_transforms.push_back(Translate(pos) * RotateY(rand_float(-180.0f, 180.0f)));
    grass_mesh.setupInstances(grass_instance_transforms);
	// Setup skyboxes
	for (string path : skyboxPaths) {
		dSkybox skybox;
		skybox.setup();
		skybox.loadCubemap(path);
		skyboxes.push_back(skybox);
	}
    // Setup particle system
    particleSystem.setup();
	// Set some GL drawing context settings
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_CULL_FACE);
}

void cleanup() {
	// Cleanup meshes
	car.mesh.cleanup();
	floor_mesh.cleanup();
	large_tree_mesh.cleanup();
	grass_mesh.cleanup();
	campfire_mesh.cleanup();
	sleeping_bag_mesh.cleanup();
    // Cleanup skyboxes / particle system
	for (dSkybox skybox : skyboxes)
		skybox.cleanup();
    particleSystem.cleanup();
    // Cleanup shadow map resources
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteTextures(1, &shadowTexture);
}

void draw() {
	// Draw scene to depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer);
	glViewport(0, 0, SHADOW_DIM, SHADOW_DIM);
	glClear(GL_DEPTH_BUFFER_BIT);
	glCullFace(GL_FRONT);
	// Rendering shadow map
	shadowPass.use();
	mat4 depthProj = Orthographic(-80, 80, -80, 80, -20, 100);
	mat4 depthView = LookAt(vec3(20, 30, 20), vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 depthVP = depthProj * depthView;
	shadowPass.set("depth_vp", depthVP);
	shadowPass.set("model", floor_mesh.model);
	shadowPass.set("transform", Scale(60));
	floor_mesh.render();
	shadowPass.set("model", campfire_mesh.model);
	shadowPass.set("transform", Translate(-16.62, 0, 11.89));
	campfire_mesh.render();
	shadowPass.set("model", sleeping_bag_mesh.model);
	shadowPass.set("transform", Translate(-17.86, 0, 10.67) * RotateY(-40.0f));
	sleeping_bag_mesh.render();
	shadowPass.set("transform", Translate(-15.92, 0, 10.98) * RotateY(45.0f));
	sleeping_bag_mesh.render();
	shadowPass.set("model", car.mesh.model);
	shadowPass.set("transform", car.transform());
	car.mesh.render();
	// Instanced rendering shadow map
	shadowPassInst.use();
	shadowPassInst.set("depth_vp", depthVP);
	shadowPassInst.set("model", large_tree_mesh.model);
	large_tree_mesh.renderInstanced();
	shadowPassInst.set("model", grass_mesh.model);
	grass_mesh.renderInstanced();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, win_width, win_height);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glCullFace(GL_BACK);
	// Draw scene as usual
    if (camera_loc == 1) { // Third person chase camera
        vec3 cameraDir = (car.dir + 2 * car.vel) / 2;
        camera.moveTo(car.pos + vec3(0, 1.5, 0) + -4 * cameraDir);
        camera.lookAt(car.pos + 4 * cameraDir);
        camera.up = vec3(0, 1, 0);
        camera.adjustFov(60 + length(car.vel) * 25);
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
	// Rendering main pass
	mainPass.use();
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	mainPass.set("txtr", 0);
	mainPass.set("shadow", 1);
	mainPass.set("lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
	mainPass.set("depth_vp", depthVP);
	mainPass.set("persp", camera.persp);
	mainPass.set("view", camera.view);
	mainPass.set("model", floor_mesh.model);
	mainPass.set("transform", Scale(60));
	floor_mesh.render();
	mainPass.set("model", campfire_mesh.model);
	mainPass.set("transform", Translate(-16.62, 0, 11.89));
	campfire_mesh.render();
	mainPass.set("model", sleeping_bag_mesh.model);
	mainPass.set("transform", Translate(-17.86, 0, 10.67) * RotateY(-40.0f));
	sleeping_bag_mesh.render();
	mainPass.set("transform", Translate(-15.92, 0, 10.98) * RotateY(45.0f));
	sleeping_bag_mesh.render();
	mainPass.set("model", car.mesh.model);
	mainPass.set("transform", car.transform());
	car.mesh.render();
    particleSystem.draw(dt, camera.persp * camera.view, floor_mesh.texture, 60);
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.persp);
    // Rendering instanced main pass
	mainPassInst.use();
	mainPassInst.set("txtr", 0);
	mainPassInst.set("shadow", 1);
	mainPassInst.set("lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
	mainPassInst.set("depth_vp", depthVP);
	mainPassInst.set("persp", camera.persp);
	mainPassInst.set("view", camera.view);
	mainPassInst.set("model", large_tree_mesh.model);
	large_tree_mesh.renderInstanced();
	mainPassInst.set("model", grass_mesh.model);
    grass_mesh.renderInstanced();
	//dTextureDebug::show(shadowTexture, 0, 0, win_width / 4, win_height / 4);
	render_imgui();
	glFlush();
}

int main() {
	srand((int)time(NULL));
    time_p init_start = sys_clock::now();
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
	window = glfwCreateWindow(win_width, win_height, "Drive", NULL, NULL);
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
	// Set up ImGui context 
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410 core");
	// Setup, icon loading
	load_icon();
	setup();
    time_p init_finish = sys_clock::now();
    double_ms init_dur = init_finish - init_start;
    init_time = init_dur.count();
    init_perf();
    time_p lastSim = sys_clock::now();
	while (!glfwWindowShouldClose(window)) {
		time_p cur = sys_clock::now();
		double_ms since = cur - lastSim;
		dt = 1 / (1000.0f / since.count() / 60.0f);
		lastSim = cur;
		update_title(cur);
        collect_perf(cur);
		car.update(dt);
		car.collide();
		draw();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanup();
	// Cleaning up ImGui/ImPlot context
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
	// Closing window and cleaning up GLFW
	glfwDestroyWindow(window);
	glfwTerminate();
}
