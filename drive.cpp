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
#include "Mesh.h"
#include "Misc.h"
#include "dSkybox.h"
#include "dParticles.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

using std::vector;
using std::string;
using std::runtime_error;

GLFWwindow* window;

int windowed_width = 1000, windowed_height = 800;
int win_width = windowed_width, win_height = windowed_height;
bool fullscreen = false;
float dt;

const int SHADOW_DIM = 16384;
GLuint shadowProgram = 0;
GLuint shadowFramebuffer = 0;
GLuint shadowTexture = 0;

float lightColor[3] = { 1.0f, 1.0f, 1.0f };
int shadowEdgeSamples = 16;


// TEX QUAD DEBUG
GLuint quadProgram = 0, quadVArray = 0, quadVBuffer = 0;
vector<vec2> quadPoints {
    {-1, -1}, {1, -1}, {1, 1},
    {1, 1}, {-1, 1}, {-1, -1}
};
const char* quadVert = R"(
    #version 410 core
    in vec2 point;
    out vec2 uv;
    void main() {
        uv = vec2(point.x / 2 + 0.5, point.y / 2 + 0.5);
        gl_Position = vec4(point, 0, 1);
    }
)";
const char* quadFrag = R"(
    #version 410 core
    in vec2 uv;
    out vec4 color;
    uniform sampler2D tex;
    void main() {
        color = vec4(vec3(texture(tex, uv).r), 1.0);
    }
)";

typedef std::chrono::system_clock::time_point time_p;
typedef std::chrono::system_clock sys_clock;
typedef std::chrono::duration<double, std::milli> double_ms;


const char* shadowVert = R"(
	#version 410 core
	in vec3 point;
	uniform mat4 depth_vp;
	uniform mat4 model;
	void main() {
		gl_Position = depth_vp * model * vec4(point, 1);
	}
)";

const char* shadowFrag = R"(
	#version 410 core
	void main() {}
)";

const char* meshCtrVert = R"(
	#version 410 core
	in vec3 point;
	in vec2 uv;
	out vec2 vUv;
	out vec4 shadowCoord;
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 persp;
	uniform mat4 depth_mvp;
	void main() {
		shadowCoord = depth_mvp * vec4(point, 1);
		vUv = uv;
		gl_Position = persp * view * model * vec4(point, 1);
	}
)";

const char* meshCtrFrag = R"(
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
vector<string> skyboxPaths{
	"textures/skybox/maskonaive/",
	"textures/skybox/classic-land/",
	"textures/skybox/empty-space/",
	"textures/skybox/dusk-ocean/"
};
int cur_skybox = 0;

dParticles particleSystem;

