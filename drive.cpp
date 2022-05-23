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
#include "dSkybox.h"
#include "dParticles.h"
#include "dTextureDebug.h"
#include "dMesh.h"
#include "dMisc.h"
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
GLuint mainProgram = 0;
GLuint mainProgramInstanced = 0;
GLuint shadowProgram = 0;
GLuint shadowProgramInstanced = 0;
GLuint shadowFramebuffer = 0;
GLuint shadowTexture = 0;

float lightColor[3] = { 1.0f, 1.0f, 1.0f };
float init_time;
int shadowEdgeSamples = 16;

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
	uniform int edgeSamples;
	vec2 uniformSamples[32] = vec2[](vec2(0.49338352, -0.58302237), vec2(-0.39376479, 0.12189280), vec2(-0.38876976, 0.39560871), vec2(-0.82853213, 0.29121478), vec2(-0.62251564, 0.27426500), vec2(0.44906493, 0.72971920), vec2(0.99295605, 0.02762058), vec2(-0.61054051, -0.74474791), vec2(-0.49073490, 0.09812672), vec2(0.64145907, -0.23052487), vec2(-0.47168601, 0.81892203), vec2(0.95110052, 0.97483373), vec2(0.84048903, 0.82753596), vec2(-0.94147225, 0.42333745), vec2(-0.97706586, 0.22633662), vec2(0.00977269, 0.02378330), vec2(-0.21250551, 0.39536213), vec2(0.46426639, 0.17288661), vec2(-0.44197788, 0.33506576), vec2(0.80805167, -0.29359674), vec2(-0.66379370, 0.04307460), vec2(0.26607188, 0.79704354), vec2(0.20652568, 0.81991369), vec2(0.64959186, -0.64564514), vec2(0.93534138, 0.83045920), vec2(0.31952140, 0.95451090), vec2(-0.85996893, 0.29045370), vec2(-0.33230688, -0.34582716), vec2(0.87055498, 0.64248681), vec2(-0.19631182, -0.83353633), vec2(0.70041707, 0.58055892), vec2(0.78863981, -0.50693407));
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
		for (int i = 0; i < edgeSamples; i++) {
			int ind = int(float(edgeSamples)*random(vec4(gl_FragCoord.xyy, i))) % edgeSamples;
			shadowVal += texture(shadow, vec3(coord.xy + uniformSamples[ind] * texelSize, coord.z)) == 0.0f ? 0.4f : 1.0f;
		}
        return shadowVal / (float(edgeSamples) + 5.0f);
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
vector<mat4> large_tree_instance_transforms = {
	Translate(49.00, 0, 37.00), Translate(50.00, 0, 32.00), Translate(29.00, 0, 15.00), Translate(50.00, 0, 10.00),
	Translate(-10.00, 0, 32.00), Translate(-34.00, 0, 7.00), Translate(15.00, 0, -13.00), Translate(6.00, 0, -15.00),
	Translate(-0.75, 0, 0.3), Translate(-9.11, 0, -8.64), Translate(-16.16, 0, -12.58), Translate(-27.02, 0, -15.88), 
	Translate(-32.39, 0, -7.18), Translate(-22.69, 0, -24.82), Translate(-15.40, 0, -31.95), Translate(-8.23, 0, -33.90), 
	Translate(-7.48, 0, -48.32), Translate(7.70, 0, -34.07), Translate(49.83, 0, -22.92), Translate(54.68, 0, -30.48), 
	Translate(52.80, 0, -38.50), Translate(46.54, 0, -49.25), Translate(36.14, 0, -50.60), Translate(26.13, 0, -51.73), 
	Translate(10.83, 0, -51.97), Translate(-13.78, 0, -33.34), Translate(-25.02, 0, -24.78), Translate(-31.62, 0, -16.71), 
	Translate(-29.06, 0, -6.59), Translate(7.55, 0, 10.67), Translate(25.84, 0, 21.15), Translate(23.49, 0, 34.07), 
	Translate(7.52, 0, 46.78), Translate(-2.10, 0, 54.42), Translate(-12.51, 0, 54.03), Translate(-21.23, 0, 54.79), 
	Translate(-32.39, 0, 54.86), Translate(-37.58, 0, 46.49), Translate(-36.75, 0, 35.87), Translate(-41.17, 0, 30.20), 
	Translate(-52.15, 0, 31.90), Translate(-57.76, 0, 14.15), Translate(-58.13, 0, 3.82), Translate(-58.32, 0, -4.81), 
	Translate(-58.50, 0, -15.75), Translate(-58.31, 0, -26.44), Translate(-58.05, 0, -38.86), Translate(-57.40, 0, -49.04),
	Translate(-34.08, 0, -27.36), Translate(-35.94, 0, -16.43), Translate(-31.61, 0, -12.72), Translate(-22.11, 0, -10.64), 
	Translate(-11.82, 0, -11.64), Translate(-10.84, 0, -21.56), Translate(-17.93, 0, -28.87), Translate(-25.40, 0, -33.24), 
	Translate(-34.90, 0, -30.52), Translate(-8.35, 0, 9.46), Translate(-7.34, 0, 21.43), Translate(-7.45, 0, 35.74), 
	Translate(-16.61, 0, 39.66), Translate(7.60, 0, 16.11), Translate(12.85, 0, 11.31), Translate(23.16, 0, 8.94), 
	Translate(23.09, 0, 17.21), Translate(33.01, 0, 7.78), Translate(31.65, 0, 19.08), Translate(30.85, 0, 25.40), 
	Translate(35.05, 0, 33.10), Translate(29.93, 0, 34.80), Translate(22.32, 0, 41.41), Translate(6.87, 0, 36.39), 
	Translate(6.92, 0, 55.30), Translate(12.63, 0, 57.65), Translate(22.51, 0, 57.53), Translate(29.38, 0, 54.41), 
	Translate(41.66, 0, 56.19), Translate(51.12, 0, 53.26), Translate(55.26, 0, 46.08), Translate(50.39, 0, 42.98), 
	Translate(53.33, 0, 37.18), Translate(56.60, 0, 29.23), Translate(50.68, 0, 23.18), Translate(47.43, 0, 17.28), 
	Translate(53.80, 0, 13.17), Translate(57.79, 0, 2.03), Translate(53.03, 0, -7.96), Translate(54.85, 0, -20.03), 
	Translate(53.67, 0, -44.20), Translate(35.25, 0, -35.85),
};
dMesh grass_mesh;
vector<mat4> grass_instance_transforms {
	Translate(7.79, 0, -5.38), Translate(5.27, 0, -8.41), Translate(-6.32, 0, -8.58), Translate(-9.40, 0, -5.62), 
	Translate(-9.49, 0, 5.47), Translate(-6.46, 0, 8.56), Translate(4.59, 0, 9.61), Translate(7.51, 0, 6.12),
	Translate(12.07, 0, 5.49), Translate(15.67, 0, 5.87), Translate(20.84, 0, 5.78), Translate(23.66, 0, 10.43),
	Translate(16.71, 0, 11.87), Translate(-7.01, 0, 17.79), Translate(-7.03, 0, 27.66), Translate(-7.15, 0, 37.66),
	Translate(-18.52, 0, 41.08), Translate(-21.48, 0, 34.07), Translate(-20.76, 0, 22.88), Translate(-30.34, 0, 15.04),
	Translate(-36.73, 0, 14.27), Translate(-43.83, 0, 14.90), Translate(-44.92, 0, 9.24), Translate(-35.86, 0, -6.22),
	Translate(-25.08, 0, -8.68), Translate(-14.91, 0, -15.59), Translate(-10.73, 0, -23.47), Translate(-11.57, 0, -30.97),
	Translate(-20.33, 0, -33.76), Translate(-31.8, 0, -28.42), Translate(13.06, 0, -5.47), Translate(20.72, 0, -5.45), 
	Translate(31.65, 0, -4.70), Translate(35.97, 0, -33.82), Translate(25.40, 0, -36.43), Translate(9.04, 0, 8.93),
	Translate(7.82, 0, -36.36), Translate(5.39, 0, -23.72), Translate(-9.14, 0, -11.88), Translate(-20.57, 0, -12.46), 
	Translate(-32.20, 0, -20.17), Translate(-20.66, 0, -26.84), Translate(4.43, 0, -47.64), Translate(3.93, 0, -53.39), 
	Translate(0.33, 0, -57.04), Translate(-7.27, 0, -51.45), Translate(-10.98, 0, -48.05), Translate(16.12, 0, -50.30), 
	Translate(30.95, 0, -49.04), Translate(45.22, 0, -46.89), Translate(49.36, 0, -38.55), Translate(50.14, 0, -25.47), 
	Translate(51.44, 0, -17.93), Translate(48.32, 0, -10.58), Translate(52.20, 0, -5.34), Translate(55.19, 0, 2.71), 
	Translate(53.23, 0, 10.13), Translate(48.65, 0, 19.89), Translate(53.20, 0, 29.25), Translate(52.94, 0, 39.64), 
	Translate(40.97, 0, 53.80), Translate(33.35, 0, 55.97), Translate(22.45, 0, 55.02), Translate(12.37, 0, 52.92), 
	Translate(4.89, 0, 51.58), Translate(7.12, 0, 44.37), Translate(8.70, 0, 35.15), Translate(8.79, 0, 15.86), 
	Translate(8.65, 0, -8.66), Translate(5.71, 0, -19.49), Translate(-7.94, 0, -18.39), Translate(-12.41, 0, -13.37), 
	Translate(-18.98, 0, -6.11), Translate(-23.53, 0, -13.22), Translate(-18.43, 0, -21.01), Translate(-12.61, 0, -26.95), 
	Translate(-20.56, 0, -31.51), Translate(-25.20, 0, -27.63), Translate(-26.66, 0, -18.66), Translate(-28.89, 0, -11.67), 
	Translate(-35.18, 0, -10.46), Translate(-35.20, 0, -24.29), Translate(-42.69, 0, 6.82), Translate(-33.15, 0, 27.00), 
	Translate(-33.92, 0, 33.73), Translate(-33.91, 0, 43.06), Translate(-33.89, 0, 49.01), Translate(-37.87, 0, 51.80), 
	Translate(-41.86, 0, 46.81), Translate(-39.06, 0, 37.08), Translate(-37.24, 0, 30.39), Translate(-45.18, 0, 26.06), 
	Translate(-52.10, 0, 28.20), Translate(-51.13, 0, 35.53), Translate(-48.92, 0, 42.02), Translate(-44.19, 0, 49.47), 
	Translate(-48.15, 0, 51.39), Translate(-52.47, 0, 45.00), Translate(-19.89, 0, 37.80), Translate(-8.92, 0, 40.31), 
	Translate(6.92, 0, 40.09), Translate(21.51, 0, 40.12), Translate(31.11, 0, 42.29), Translate(34.82, 0, 36.45), 
	Translate(35.20, 0, 30.16), Translate(34.46, 0, 21.91), Translate(33.01, 0, 14.88), Translate(31.73, 0, 7.26), 
	Translate(18.89, 0, 8.64), Translate(11.94, 0, 10.72), Translate(6.14, 0, 13.45), Translate(19.11, 0, 15.25), 
	Translate(25.39, 0, 18.05), Translate(27.88, 0, 24.53), Translate(24.49, 0, 30.18), Translate(-15.33, 0, -9.27), 
	Translate(-17.74, 0, -15.42), Translate(-21.91, 0, -19.81), Translate(-28.09, 0, -25.55), Translate(-35.06, 0, -18.19), 
	Translate(-31.77, 0, -11.53), Translate(-17.96, 0, -8.67), Translate(-7.88, 0, -29.60), Translate(-14.56, 0, -29.49), 
	Translate(-16.12, 0, -34.21), Translate(-21.71, 0, 28.06), Translate(6.18, 0, 37.26), Translate(7.01, 0, 49.03), 
	Translate(10.97, 0, 55.62), Translate(17.51, 0, 56.45), Translate(49.75, 0, 49.19), Translate(53.50, 0, 33.92), 
	Translate(49.00, 0, 25.35), Translate(53.92, 0, 22.33), Translate(49.92, 0, 17.05), Translate(53.79, 0, 16.88), 
	Translate(56.55, 0, 13.86), Translate(56.15, 0, 8.20), Translate(50.05, 0, 6.49), Translate(33.86, 0, 10.72), 
	Translate(26.73, 0, 12.74), Translate(23.75, 0, 14.55), Translate(29.74, 0, 18.33), Translate(28.37, 0, 7.24), 
	Translate(34.98, 0, -7.36), Translate(26.29, 0, -5.06), Translate(48.05, 0, -21.12), Translate(52.87, 0, -23.11),
	Translate(54.21, 0, -27.11), Translate(49.96, 0, -33.09), Translate(49.25, 0, -43.09), Translate(49.27, 0, -49.34), 
	Translate(43.54, 0, -53.94), Translate(34.86, 0, -55.65), Translate(31.18, 0, -54.53), Translate(23.49, 0, -47.76), 
	Translate(19.93, 0, -52.79), Translate(16.68, 0, -57.96), Translate(11.80, 0, -57.26), Translate(9.42, 0, -53.46),
	Translate(10.55, 0, -47.26), Translate(16.66, 0, -47.86), Translate(7.30, 0, -56.81), Translate(0.79, 0, -55.33), 
	Translate(-6.02, 0, -54.50), Translate(-11.82, 0, -57.15), Translate(-22.68, 0, -58.74), Translate(-30.38, 0, -57.70),
	Translate(-40.51, 0, -57.49), Translate(-53.80, 0, -57.32), Translate(-57.84, 0, -53.29), Translate(-58.09, 0, -44.97), 
	Translate(-58.10, 0, -32.31), Translate(-57.77, 0, -20.81), Translate(-57.48, 0, -11.24), Translate(-57.76, 0, -0.54), 
	Translate(-58.23, 0, 8.00), Translate(-57.65, 0, 20.25), Translate(-56.34, 0, 27.91), Translate(-12.13, 0, -7.25), 
	Translate(-10.14, 0, -17.27), Translate(-13.09, 0, -21.82), Translate(-17.47, 0, -25.97), Translate(-23.17, 0, -31.02), 
	Translate(-31.75, 0, -32.83), Translate(-31.41, 0, -24.12), Translate(-25.43, 0, -21.01), Translate(-19.74, 0, -23.85), 
	Translate(-16.54, 0, -25.51), Translate(-7.47, 0, -25.94), Translate(-9.09, 0, -20.63), Translate(-20.19, 0, -17.30),
	Translate(-30.55, 0, -8.13), Translate(-24.77, 0, -6.63), Translate(-13.41, 0, -7.41), Translate(12.13, 0, -7.70), 
	Translate(48.80, 0, -29.86), Translate(54.54, 0, -13.45), Translate(55.77, 0, -6.18), Translate(57.66, 0, -15.36), 
	Translate(54.96, 0, -34.84), Translate(52.84, 0, -46.37), Translate(50.84, 0, -54.69), Translate(37.50, 0, -53.79), 
	Translate(30.17, 0, -50.93), Translate(25.60, 0, -55.13), Translate(20.31, 0, -56.20), Translate(15.10, 0, -53.24), 
	Translate(10.99, 0, -50.25), Translate(-0.23, 0, -53.39), Translate(-4.14, 0, -56.11), Translate(-11.35, 0, -52.83), 
	Translate(-33.86, 0, -35.34), Translate(-36.90, 0, -22.12), Translate(-37.35, 0, -10.74), Translate(-21.66, 0, 24.40), 
	Translate(-21.72, 0, 39.58), Translate(-31.84, 0, 51.97), Translate(-36.49, 0, 54.49), Translate(-34.69, 0, 56.44), 
	Translate(-27.99, 0, 54.64), Translate(-20.92, 0, 53.10), Translate(-16.84, 0, 56.33), Translate(-12.06, 0, 57.21), 
	Translate(-9.39, 0, 53.56), Translate(-4.92, 0, 54.34), Translate(2.20, 0, 56.54), Translate(8.00, 0, 52.92), 
	Translate(9.49, 0, 57.30), Translate(15.74, 0, 54.92), Translate(24.05, 0, 41.10), Translate(25.94, 0, 36.21), 
	Translate(27.32, 0, 32.39), Translate(29.15, 0, 27.36), Translate(32.31, 0, 21.46), Translate(35.16, 0, 24.48), 
	Translate(32.21, 0, 32.18), Translate(32.75, 0, 36.64), Translate(30.44, 0, 40.23), Translate(23.81, 0, 41.49), 
	Translate(19.42, 0, 37.20)
};

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
            printf("Translate(%.2f, %.0f, %.2f), ", car.pos.x, car.pos.y, car.pos.z);
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
		char sampleComboPreview[11];
		snprintf(sampleComboPreview, 11, "%d Samples", shadowEdgeSamples);
		if (ImGui::BeginCombo("Shadow Edge Samples", sampleComboPreview)) {
			if (ImGui::Selectable("2 Samples", shadowEdgeSamples == 2)) shadowEdgeSamples = 2;
			if (ImGui::Selectable("4 Samples", shadowEdgeSamples == 4)) shadowEdgeSamples = 4;
			if (ImGui::Selectable("8 Samples", shadowEdgeSamples == 8)) shadowEdgeSamples = 8;
			if (ImGui::Selectable("16 Samples", shadowEdgeSamples == 16)) shadowEdgeSamples = 16;
			if (ImGui::Selectable("32 Samples", shadowEdgeSamples == 32)) shadowEdgeSamples = 32;
			ImGui::EndCombo();
		}
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
	// Compile programs
	if (!(mainProgram = LinkProgramViaCode(&mainVert, &mainFrag)))
		throw runtime_error("Failed to compile main render program!");
    if (!(mainProgramInstanced = LinkProgramViaCode(&mainVertInstanced, &mainFrag)))
        throw runtime_error("Failed to compile main instanced render program");
	if (!(shadowProgram = LinkProgramViaCode(&shadowVert, &shadowFrag)))
		throw runtime_error("Failed to compile shadow render program!");
	if (!(shadowProgramInstanced = LinkProgramViaCode(&shadowVertInstanced, &shadowFrag)))
		throw runtime_error("Failed to compile shadow instanced render program!");
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
	mat4 grass_transform;
	grass_mesh = dMesh("objects/grass.obj", "textures/grass.png", grass_transform);
    // Setup instance render buffers
	large_tree_mesh.setupInstances(large_tree_instance_transforms);
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
	glUseProgram(shadowProgram);
	glCullFace(GL_FRONT);
	mat4 depthProj = Orthographic(-80, 80, -80, 80, -20, 100);
	mat4 depthView = LookAt(vec3(20, 30, 20), vec3(0, 0, 0), vec3(0, 1, 0));
	mat4 depthVP = depthProj * depthView;
	SetUniform(shadowProgram, "depth_vp", depthVP);
	SetUniform(shadowProgram, "model", floor_mesh.model);
	SetUniform(shadowProgram, "transform", Scale(60));
	floor_mesh.render();
	SetUniform(shadowProgram, "model", car.mesh.model);
	SetUniform(shadowProgram, "transform", car.transform());
	car.mesh.render();
	glUseProgram(shadowProgramInstanced);
	SetUniform(shadowProgramInstanced, "depth_vp", depthVP);
	SetUniform(shadowProgramInstanced, "model", large_tree_mesh.model);
	large_tree_mesh.renderInstanced();
	SetUniform(shadowProgramInstanced, "model", grass_mesh.model);
	grass_mesh.renderInstanced();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, win_width, win_height);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glUseProgram(mainProgram);
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
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowTexture);
	SetUniform(mainProgram, "txtr", 0);
	SetUniform(mainProgram, "shadow", 1);
	SetUniform(mainProgram, "lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
	SetUniform(mainProgram, "edgeSamples", shadowEdgeSamples);
	SetUniform(mainProgram, "depth_vp", depthVP);
	SetUniform(mainProgram, "persp", camera.persp);
	SetUniform(mainProgram, "view", camera.view);
	SetUniform(mainProgram, "model", floor_mesh.model);
	SetUniform(mainProgram, "transform", Scale(60));
	floor_mesh.render();
	SetUniform(mainProgram, "model", car.mesh.model);
	SetUniform(mainProgram, "transform", car.transform());
	car.mesh.render();
    particleSystem.draw(dt, camera.persp * camera.view, floor_mesh.texture, 60);
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.persp);
    // Instanced renders
    glUseProgram(mainProgramInstanced);
    SetUniform(mainProgramInstanced, "txtr", 0);
    SetUniform(mainProgramInstanced, "shadow", 1);
    SetUniform(mainProgramInstanced, "lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
    SetUniform(mainProgramInstanced, "edgeSamples", shadowEdgeSamples);
    SetUniform(mainProgramInstanced, "depth_vp", depthVP);
    SetUniform(mainProgramInstanced, "persp", camera.persp);
    SetUniform(mainProgramInstanced, "view", camera.view);
	SetUniform(mainProgramInstanced, "model", large_tree_mesh.model);
	large_tree_mesh.renderInstanced();
	SetUniform(mainProgramInstanced, "model", grass_mesh.model);
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
