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
#include "dCamera.h"
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

const int SHADOW_DIM = 16384;
RenderPass mainPass;
RenderPass mainPassInst;
RenderPass shadowPass;
RenderPass shadowPassInst;
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

Camera camera(win_width, win_height, 60, 0.5f, 120.0f);

int camera_type = 1;
// 1 - Third person chase camera
// 2 - Top-down camera of arena
// 3 - Hood camera
// 4 - Frozen free camera, moved right behind car

Mesh floor_mesh;
vector<vec3> floor_points = { {-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1} };
vector<vec3> floor_normals = { {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0} };
vector<vec2> floor_uvs = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
vector<int3> floor_triangles = { {2, 1, 0}, {0, 3, 2} };

Mesh large_tree_mesh;
vector<vec3> large_tree_instance_positions {
	{49, 0, 37}, {50, 0, 32}, {29, 0, 15}, {50, 0, 10},
	{-10, 0, 32}, {-34, 0, 7}, {15, 0, -13}, {6, 0, -15},
	{-0.75f, 0, 0.3f}, {-9.11f, 0, -8.64f}, {-16.16f, 0, -12.58f}, {-27.02f, 0, -15.88f}, 
	{-32.39f, 0, -7.18f}, {-22.69f, 0, -24.82f}, {-15.40f, 0, -31.95f}, {-8.23f, 0, -33.90f}, 
	{-7.48f, 0, -48.32f}, {7.70f, 0, -34.07f}, {49.83f, 0, -22.92f}, {54.68f, 0, -30.48f}, 
	{52.80f, 0, -38.50f}, {46.54f, 0, -49.25f}, {36.14f, 0, -50.60f}, {26.13f, 0, -51.73f}, 
	{10.83f, 0, -51.97f}, {-13.78f, 0, -33.34f}, {-25.02f, 0, -24.78f}, {-31.62f, 0, -16.71f}, 
	{-29.06f, 0, -6.59f}, {7.55f, 0, 10.67f}, {25.84f, 0, 21.15f}, {23.49f, 0, 34.07f}, 
	{7.52f, 0, 46.78f}, {-2.10f, 0, 54.42f}, {-12.51f, 0, 54.03f}, {-21.23f, 0, 54.79f}, 
	{-32.39f, 0, 54.86f}, {-37.58f, 0, 46.49f}, {-36.75f, 0, 35.87f}, {-41.17f, 0, 30.20f}, 
	{-52.15f, 0, 31.90f}, {-57.76f, 0, 14.15f}, {-58.13f, 0, 3.82f}, {-58.32f, 0, -4.81f}, 
	{-58.50f, 0, -15.75f}, {-58.31f, 0, -26.44f}, {-58.05f, 0, -38.86f}, {-57.40f, 0, -49.04f},
	{-34.08f, 0, -27.36f}, {-35.94f, 0, -16.43f}, {-31.61f, 0, -12.72f}, {-22.11f, 0, -10.64f}, 
	{-11.82f, 0, -11.64f}, {-10.84f, 0, -21.56f}, {-17.93f, 0, -28.87f}, {-25.40f, 0, -33.24f}, 
	{-34.90f, 0, -30.52f}, {-8.35f, 0, 9.46f}, {-7.34f, 0, 21.43f}, {-7.45f, 0, 35.74f}, 
	{-16.61f, 0, 39.66f}, {7.60f, 0, 16.11f}, {12.85f, 0, 11.31f}, {23.16f, 0, 8.94f}, 
	{23.09f, 0, 17.21f}, {33.01f, 0, 7.78f}, {31.65f, 0, 19.08f}, {30.85f, 0, 25.40f}, 
	{35.05f, 0, 33.10f}, {29.93f, 0, 34.80f}, {22.32f, 0, 41.41f}, {6.87f, 0, 36.39f}, 
	{6.92f, 0, 55.30f}, {12.63f, 0, 57.65f}, {22.51f, 0, 57.53f}, {29.38f, 0, 54.41f}, 
	{41.66f, 0, 56.19f}, {51.12f, 0, 53.26f}, {55.26f, 0, 46.08f}, {50.39f, 0, 42.98f}, 
	{53.33f, 0, 37.18f}, {56.60f, 0, 29.23f}, {50.68f, 0, 23.18f}, {47.43f, 0, 17.28f}, 
	{53.80f, 0, 13.17f}, {57.79f, 0, 2.03f}, {53.03f, 0, -7.96f}, {54.85f, 0, -20.03f}, 
	{53.67f, 0, -44.20f}, {35.25f, 0, -35.85f}, {25.01f, 0, 17.04f}, {27.80f, 0, 18.07f}, 
	{30.01f, 0, 21.32f}, {27.01f, 0, 24.33f},
};
Mesh grass_mesh;
vector<vec3> grass_instance_positions {
	{7.79f, 0, -5.38f}, {5.27f, 0, -8.41f}, {-6.32f, 0, -8.58f}, {-9.40f, 0, -5.62f}, 
	{-9.49f, 0, 5.47f}, {-6.46f, 0, 8.56f}, {4.59f, 0, 9.61f}, {7.51f, 0, 6.12f},
	{12.07f, 0, 5.49f}, {15.67f, 0, 5.87f}, {20.84f, 0, 5.78f}, {23.66f, 0, 10.43f},
	{16.71f, 0, 11.87f}, {-7.01f, 0, 17.79f}, {-7.03f, 0, 27.66f}, {-7.15f, 0, 37.66f},
	{-18.52f, 0, 41.08f}, {-21.48f, 0, 34.07f}, {-20.76f, 0, 22.88f}, {-30.34f, 0, 15.04f},
	{-36.73f, 0, 14.27f}, {-43.83f, 0, 14.90f}, {-44.92f, 0, 9.24f}, {-35.86f, 0, -6.22f},
	{-25.08f, 0, -8.68f}, {-14.91f, 0, -15.59f}, {-10.73f, 0, -23.47f}, {-11.57f, 0, -30.97f},
	{-20.33f, 0, -33.76f}, {-31.8f, 0, -28.42f}, {13.06f, 0, -5.47f}, {20.72f, 0, -5.45f}, 
	{31.65f, 0, -4.70f}, {35.97f, 0, -33.82f}, {25.40f, 0, -36.43f}, {9.04f, 0, 8.93f},
	{7.82f, 0, -36.36f}, {5.39f, 0, -23.72f}, {-9.14f, 0, -11.88f}, {-20.57f, 0, -12.46f}, 
	{-32.20f, 0, -20.17f}, {-20.66f, 0, -26.84f}, {4.43f, 0, -47.64f}, {3.93f, 0, -53.39f}, 
	{0.33f, 0, -57.04f}, {-7.27f, 0, -51.45f}, {-10.98f, 0, -48.05f}, {16.12f, 0, -50.30f}, 
	{30.95f, 0, -49.04f}, {45.22f, 0, -46.89f}, {49.36f, 0, -38.55f}, {50.14f, 0, -25.47f}, 
	{51.44f, 0, -17.93f}, {48.32f, 0, -10.58f}, {52.20f, 0, -5.34f}, {55.19f, 0, 2.71f}, 
	{53.23f, 0, 10.13f}, {48.65f, 0, 19.89f}, {53.20f, 0, 29.25f}, {52.94f, 0, 39.64f}, 
	{40.97f, 0, 53.80f}, {33.35f, 0, 55.97f}, {22.45f, 0, 55.02f}, {12.37f, 0, 52.92f}, 
	{4.89f, 0, 51.58f}, {7.12f, 0, 44.37f}, {8.70f, 0, 35.15f}, {8.79f, 0, 15.86f}, 
	{8.65f, 0, -8.66f}, {5.71f, 0, -19.49f}, {-7.94f, 0, -18.39f}, {-12.41f, 0, -13.37f}, 
	{-18.98f, 0, -6.11f}, {-23.53f, 0, -13.22f}, {-18.43f, 0, -21.01f}, {-12.61f, 0, -26.95f}, 
	{-20.56f, 0, -31.51f}, {-25.20f, 0, -27.63f}, {-26.66f, 0, -18.66f}, {-28.89f, 0, -11.67f}, 
	{-35.18f, 0, -10.46f}, {-35.20f, 0, -24.29f}, {-42.69f, 0, 6.82f}, {-33.15f, 0, 27.00f}, 
	{-33.92f, 0, 33.73f}, {-33.91f, 0, 43.06f}, {-33.89f, 0, 49.01f}, {-37.87f, 0, 51.80f}, 
	{-41.86f, 0, 46.81f}, {-39.06f, 0, 37.08f}, {-37.24f, 0, 30.39f}, {-45.18f, 0, 26.06f}, 
	{-52.10f, 0, 28.20f}, {-51.13f, 0, 35.53f}, {-48.92f, 0, 42.02f}, {-44.19f, 0, 49.47f}, 
	{-48.15f, 0, 51.39f}, {-52.47f, 0, 45.00f}, {-19.89f, 0, 37.80f}, {-8.92f, 0, 40.31f}, 
	{6.92f, 0, 40.09f}, {21.51f, 0, 40.12f}, {31.11f, 0, 42.29f}, {34.82f, 0, 36.45f}, 
	{35.20f, 0, 30.16f}, {34.46f, 0, 21.91f}, {33.01f, 0, 14.88f}, {31.73f, 0, 7.26f}, 
	{18.89f, 0, 8.64f}, {11.94f, 0, 10.72f}, {6.14f, 0, 13.45f}, {19.11f, 0, 15.25f}, 
	{25.39f, 0, 18.05f}, {27.88f, 0, 24.53f}, {24.49f, 0, 30.18f}, {-15.33f, 0, -9.27f}, 
	{-17.74f, 0, -15.42f}, {-21.91f, 0, -19.81f}, {-28.09f, 0, -25.55f}, {-35.06f, 0, -18.19f}, 
	{-31.77f, 0, -11.53f}, {-17.96f, 0, -8.67f}, {-7.88f, 0, -29.60f}, {-14.56f, 0, -29.49f}, 
	{-16.12f, 0, -34.21f}, {-21.71f, 0, 28.06f}, {6.18f, 0, 37.26f}, {7.01f, 0, 49.03f}, 
	{10.97f, 0, 55.62f}, {17.51f, 0, 56.45f}, {49.75f, 0, 49.19f}, {53.50f, 0, 33.92f}, 
	{49.00f, 0, 25.35f}, {53.92f, 0, 22.33f}, {49.92f, 0, 17.05f}, {53.79f, 0, 16.88f}, 
	{56.55f, 0, 13.86f}, {56.15f, 0, 8.20f}, {50.05f, 0, 6.49f}, {33.86f, 0, 10.72f}, 
	{26.73f, 0, 12.74f}, {23.75f, 0, 14.55f}, {29.74f, 0, 18.33f}, {28.37f, 0, 7.24f}, 
	{34.98f, 0, -7.36f}, {26.29f, 0, -5.06f}, {48.05f, 0, -21.12f}, {52.87f, 0, -23.11f},
	{54.21f, 0, -27.11f}, {49.96f, 0, -33.09f}, {49.25f, 0, -43.09f}, {49.27f, 0, -49.34f}, 
	{43.54f, 0, -53.94f}, {34.86f, 0, -55.65f}, {31.18f, 0, -54.53f}, {23.49f, 0, -47.76f}, 
	{19.93f, 0, -52.79f}, {16.68f, 0, -57.96f}, {11.80f, 0, -57.26f}, {9.42f, 0, -53.46f},
	{10.55f, 0, -47.26f}, {16.66f, 0, -47.86f}, {7.30f, 0, -56.81f}, {0.79f, 0, -55.33f}, 
	{-6.02f, 0, -54.50f}, {-11.82f, 0, -57.15f}, {-22.68f, 0, -58.74f}, {-30.38f, 0, -57.70f},
	{-40.51f, 0, -57.49f}, {-53.80f, 0, -57.32f}, {-57.84f, 0, -53.29f}, {-58.09f, 0, -44.97f}, 
	{-58.10f, 0, -32.31f}, {-57.77f, 0, -20.81f}, {-57.48f, 0, -11.24f}, {-57.76f, 0, -0.54f}, 
	{-58.23f, 0, 8.00f}, {-57.65f, 0, 20.25f}, {-56.34f, 0, 27.91f}, {-12.13f, 0, -7.25f}, 
	{-10.14f, 0, -17.27f}, {-13.09f, 0, -21.82f}, {-17.47f, 0, -25.97f}, {-23.17f, 0, -31.02f}, 
	{-31.75f, 0, -32.83f}, {-31.41f, 0, -24.12f}, {-25.43f, 0, -21.01f}, {-19.74f, 0, -23.85f}, 
	{-16.54f, 0, -25.51f}, {-7.47f, 0, -25.94f}, {-9.09f, 0, -20.63f}, {-20.19f, 0, -17.30f},
	{-30.55f, 0, -8.13f}, {-24.77f, 0, -6.63f}, {-13.41f, 0, -7.41f}, {12.13f, 0, -7.70f}, 
	{48.80f, 0, -29.86f}, {54.54f, 0, -13.45f}, {55.77f, 0, -6.18f}, {57.66f, 0, -15.36f}, 
	{54.96f, 0, -34.84f}, {52.84f, 0, -46.37f}, {50.84f, 0, -54.69f}, {37.50f, 0, -53.79f}, 
	{30.17f, 0, -50.93f}, {25.60f, 0, -55.13f}, {20.31f, 0, -56.20f}, {15.10f, 0, -53.24f}, 
	{10.99f, 0, -50.25f}, {-0.23f, 0, -53.39f}, {-4.14f, 0, -56.11f}, {-11.35f, 0, -52.83f}, 
	{-33.86f, 0, -35.34f}, {-36.90f, 0, -22.12f}, {-37.35f, 0, -10.74f}, {-21.66f, 0, 24.40f}, 
	{-21.72f, 0, 39.58f}, {-31.84f, 0, 51.97f}, {-36.49f, 0, 54.49f}, {-34.69f, 0, 56.44f}, 
	{-27.99f, 0, 54.64f}, {-20.92f, 0, 53.10f}, {-16.84f, 0, 56.33f}, {-12.06f, 0, 57.21f}, 
	{-9.39f, 0, 53.56f}, {-4.92f, 0, 54.34f}, {2.20f, 0, 56.54f}, {8.00f, 0, 52.92f}, 
	{9.49f, 0, 57.30f}, {15.74f, 0, 54.92f}, {24.05f, 0, 41.10f}, {25.94f, 0, 36.21f}, 
	{27.32f, 0, 32.39f}, {29.15f, 0, 27.36f}, {32.31f, 0, 21.46f}, {35.16f, 0, 24.48f}, 
	{32.21f, 0, 32.18f}, {32.75f, 0, 36.64f}, {30.44f, 0, 40.23f}, {23.81f, 0, 41.49f}, 
	{19.42f, 0, 37.20f}
};

