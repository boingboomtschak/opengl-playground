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
GLuint instancedProgram = 0;
GLuint shadowProgram = 0;
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
    layout (location = 3) in vec3 instance_pos;
    out vec2 vUv;
    out vec4 shadowCoord;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 persp;
    uniform mat4 depth_vp;
    void main() {
        mat4 transform = mat4(
            1, 0, 0, instance_pos.x,
            0, 1, 0, instance_pos.y,
            0, 0, 1, instance_pos.z,
            0, 0, 0, 1
        );
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
vector<vec3> large_tree_positions = {
	{49, 0, 37}, {50, 0, 32}, {29, 0, 15}, {50, 0, 10},
	{-10, 0, 32}, {-34, 0, 7}, {15, 0, -13}, {6, 0, -15},
	{-0.75, 0, 0.3}
};
dMesh grass_mesh;
GLuint grass_position_buffer = 0;
vector<vec3> grass_positions = {
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
	{4.89, 0, 51.58}, {7.12, 0, 44.37}, {8.70, 0, 35.15}, {8.79, 0, 15.86}
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
		for (vec3 tree_pos : large_tree_positions) {
			float d = dist(pos, tree_pos);
			if (d < 1.5) vel *= 0.5;
			//if (d < 1.0) vel = (pos - tree_pos); hard collision with tree
		}
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
    if (!(instancedProgram = LinkProgramViaCode(&mainVertInstanced, &mainFrag)))
        throw runtime_error("Failed to compile instanced render program");
	if (!(shadowProgram = LinkProgramViaCode(&shadowVert, &shadowFrag)))
		throw runtime_error("Failed to compile shadow render program!");
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
    grass_mesh.setupInstances(grass_positions);
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
    // Cleanup instance render buffers
    glDeleteBuffers(1, &grass_position_buffer);
    
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
	SetUniform(shadowProgram, "model", large_tree_mesh.model);
	for (vec3 pos : large_tree_positions) {
		SetUniform(shadowProgram, "transform", Translate(pos));
		large_tree_mesh.render();
	}
	SetUniform(shadowProgram, "model", grass_mesh.model);
	for (vec3 pos : grass_positions) {
		SetUniform(shadowProgram, "transform", Translate(pos));
		grass_mesh.render();
	}
	SetUniform(shadowProgram, "model", car.mesh.model);
	SetUniform(shadowProgram, "transform", car.transform());
	car.mesh.render();
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
	SetUniform(mainProgram, "model", large_tree_mesh.model);
	for (vec3 pos : large_tree_positions) {
		SetUniform(mainProgram, "transform", Translate(pos));
		large_tree_mesh.render();
	}
	SetUniform(mainProgram, "model", car.mesh.model);
	SetUniform(mainProgram, "transform", car.transform());
	car.mesh.render();
    particleSystem.draw(dt, camera.persp * camera.view, floor_mesh.texture, 60);
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.persp);
    // Instanced renders
    glUseProgram(instancedProgram);
    SetUniform(instancedProgram, "txtr", 0);
    SetUniform(instancedProgram, "shadow", 1);
    SetUniform(instancedProgram, "model", grass_mesh.model);
    SetUniform(instancedProgram, "lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
    SetUniform(instancedProgram, "edgeSamples", shadowEdgeSamples);
    SetUniform(instancedProgram, "depth_vp", depthVP);
    SetUniform(instancedProgram, "persp", camera.persp);
    SetUniform(instancedProgram, "view", camera.view);
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