struct Camera {
	float fov = 60;
	float zNear = 0.5f;
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

// Simple mesh wrapper struct
struct MeshContainer {
	mat4 model = mat4();
	GLuint program = 0, texture = 0;
	GLuint vArray = 0, vBuffer = 0, iBuffer = 0;
    GLuint depthVArray = 0, depthVBuffer = 0, depthIBuffer = 0;
	vector<vec3> points;
	vector<vec3> normals;
	vector<vec2> uvs;
	vector<int3> triangles;
	MeshContainer() { }
	MeshContainer(vector<vec3> _points, vector<vec3> _normals, vector<vec2> _uvs, vector<int3> _triangles, string texFilename, bool texMipmap = true) {
		points = _points;
		normals = _normals;
		uvs = _uvs;
		triangles = _triangles;
		texture = LoadTexture(texFilename.c_str(), 0, texMipmap);
		if (texture < 0)
			throw runtime_error("Failed to read texture '" + texFilename + "'!");
		compile();
	}
	MeshContainer(string objFilename, string texFilename, mat4 mdl) {
		if (!ReadAsciiObj(objFilename.c_str(), points, triangles, &normals, &uvs))
			throw runtime_error("Failed to read mesh obj '" + objFilename + "'!");
		Normalize(points, 1.0f);
		texture = LoadTexture(texFilename.c_str(), 0);
		if (texture < 0)
			throw runtime_error("Failed to read texture '" + texFilename + "'!");
		model = mdl;
		compile();
	}
	void compile() {
		if (!(program = LinkProgramViaCode(&meshCtrVert, &meshCtrFrag)))
			throw runtime_error("Failed to compile mesh container shaders!");
	}
	void allocate() {
		glGenVertexArrays(1, &vArray);
		glBindVertexArray(vArray);
		size_t pSize = points.size() * sizeof(vec3);
		size_t nSize = normals.size() * sizeof(vec3);
		size_t uSize = uvs.size() * sizeof(vec2);
		size_t vBufSize = pSize + nSize + uSize;
		glGenBuffers(1, &vBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
		glBufferData(GL_ARRAY_BUFFER, vBufSize, NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, pSize, points.data());
		glBufferSubData(GL_ARRAY_BUFFER, pSize, nSize, normals.data());
		glBufferSubData(GL_ARRAY_BUFFER, pSize + nSize, uSize, uvs.data());
		size_t tSize = triangles.size() * sizeof(int3);
		glGenBuffers(1, &iBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, tSize, triangles.data(), GL_STATIC_DRAW);
		VertexAttribPointer(program, "point", 3, 0, 0);
		//VertexAttribPointer(program, "normal", 3, 0, (GLvoid*)(pSize));
		VertexAttribPointer(program, "uv", 2, 0, (GLvoid*)(pSize + nSize));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glGenVertexArrays(1, &depthVArray);
        glBindVertexArray(depthVArray);
        glGenBuffers(1, &depthVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, depthVBuffer);
        glBufferData(GL_ARRAY_BUFFER, pSize, points.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &depthIBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, depthIBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, tSize, triangles.data(), GL_STATIC_DRAW);
        VertexAttribPointer(shadowProgram, "point", 3, 0, 0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	void draw(mat4 transform, mat4 depth_vp = NULL) {
		glUseProgram(program);
		glBindVertexArray(vArray);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		mat4 m = transform * model;
		SetUniform(program, "model", m);
		SetUniform(program, "view", camera.view);
		SetUniform(program, "persp", camera.persp);
		SetUniform(program, "txtr", 0);
		if (depth_vp != NULL) {
            mat4 depth_mvp = depth_vp * m;
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, shadowTexture);
			SetUniform(program, "depth_mvp", depth_mvp);
			SetUniform(program, "shadow", 1);
			SetUniform(program, "lightColor", vec3(lightColor[0], lightColor[1], lightColor[2]));
			SetUniform(program, "edgeSamples", shadowEdgeSamples);
		}
		glDrawElements(GL_TRIANGLES, (GLsizei)(triangles.size() * sizeof(int3)), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (depth_vp != NULL) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
	}
	void drawDepth(mat4 transform) {
		glBindVertexArray(depthVArray);
        SetUniform(shadowProgram, "model", transform * model);
		glDrawElements(GL_TRIANGLES, (GLsizei)(triangles.size() * sizeof(int3)), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
	void deallocate() {
		glDeleteVertexArrays(1, &vArray);
		glDeleteBuffers(1, &vBuffer);
		glDeleteBuffers(1, &iBuffer);
		glDeleteTextures(1, &texture);
        // deallocate depth VAO/buffers
        glDeleteVertexArrays(1, &depthVArray);
        glDeleteBuffers(1, &depthVBuffer);
        glDeleteBuffers(1, &depthIBuffer);
	}
};

MeshContainer floor_mesh;
vector<vec3> floor_points = { {-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1} };
vector<vec3> floor_normals = { {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0} };
vector<vec2> floor_uvs = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
vector<int3> floor_triangles = { {2, 1, 0}, {0, 3, 2} };

MeshContainer large_tree_mesh;
vector<vec3> large_tree_positions = {
	{49, 0, 37}, {50, 0, 32}, {29, 0, 15}, {50, 0, 10},
	{-10, 0, 32}, {-34, 0, 7}, {15, 0, -13}, {6, 0, -15},
	{-0.75, 0, 0.3}
};

struct Car {
	MeshContainer mesh;
	float mass; // mass of car
	float engine; // engine force
	float roll; // rolling resistance
	float drag; // aerodynamic drag constant
	float last_pt = 0.0f;
	vec3 pos; // position
	vec3 dir; // direction
	vec3 vel; // velocity
	Car() { };
	Car(MeshContainer _mesh, float _mass, float _engine, float _roll, float _drag) {
		mesh = _mesh;
		mass = _mass;
		engine = _engine;
		roll = _roll;
		drag = _drag;
		pos = vec3(0, 0, 0);
		dir = vec3(1, 0, 0);
		vel = vec3(0, 0, 0);
	}
	void draw(mat4 depth_mvp = NULL) { 
		mat4 transform = Translate(pos) * Orientation(dir, vec3(0, 1, 0));
		mesh.draw(transform, depth_mvp == NULL ? NULL : depth_mvp); 
	}
	void drawDepth() {
		mat4 transform = Translate(pos) * Orientation(dir, vec3(0, 1, 0));
		mesh.drawDepth(transform);
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
	if (key == GLFW_KEY_P && action == GLFW_PRESS)
		printf("Pos: %.2f %.2f %.2f\n", car.pos.x, car.pos.y, car.pos.z);
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
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			if (monitor != NULL) {
                int videoModesCount;
                const GLFWvidmode* videoModes = glfwGetVideoModes(monitor, &videoModesCount);
                const GLFWvidmode highest = videoModes[videoModesCount - 1];
				glfwSetWindowMonitor(window, monitor, 0, 0, highest.width, highest.height, highest.refreshRate);
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
	double_ms sinceTitleUpdate = cur - lastTitleUpdate;
	if (sinceTitleUpdate.count() > 200.0f) {
		char title[30];
		snprintf(title, 30, "Drive: %.0f fps", (1.0f / dt) * 60.0f);
		glfwSetWindowTitle(window, title);
		lastTitleUpdate = cur;
	}
}

void render_imgui() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("Drive")) {
		if (ImGui::MenuItem("Quit", "CTRL + Q", false)) glfwSetWindowShouldClose(window, GLFW_TRUE);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Lighting")) {
		ImGui::ColorEdit3("Light Color", lightColor);
		if (ImGui::BeginCombo("Shadow Edge Samples", "", ImGuiComboFlags_NoPreview)) {
			if (ImGui::Selectable("2 Samples", shadowEdgeSamples == 2)) shadowEdgeSamples = 2;
			if (ImGui::Selectable("4 Samples", shadowEdgeSamples == 4)) shadowEdgeSamples = 4;
			if (ImGui::Selectable("8 Samples", shadowEdgeSamples == 8)) shadowEdgeSamples = 8;
			if (ImGui::Selectable("16 Samples", shadowEdgeSamples == 16)) shadowEdgeSamples = 16;
			if (ImGui::Selectable("32 Samples", shadowEdgeSamples == 32)) shadowEdgeSamples = 32;
			ImGui::EndCombo();
		}

		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void setup() {
	// Initialize GLFW callbacks
	glfwSetKeyCallback(window, Keyboard);
	glfwSetWindowSizeCallback(window, WindowResized);
    // Set up shadowmap resources
    if (!(shadowProgram = LinkProgramViaCode(&shadowVert, &shadowFrag)))
        throw runtime_error("Failed to compile shadow depth program!");
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
	mat4 car_transform = Scale(0.75f) * Translate(0, 0.38, 0) * RotateY(-90);
	MeshContainer car_mesh = MeshContainer("objects/car.obj", "textures/car.png", car_transform);
	car_mesh.allocate();
	// Mesh, mass, engine force, rolling resistance, air drag
	car = Car(car_mesh, 500.0, 3, 10.0, 10.0);
	car.pos = vec3(2, 0, 0);
	floor_mesh = MeshContainer(floor_points, floor_normals, floor_uvs, floor_triangles, "textures/racetrack.png");
	floor_mesh.allocate();
	mat4 large_tree_transform = Scale(2.0) * Translate(0, 0.9, 0);
	large_tree_mesh = MeshContainer("objects/largetree.obj", "textures/largetree.png", large_tree_transform);
	large_tree_mesh.allocate();
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
    glClearColor(0.651f, 0.961f, 0.941f, 1.0f);
    // TEX QUAD DEBUG
    if (!(quadProgram = LinkProgramViaCode(&quadVert, &quadFrag)))
        throw runtime_error("Failed to compile debug quad program!");
    glGenVertexArrays(1, &quadVArray);
    glBindVertexArray(quadVArray);
    glGenBuffers(1, &quadVBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizei)(quadPoints.size() * sizeof(vec2)), quadPoints.data(), GL_STATIC_DRAW);
    VertexAttribPointer(quadProgram, "point", 2, 0, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void cleanup() {
	// Cleanup meshes
	car.mesh.deallocate();
	floor_mesh.deallocate();
	large_tree_mesh.deallocate();
	for (dSkybox skybox : skyboxes)
		skybox.cleanup();
    particleSystem.cleanup();
	glDeleteFramebuffers(1, &shadowFramebuffer);
	glDeleteTextures(1, &shadowTexture);
    // TEX QUAD DEBUG
    glDeleteVertexArrays(1, &quadVArray);
    glDeleteBuffers(1, &quadVBuffer);
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
	floor_mesh.drawDepth(Scale(60));
	for (vec3 pos : large_tree_positions)
		large_tree_mesh.drawDepth(Translate(pos));
	car.drawDepth();
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
	floor_mesh.draw(Scale(60), depthVP);
	for (vec3 pos : large_tree_positions)
		large_tree_mesh.draw(Translate(pos), depthVP);
	car.draw(depthVP);
    particleSystem.draw(dt, camera.persp * camera.view, floor_mesh.texture, 60);
	skyboxes[cur_skybox].draw(camera.look - camera.loc, camera.persp);
	// DEBUG SHADOW TEXTURE
    /*
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, 1024, 1024);
    glUseProgram(quadProgram);
    glBindVertexArray(quadVArray);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    SetUniform(quadProgram, "tex", 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
     */
	render_imgui();
	glFlush();
}

int main() {
	srand((int)time(NULL));
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
	// Set up ImGui context 
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410 core");
	// Setup, icon loading
	load_icon();
	setup();
	time_p lastSim = sys_clock::now();
	while (!glfwWindowShouldClose(window)) {
		time_p cur = sys_clock::now();
		double_ms since = cur - lastSim;
		dt = 1 / (1000.0f / since.count() / 60.0f);
		lastSim = cur;
		update_title(cur);
		car.update(dt);
		car.collide();
		draw();
		glfwPollEvents();
		glfwSwapBuffers(window);
	}
	cleanup();
	// Cleaning up ImGui context
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	// Closing window and cleaning up GLFW
	glfwDestroyWindow(window);
	glfwTerminate();
}