Mesh campfire_mesh;
Mesh sleeping_bag_mesh;

struct Car {
	Mesh mesh;
	float mass = 0.0; // mass of car
	float engine = 0.0; // engine force
	float roll = 0.0; // rolling resistance
	float drag = 0.0; // aerodynamic drag constant
	float last_pt = 0.0f;
	vec3 pos; // position
	vec3 dir; // direction
	vec3 vel; // velocity
	Car() { };
	Car(Mesh _mesh, float _mass, float _engine, float _roll, float _drag) {
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
		float drift = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) ? 0.5f : 1.0f;
		// Check for car turning with A/D
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			float deg = dt * -1.5f * ((drift == 0.5f) ? 1.25f : 1.0f);
			vec4 dirw = RotateY(deg) * dir;
			dir = vec3(dirw.x, dirw.y, dirw.z);
		}
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			float deg = dt * 1.5f * ((drift == 0.5f) ? 1.25f : 1.0f);
			vec4 dirw = RotateY(deg) * dir;
			dir = vec3(dirw.x, dirw.y, dirw.z);
		}
		vec3 force = vec3(0.0);
		// Apply engine force if pressed (F = u{vel} * engine)
		float turbo = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 1.5f : 1.0f;
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
				particleSystem.createParticle(pos - 0.3f * cross(dir, vec3(0, 1, 0)) - 0.5f * dir, vec3(1, 0.1f, 0.1f));
				particleSystem.createParticle(pos + 0.3f * cross(dir, vec3(0, 1, 0)) - 0.5f * dir, vec3(1, 0.1f, 0.1f));
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
	camera.width = win_width = width;
	camera.height = win_height = height;
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
            printf("{%.2ff, %.0ff, %.2ff}, ", car.pos.x, car.pos.y, car.pos.z);
        }
    }
    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
        camera_type = 1;
    if (key == GLFW_KEY_2 && action == GLFW_PRESS)
        camera_type = 2;
    if (key == GLFW_KEY_3 && action == GLFW_PRESS)
        camera_type = 3;
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
        camera_type = 4;
		camera.loc = car.pos + vec3(0, 1.5, 0) + -3 * car.dir;
		camera.look = car.pos + 2.5 * car.dir;
		camera.fov = 70;
        camera.up = vec3(0, 1, 0);
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
	float_ms sinceTitleUpdate = cur - lastTitleUpdate;
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
    float_ms sincePerfCollect = cur - lastPerfCollect;
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
        ImPlot::PushStyleColor(ImPlotCol_FrameBg, {0.0f, 0.0f, 0.0f, 0.3f});
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0.0f, 0.0f, 0.0f, 0.0f});
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
	Mesh car_mesh = Mesh("objects/car.obj", "textures/car.png", car_transform);
	// Mesh, mass, engine force, rolling resistance, air drag
	car = Car(car_mesh, 500.0, 3, 10.0, 10.0);
	car.pos = vec3(2, 0, 0);
	floor_mesh = Mesh(floor_points, floor_uvs, floor_normals, floor_triangles, "textures/racetrack.png");
    mat4 large_tree_transform = Scale(2.0);
	large_tree_mesh = Mesh("objects/largetree.obj", "textures/largetree.png", large_tree_transform);
	grass_mesh = Mesh("objects/grass.obj", "textures/grass.png", mat4());
	campfire_mesh = Mesh("objects/campfire.obj", "textures/campfire.png", Scale(0.5f));
	sleeping_bag_mesh = Mesh("objects/sleeping_bag.obj", "textures/sleeping_bag.png", Translate(0.0f, 0.05f, 0.0f));
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
	shadowPass.set("transform", Translate(-16.62f, 0, 11.89f));
	campfire_mesh.render();
	shadowPass.set("model", sleeping_bag_mesh.model);
	shadowPass.set("transform", Translate(-17.86f, 0, 10.67f) * RotateY(-40.0f));
	sleeping_bag_mesh.render();
	shadowPass.set("transform", Translate(-15.92f, 0, 10.98f) * RotateY(45.0f));
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
    if (camera_type == 1) { // Third person chase camera
        vec3 cameraDir = (car.dir + 2 * car.vel) / 2;
		camera.loc = car.pos + vec3(0, 1.5f, 0) + -4 * cameraDir;
		camera.look = car.pos + 4 * cameraDir;
        camera.up = vec3(0, 1, 0);
		camera.fov = 60 + length(car.vel) * 25;
    } else if (camera_type == 2) { // Top-down camera
		camera.loc = car.pos + vec3(0, 30, 0);
		camera.look = car.pos;
        camera.up = car.dir;
		camera.fov = 60;
    } else if (camera_type == 3) {
		camera.loc = car.pos + vec3(0, 0.5f, 0) + car.dir * 0.5f;
		camera.look = car.pos + car.dir * 3 + vec3(0, 0.5f, 0);
        camera.up = vec3(0, 1, 0);
		camera.fov = 60;
    }
	camera.update();
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
	mainPass.set("transform", Translate(-16.62f, 0, 11.89f));
	campfire_mesh.render();
	mainPass.set("model", sleeping_bag_mesh.model);
	mainPass.set("transform", Translate(-17.86f, 0, 10.67f) * RotateY(-40.0f));
	sleeping_bag_mesh.render();
	mainPass.set("transform", Translate(-15.92f, 0, 10.98f) * RotateY(45.0f));
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
	//TextureDebug::show(shadowTexture, 0, 0, win_width / 4, win_height / 4);
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
    float_ms init_dur = init_finish - init_start;
    init_time = init_dur.count();
    init_perf();
    time_p lastSim = sys_clock::now();
	while (!glfwWindowShouldClose(window)) {
		time_p cur = sys_clock::now();
		float_ms since = cur - lastSim;
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
