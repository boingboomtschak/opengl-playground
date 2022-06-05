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
#include "dCollisions.h"
#include "dCamera.h"
#include "dRenderPass.h"
#include "dMesh.h"
#include "dMisc.h"
#include "dSkybox.h"
#include "dParticles.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
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
bool frustumCulling = true;
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
	"textures/skybox/humble/",
	"textures/skybox/classic-land/",
	"textures/skybox/maskonaive/",
	"textures/skybox/empty-space/",
	"textures/skybox/dusk-ocean/",
};
vector<string> skyboxNames {
	"Humble",
	"Classic Land",
	"Maskonaive",
	"Empty Space",
	"Dusk Ocean",
};
int cur_skybox = 0;

dParticles particleSystem;

Camera camera(win_width, win_height, 60, 0.5f, 800.0f);

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
	{30.01f, 0, 21.32f}, {27.01f, 0, 24.33f}, {-7.86f, 0, -15.65f}, {-14.89f, 0, -17.18f}, 
	{-24.90f, 0, -20.11f}, {-31.91f, 0, -20.91f}, {-57.89f, 0, 9.50f}, {-59.31f, 0, 19.05f},
	{-58.51f, 0, 25.29f}, {-56.70f, 0, 35.26f}, {-50.98f, 0, 38.97f}, {-46.18f, 0, 33.34f}, 
	{-38.07f, 0, 26.96f}, {-34.56f, 0, 31.28f}, {-34.16f, 0, 37.87f}, {-36.81f, 0, 42.84f}, 
	{-41.31f, 0, 42.38f}, {-32.40f, 0, 49.67f}, {-23.46f, 0, 57.46f}, {-14.06f, 0, 56.53f}, 
	{-2.28f, 0, 53.18f}, {6.61f, 0, 50.97f}, {14.48f, 0, 53.34f}, {19.61f, 0, 54.60f}, 
	{27.19f, 0, 56.32f}, {45.39f, 0, 57.37f}, {51.48f, 0, 48.51f}, {53.57f, 0, 42.74f}, 
	{55.55f, 0, 36.85f}, {54.38f, 0, 26.52f}, {55.33f, 0, 19.61f}, {54.74f, 0, 6.12f}, 
	{49.12f, 0, -5.46f}, {46.48f, 0, -14.67f}, {56.58f, 0, -27.82f}, {54.64f, 0, -40.71f}, 
	{49.18f, 0, -45.38f}, {40.86f, 0, -49.48f}, {25.39f, 0, -49.89f}, {13.77f, 0, -49.16f}, 
	{-2.02f, 0, -51.96f}, {11.01f, 0, 7.26f}, {13.33f, 0, 8.87f}, {16.48f, 0, 10.46f},
    {19.92f, 0, 11.65f}, {26.48f, 0, 12.77f}, {32.18f, 0, 10.76f}, {35.11f, 0, 6.94f},
    {31.87f, 0, 16.33f}, {7.90f, 0, 38.96f}, {8.29f, 0, 42.33f}, {2.28f, 0, 51.69f},
    {-6.42f, 0, 56.59f}, {-28.42f, 0, 53.62f}, {-38.48f, 0, 52.90f}, {-41.16f, 0, 52.04f},
    {-45.38f, 0, 48.90f}, {-46.25f, 0, 45.71f}, {-45.19f, 0, 42.25f}, {-43.87f, 0, 36.51f},
    {-44.07f, 0, 31.30f}, {-47.49f, 0, 27.07f}, {-51.11f, 0, 27.18f}, {-54.14f, 0, 31.55f},
    {-53.87f, 0, 37.11f}, {-53.30f, 0, 41.10f}, {-57.40f, 0, 18.26f}, {-56.87f, 0, 11.89f},
    {-57.14f, 0, 7.42f}, {-56.71f, 0, 0.37f}, {-57.52f, 0, -19.77f}, {-57.82f, 0, -30.18f},
    {-57.25f, 0, -42.02f}, {-42.21f, 0, -58.22f}, {-31.70f, 0, -57.72f}, {-13.13f, 0, -57.26f},
    {-5.69f, 0, -57.26f}, {4.19f, 0, -57.32f}, {12.61f, 0, -57.31f}, {20.29f, 0, -56.96f},
    {25.05f, 0, -56.66f}, {30.72f, 0, -56.69f}, {34.94f, 0, -56.89f}, {39.91f, 0, -56.25f},
    {42.73f, 0, -54.73f}, {45.96f, 0, -52.28f}, {49.24f, 0, -50.56f}, {52.45f, 0, -50.05f},
    {52.41f, 0, -56.66f}, {46.12f, 0, -53.55f}, {42.78f, 0, -52.05f}, {34.30f, 0, -51.96f},
    {29.32f, 0, -46.49f}, {47.28f, 0, -39.51f}, {49.31f, 0, -35.55f}, {50.26f, 0, -31.58f},
    {51.32f, 0, -27.77f}, {52.52f, 0, -22.80f}, {53.24f, 0, -17.62f}, {55.14f, 0, -11.74f},
    {56.41f, 0, -6.88f}, {51.25f, 0, 12.24f}, {49.11f, 0, 19.59f}, {48.08f, 0, 24.16f},
    {49.38f, 0, 29.80f}, {53.92f, 0, 33.04f}, {57.16f, 0, 36.24f}, {56.22f, 0, 48.83f},
    {47.80f, 0, 53.40f}, {37.92f, 0, 54.39f}, {20.33f, 0, 55.91f}, {10.11f, 0, 54.36f},
    {-26.37f, 0, 54.01f}, {34.43f, 0, 54.40f}, {31.47f, 0, 57.06f}, {53.41f, 0, 54.71f},
    {55.33f, 0, 41.25f}, {50.85f, 0, 35.64f}, {49.49f, 0, 25.79f}, {51.63f, 0, 18.89f},
    {53.68f, 0, 1.17f}, {54.13f, 0, -3.98f}, {57.81f, 0, -15.81f}, {58.67f, 0, -19.96f},
    {55.51f, 0, -35.65f}, {48.99f, 0, -40.59f}, {46.78f, 0, -44.15f}, {43.45f, 0, -47.26f},
    {34.74f, 0, -48.75f}, {30.60f, 0, -49.21f}, {22.94f, 0, -48.71f}, {18.03f, 0, -49.42f},
    {11.44f, 0, -53.66f}, {7.85f, 0, -54.97f}, {2.40f, 0, -54.92f}, {-4.58f, 0, -54.76f},
    {-18.88f, 0, -57.27f}, {-24.03f, 0, -58.32f}, {-36.24f, 0, -58.71f}, {-46.70f, 0, -57.97f},
    {-53.89f, 0, -57.15f}, {-58.09f, 0, -45.90f}, {-56.46f, 0, -36.90f}, {-56.69f, 0, -26.61f},
    {-56.60f, 0, -14.89f}, {-56.29f, 0, 22.82f}, {-51.58f, 0, 45.38f}, {-43.59f, 0, 53.44f},
    {-38.02f, 0, 56.29f}, {-18.65f, 0, 55.90f}, {-1.49f, 0, 57.18f}, {5.27f, 0, 58.12f},
    {32.96f, 0, 56.77f}, {46.31f, 0, 51.64f}, {47.90f, 0, 41.29f}, {35.89f, 0, 38.23f},
    {30.27f, 0, 39.04f}, {26.93f, 0, 38.70f}, {23.19f, 0, 31.18f}, {27.90f, 0, 28.60f},
    {32.41f, 0, 27.79f}, {33.90f, 0, 14.39f}, {27.47f, 0, 9.37f}, {8.09f, 0, -6.20f},
    {8.81f, 0, -13.83f}, {6.51f, 0, -22.14f}, {6.91f, 0, -24.57f}, {7.72f, 0, -27.59f},
    {16.58f, 0, -5.08f}, {27.26f, 0, -5.23f},
};
vector<mat4> large_tree_instance_transforms;
int num_culled_large_trees = 0;

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
	{19.42f, 0, 37.20f}, {-33.55f, 0, 14.57f}, {-27.67f, 0, 14.98f}, {-29.79f, 0, 14.04f}, 
	{-32.18f, 0, 13.55f}, {-35.26f, 0, 13.33f}, {-38.98f, 0, 13.40f}, {-44.59f, 0, 12.91f}, 
	{-46.66f, 0, 7.57f}, {-44.61f, 0, 5.95f}, {-40.08f, 0, 5.57f}, {-27.39f, 0, -5.66f}, 
	{-26.85f, 0, -9.52f}, {-26.07f, 0, -12.88f}, {-24.40, 0, -16.34f}, {-23.27f, 0, -20.96f}, 
	{-27.79f, 0, -23.86f}, {-29.43f, 0, -21.98f}, {-30.33f, 0, -18.65f}, {-30.18f, 0, -15.58f}, 
	{-31.41f, 0, -13.76f}, {-34.75f, 0, -12.85f}, {-36.49f, 0, -14.15f}, {-36.92f, 0, -18.37f}, 
	{-36.22f, 0, -20.76f}, {-34.27f, 0, -22.29f}, {-32.51f, 0, -24.84f}, {-31.13f, 0, -26.86f}, 
	{-29.61f, 0, -29.13f}, {-28.76f, 0, -32.15f}, {-28.27f, 0, -34.72f}, {-26.58f, 0, -36.07f}, 
	{-22.98f, 0, -35.24f}, {-22.26f, 0, -33.07f}, {-21.23f, 0, -29.38f}, {-18.45f, 0, -26.65f}, 
	{-13.72f, 0, -25.51f}, {-10.28f, 0, -25.43f}, {-5.84f, 0, -27.94f}, {-6.24f, 0, -32.47f}, 
	{-6.26f, 0, -35.59f}, {-10.39f, 0, -36.67f}, {-14.25f, 0, -36.43f}, {-17.40f, 0, -36.38f}, 
	{-22.20f, 0, -36.52f}, {-27.87f, 0, -36.81f}, {-36.01f, 0, -36.67f}, {-17.48f, 0, -18.48f}, 
	{-21.01f, 0, -21.98f}, {-25.72f, 0, -23.66f}, {-28.87f, 0, -19.30f}, {-28.49f, 0, -13.54f}, 
	{-26.28f, 0, -10.59f}, {-22.39f, 0, -8.57f}, {-16.60f, 0, -5.67f}, {-13.35f, 0, -4.84f}, 
	{-7.64f, 0, -6.07f}, {-6.41f, 0, -11.05f}, {-9.05f, 0, -14.32f}, {-12.80f, 0, -16.71f}, 
	{-15.39f, 0, -19.39f}, {-16.52f, 0, -22.11f}, {-15.72f, 0, -27.53f}, {-18.79f, 0, -34.71f}, 
	{-16.88f, 0, -23.59f}, {-14.52f, 0, -20.90f}, {-12.08f, 0, -16.92f}, {-7.35f, 0, -12.63f}, 
	{-5.75f, 0, -17.78f}, {-5.84f, 0, -21.74f}, {6.76f, 0, -47.18f}, {7.78f, 0, -50.94f}, 
	{4.30f, 0, -54.88f}, {-0.90f, 0, -56.83f}, {-6.28f, 0, -58.10f}, {-10.86f, 0, -54.56f}, 
	{-8.83f, 0, -50.10f}, {-19.19f, 0, -56.78f}, {-16.15f, 0, -58.57f}, {-12.47f, 0, -49.55f}, 
	{4.79f, 0, -12.65f}, {9.41f, 0, -10.75f}, {10.83f, 0, -6.25f}, {4.76f, 0, -11.12f}, 
	{3.96f, 0, -20.00f}, {4.69f, 0, -27.87f}, {5.04f, 0, -33.21f}, {10.44f, 0, -36.83f}, 
	{27.54f, 0, -35.99f}, {36.46f, 0, -21.07f}, {36.40f, 0, -17.21f}, {35.67f, 0, -11.70f}, 
	{35.96f, 0, -4.80f}, {29.95f, 0, -6.10f}, {24.39f, 0, -5.66f}, {18.90f, 0, -5.31f}, 
	{11.17f, 0, -5.14f}, {-7.37f, 0, 12.00f}, {-6.71f, 0, 15.04f}, {-5.78f, 0, 19.91f}, 
	{-5.51f, 0, 24.88f}, {-5.64f, 0, 30.31f}, {-5.64f, 0, 35.28f}, {-5.81f, 0, 39.78f}, 
	{-10.05f, 0, 42.11f}, {-13.67f, 0, 41.83f}, {-20.10f, 0, 42.18f}, {-22.22f, 0, 37.72f}, 
	{-20.33f, 0, 34.77f}, {-21.37f, 0, 31.31f}, {-20.09f, 0, 26.05f}, {-20.46f, 0, 20.12f}, 
	{-39.62f, 0, 15.22f}, {-42.69f, 0, 12.81f}, {-42.90f, 0, 9.50f}, {-56.69f, 0, 21.79f}, 
	{-56.43f, 0, 24.90f}, {-52.67f, 0, 26.47f}, {-48.11f, 0, 26.70f}, {-44.68f, 0, 28.87f}, 
	{-41.58f, 0, 26.45f}, {-38.73f, 0, 24.67f}, {-34.97f, 0, 29.57f}, {-34.64f, 0, 37.02f}, 
	{-33.14f, 0, 46.21f}, {-25.28f, 0, 56.25f}, {-9.07f, 0, 56.45f}, {-2.69f, 0, 57.27f}, 
	{3.69f, 0, 58.65f}, {16.90f, 0, 58.91f}, {27.23f, 0, 58.25f}, {36.18f, 0, 56.12f}, 
	{43.06f, 0, 52.91f}, {48.84f, 0, 48.37f}, {52.10f, 0, 44.23f}, {55.49f, 0, 39.55f}, 
	{56.85f, 0, 34.40f}, {54.39f, 0, 28.08f}, {52.06f, 0, 21.57f}, {51.39f, 0, 15.64f}, 
	{52.38f, 0, 6.39f}, {53.44f, 0, 0.50f}, {55.45f, 0, -9.32f}, {56.91f, 0, -18.42f}, 
	{57.29f, 0, -25.93f}, {54.85f, 0, -37.79f}, {51.45f, 0, -44.24f}, {43.59f, 0, -49.36f}, 
	{36.16f, 0, -52.86f}, {28.66f, 0, -56.44f}, {22.47f, 0, -57.88f}, {17.48f, 0, -56.27f}, 
	{18.20f, 0, -48.93f}, {26.47f, 0, -47.31f}, {34.46f, 0, -47.86f}, {42.41f, 0, -51.44f}, 
	{49.29f, 0, -36.01f}, {47.09f, 0, -27.21f}, {49.26f, 0, -18.03f}, {56.10f, 0, -4.74f}, 
	{56.14f, 0, -4.00f}, {53.54f, 0, 1.52f}, {47.41f, 0, 7.95f}, {46.24f, 0, 21.65f},
    {38.94f, 0, -47.43f}, {33.79f, 0, -49.83f}, {30.55f, 0, -52.44f}, {27.22f, 0, -54.78f},
    {22.52f, 0, -55.69f}, {17.95f, 0, -55.24f}, {13.81f, 0, -55.44f}, {9.36f, 0, -56.34f},
    {2.66f, 0, -56.98f}, {-2.99f, 0, -57.04f}, {-9.30f, 0, -57.38f}, {-15.63f, 0, -57.94f},
    {-20.38f, 0, -58.25f}, {-25.96f, 0, -58.29f}, {-34.27f, 0, -58.04f}, {-39.78f, 0, -58.01f},
    {-45.41f, 0, -58.31f}, {-52.74f, 0, -57.31f}, {-56.38f, 0, -55.73f}, {-58.71f, 0, -46.41f},
    {-56.91f, 0, -40.70f}, {-56.34f, 0, -35.57f}, {-56.91f, 0, -26.94f}, {-56.76f, 0, -23.05f},
    {-56.58f, 0, -16.76f}, {-56.61f, 0, -12.46f}, {-56.59f, 0, -6.18f}, {-56.64f, 0, -1.49f},
    {-56.39f, 0, 8.65f}, {-56.33f, 0, 12.85f}, {-56.32f, 0, 17.78f}, {-54.17f, 0, 28.98f},
    {-48.91f, 0, 29.00f}, {-45.86f, 0, 27.89f}, {-34.40f, 0, 26.29f}, {-36.32f, 0, 28.00f},
    {-39.20f, 0, 31.56f}, {-41.69f, 0, 34.52f}, {-44.07f, 0, 37.33f}, {-45.98f, 0, 40.27f},
    {-47.25f, 0, 43.02f}, {-47.96f, 0, 46.18f}, {-47.64f, 0, 48.80f}, {-45.17f, 0, 51.62f},
    {-42.54f, 0, 52.05f}, {-38.97f, 0, 50.31f}, {-36.42f, 0, 47.81f}, {-34.10f, 0, 40.85f},
    {-30.29f, 0, 57.02f}, {-20.73f, 0, 56.91f}, {-7.47f, 0, 57.99f}, {-4.24f, 0, 57.32f},
    {1.91f, 0, 54.61f}, {13.65f, 0, 54.87f}, {30.86f, 0, 57.23f}, {37.79f, 0, 54.56f},
    {46.25f, 0, 52.50f}, {51.37f, 0, 50.67f}, {56.74f, 0, 35.82f}, {53.26f, 0, 31.24f},
    {50.68f, 0, 28.23f}, {47.81f, 0, 22.54f}, {50.64f, 0, 13.15f}, {51.93f, 0, 9.73f},
    {54.83f, 0, -1.94f}, {55.71f, 0, -13.07f}, {56.62f, 0, -21.19f}, {52.35f, 0, -30.55f},
    {48.46f, 0, -40.50f}, {53.61f, 0, -48.78f}, {5.09f, 0, 16.62f}, {9.34f, 0, 12.47f},
    {12.81f, 0, 13.42f}, {15.58f, 0, 14.36f}, {19.47f, 0, 14.14f}, {22.79f, 0, 13.22f},
    {28.75f, 0, 11.69f}, {33.18f, 0, 12.77f}, {33.85f, 0, 17.45f}, {33.40f, 0, 24.04f},
    {32.91f, 0, 27.34f}, {32.58f, 0, 34.53f}, {32.69f, 0, 39.06f}, {34.38f, 0, 41.43f},
    {35.74f, 0, 39.22f}, {35.91f, 0, 37.12f}, {36.90f, 0, 32.56f}, {37.51f, 0, 29.22f},
    {32.64f, 0, 26.35f}, {27.75f, 0, 34.76f}, {27.28f, 0, 36.89f}, {26.97f, 0, 38.37f},
    {24.39f, 0, 38.93f}, {22.82f, 0, 36.59f}, {21.97f, 0, 34.31f}, {22.24f, 0, 30.07f},
    {23.61f, 0, 25.81f}, {23.95f, 0, 22.37f}, {16.42f, 0, 15.65f}, {14.28f, 0, 15.42f},
    {12.38f, 0, 15.56f}, {6.59f, 0, 19.50f}, {4.14f, 0, 23.44f}, {4.71f, 0, 31.14f},
    {5.15f, 0, 34.47f}, {5.30f, 0, 38.29f}, {5.30f, 0, 41.84f}, {5.20f, 0, 45.10f},
    {4.66f, 0, 48.77f}, {-0.55f, 0, 55.32f}, {-9.01f, 0, 57.66f}, {-17.88f, 0, 56.68f},
    {-24.87f, 0, 52.94f}, {-27.82f, 0, 51.87f}, {-34.80f, 0, 51.60f}, {-41.61f, 0, 51.37f},
    {-50.01f, 0, 46.54f}, {-49.57f, 0, 43.92f}, {-48.88f, 0, 38.37f}, {-48.94f, 0, 35.12f},
    {-49.97f, 0, 31.50f}, {-51.18f, 0, 29.02f}, {-55.23f, 0, 24.71f}, {-46.65f, 0, 14.44f},
    {-45.64f, 0, 10.87f}, {-45.77f, 0, 9.32f}, {-46.22f, 0, 4.41f}, {-35.55f, 0, -9.57f},
    {-32.86f, 0, -9.59f}, {-29.92f, 0, -10.06f}, {-23.26f, 0, -14.88f}, {-22.00f, 0, -17.16f},
    {-19.81f, 0, -21.49f}, {-19.55f, 0, -29.35f}, {-20.39f, 0, -35.36f}, {-17.33f, 0, -32.07f},
    {-15.32f, 0, -28.43f}, {-11.14f, 0, -25.83f}, {5.46f, 0, -29.87f}, {7.75f, 0, -30.83f},
    {23.47f, 0, -36.36f}, {47.78f, 0, -28.33f}, {50.18f, 0, -27.19f}, {52.96f, 0, -25.91f},
    {57.32f, 0, -23.95f}, {56.58f, 0, -31.89f}, {54.45f, 0, -33.92f}, {52.47f, 0, -36.03f},
    {50.95f, 0, -38.36f}, {51.49f, 0, -46.06f}, {52.44f, 0, -48.50f}, {53.00f, 0, -51.11f},
    {52.91f, 0, -53.58f}, {50.07f, 0, -57.54f}, {45.45f, 0, -57.41f}, {43.36f, 0, -56.52f},
    {40.55f, 0, -55.41f}, {37.62f, 0, -54.58f}, {34.99f, 0, -54.38f}, {32.68f, 0, -54.87f},
    {25.77f, 0, -58.26f}, {19.76f, 0, -58.25f}, {13.74f, 0, -56.77f}, {5.09f, 0, -56.43f},
    {-7.84f, 0, -56.52f}, {-18.55f, 0, -57.89f}, {-27.67f, 0, -57.99f}, {-36.49f, 0, -58.12f},
    {-41.07f, 0, -58.32f},
};
vector<mat4> grass_instance_transforms;
int num_culled_grass = 0;

Mesh cloud_mesh;
vector<mat4> cloud_instance_transforms;

Mesh campfire_mesh;
Mesh sleeping_bag_mesh;

struct Car {
	Mesh mesh;
	float mass = 0.0; // mass of car
	float engine = 0.0; // engine force
	float roll = 0.0; // rolling resistance
	float drag = 0.0; // aerodynamic drag constant
	vec3 pos = vec3(0, 0, 0); // position
	vec3 dir = vec3(1, 0, 0); // direction
	vec3 vel = vec3(0, 0, 0); // velocity
	Car() { };
	Car(Mesh _mesh, float _mass, float _engine, float _roll, float _drag) {
		mesh = _mesh;
		mass = _mass;
		engine = _engine;
		roll = _roll;
		drag = _drag;
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
		static float last_pt = 0.0f;
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
            printf("{%.2ff, %.0f, %.2ff}, ", car.pos.x, car.pos.y, car.pos.z);
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
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            frustumCulling = !frustumCulling;
            if (!frustumCulling) {
                large_tree_mesh.loadInstances(large_tree_instance_transforms);
                num_culled_large_trees = (int)large_tree_instance_transforms.size();
                grass_mesh.loadInstances(grass_instance_transforms);
                num_culled_grass = (int)grass_instance_transforms.size();
            }
        } else {
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
	ImGui::SetNextWindowSize(ImVec2(work_size.x / 4.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.35f);
	if (ImGui::Begin("Performance", NULL, window_flags)) {
        ImGui::Text("Initialization time: %.2f ms", init_time);
		ImGui::Text("Trees in view: %d / %d", num_culled_large_trees, (int)large_tree_instance_transforms.size());
		ImGui::Text("Grass in view: %d / %d", num_culled_grass, (int)grass_instance_transforms.size());
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
		if (ImGui::Checkbox("Frustum Culling", &frustumCulling)) {
			large_tree_mesh.loadInstances(large_tree_instance_transforms);
			num_culled_large_trees = (int)large_tree_instance_transforms.size();
			grass_mesh.loadInstances(grass_instance_transforms);
			num_culled_grass = (int)grass_instance_transforms.size();
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
	cloud_mesh = Mesh("objects/cloud.obj", "textures/cloud.png", Scale(4.0f));
	campfire_mesh = Mesh("objects/campfire.obj", "textures/campfire.png", Scale(0.5f));
	sleeping_bag_mesh = Mesh("objects/sleeping_bag.obj", "textures/sleeping_bag.png", Translate(0.0f, 0.05f, 0.0f));
    // Setup instance render buffers
	for (vec3 pos : large_tree_instance_positions)
		large_tree_instance_transforms.push_back(Translate(pos) * RotateY(rand_float(-180.0f, 180.0f)));
	large_tree_mesh.setupInstanceBuffer((GLsizei)large_tree_instance_transforms.size());
	large_tree_mesh.loadInstances(large_tree_instance_transforms); 
	for (vec3 pos : grass_instance_positions)
		grass_instance_transforms.push_back(Translate(pos) * RotateY(rand_float(-180.0f, 180.0f)));
	grass_mesh.setupInstanceBuffer((GLsizei)grass_instance_transforms.size());
	grass_mesh.loadInstances(grass_instance_transforms); 
	for (int i = 0; i < 500; i++) {
		mat4 m = Translate(rand_float(-600.0, 600.0), rand_float(50.0, 80.0), rand_float(-600.0, 600.0)) * RotateY(rand_float(-5.0f, 5.0f)) * Scale(rand_float(2.0, 4.0), 1.0, rand_float(2.0, 4.0));
		cloud_instance_transforms.push_back(m);
	}
	cloud_mesh.setupInstanceBuffer((GLsizei)cloud_instance_transforms.size());
	cloud_mesh.loadInstances(cloud_instance_transforms);
	// Setup colliders
	large_tree_mesh.createCollider<Sphere>();
	grass_mesh.createCollider<Sphere>();
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
	cloud_mesh.cleanup();
    // Cleanup skyboxes / particle system
	for (dSkybox skybox : skyboxes)
		skybox.cleanup();
    particleSystem.cleanup();
    // Cleanup shadow map resources
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteTextures(1, &shadowTexture);
}

void draw() {
	// Update camera
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
	if (frustumCulling) {
		// Cull instances out of frustum, update instances
        Frustum frustum(camera);
		//vector<mat4> culled_large_trees = cull_instances_aabb(large_tree_mesh.collider, large_tree_instance_transforms, camera.persp * camera.view, large_tree_mesh.model);
        vector<mat4> culled_large_trees = cull_instances_sphere(frustum, large_tree_mesh.collider, large_tree_instance_transforms);
		large_tree_mesh.loadInstances(culled_large_trees);
		num_culled_large_trees = (int)culled_large_trees.size();
		//vector<mat4> culled_grass = cull_instances_aabb(grass_mesh.collider, grass_instance_transforms, camera.persp * camera.view, grass_mesh.model);
        vector<mat4> culled_grass = cull_instances_sphere(frustum, grass_mesh.collider, grass_instance_transforms);
		grass_mesh.loadInstances(culled_grass);
		num_culled_grass = (int)culled_grass.size();
	}
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
	mainPassInst.set("model", cloud_mesh.model);
	cloud_mesh.renderInstanced();
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
