// Michael Pablo
// Assignment 5
// Using texture mapping and phong shading

#include <glad.h>
#include <glfw3.h>
#include "GLXtras.h"
#include <stdio.h>
#include <time.h>
#include "VecMat.h"
#include <vector>
#include "Mesh.h"
#include "Camera.h"
#include "Misc.h"

// GPU identifiers
GLuint vBuffer = 0;
GLuint program = 0;

const char *vertexShader = R"(
    #version 130
    in vec3 point, normal;
    in vec2 uv;
    out vec3 vPoint, vNormal; 
    out vec2 vUv;
    uniform mat4 persp;
    uniform mat4 view;
    void main() {
        vPoint = (view*vec4(point,1)).xyz;
        vNormal = (view*vec4(normal,0)).xyz;
        gl_Position = persp*view*vec4(point, 1);
        vUv = uv;
    }
)";

const char *pixelShader = R"(
    #version 130
	in vec3 vPoint, vNormal;
    in vec2 vUv;
	uniform vec3 light = vec3(10, 0, 0);
	uniform float amb = .05, dif = .7, spc = .5;  // lighting coefficients
	uniform sampler2D textureImage;
    out vec4 pColor;
	void main() {
		vec3 N = normalize(vNormal);          // surface normal
		vec3 L = normalize(light-vPoint);     // light vector
		vec3 E = normalize(vPoint);           // eye vector
		vec3 R = reflect(L, N);               // highlight vector
		float d = dif*max(0, dot(N, L));      // one-sided Lambert
		float h = max(0, dot(R, E));          // highlight term
		float s = spc*pow(h, 100);            // specular term
		float intensity = clamp(amb+d+s, 0, 1);
        vec3 color = texture(textureImage, vUv).rgb;
		pColor = vec4(intensity*color, 1);
	}
)";

// Global Variables 
int winW = 400, winH = 400;
Camera camera((float)winW / winH, vec3(0, 0, 0), vec3(0, 0, -5));
time_t startTime = clock();
const char* objFileName ="objects/sphere.obj";
const char* textureName = "textures/basketball.tga";
int textID = -1;
float rotSpeed = .3f;
vec2 mouseDown(0, 0); // location of last mouse down
vec2 rotOld(0,0), rotNew(0, 0);
vec3 tranOld(0, 0, 0), tranNew(0, 0, 0);
float tranSpeed = 0.1f;
float gravity = 9.8f;
bool isGrav = false;
// Obj Information
vector<vec3> normals;
vector<vec2> uvs;
vector<vec3> points;
vector<int3> triangles;

void Display() {
    // clear background
    glClearColor(1,1,1,1);
    glClear(GL_COLOR_BUFFER_BIT);
    // access GPU vertex buffer
    glUseProgram(program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
    // associate position input to shader with position array in vertex buffer
    int sizePts = points.size() * sizeof(vec3);
    VertexAttribPointer(program, "point", 3, 0, (void*)0);
    // Connect the normals in GPU memory to the vertex shader
    VertexAttribPointer(program, "normal", 3, 0, (void*)(points.size() * sizeof(vec3)));
    // rotate verticies using matricies
    SetUniform(program, "view", camera.modelview);
    SetUniform(program, "persp", camera.persp);
    // draw lines instead of triangles
    //for (int i = 0; i < (int)triangles.size(); i++) {
    //    glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, &triangles[i]);
    //} 
    //Connect the uvs in GPU memory to the vertex buffer
    int pSize = points.size() * sizeof(vec3), nSize = normals.size() * sizeof(vec3);
    VertexAttribPointer(program, "uv", 2, 0, (void*)(pSize + nSize));
    //Set the texture map identifier(for this program, it is 0) and make the texture “active”
    SetUniform(program, "textureImage", 0);
    glBindTexture(GL_TEXTURE_2D, textID);
    glActiveTexture(GL_TEXTURE0 + 0);
    // draw inside the triangles
    glDrawElements(GL_TRIANGLES, 3 * triangles.size(), GL_UNSIGNED_INT, &triangles[0]);
    glFlush();
}

void InitVertexBuffer() {
    // make GPU buffer for points & colors, set it active buffer
    glGenBuffers(1, &vBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
    // allocate space for three arrays and copy each (points, normals, and uvs may be different in size)
    int nPts = points.size(), nNrms = normals.size(), nUVS = uvs.size();
    int sizePts = nPts * sizeof(vec3), sizeNrms = nNrms * sizeof(vec3), sizeUVS = nUVS * sizeof(vec2);
    glBufferData(GL_ARRAY_BUFFER, sizePts + sizeNrms + sizeUVS, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizePts, points.data());
    glBufferSubData(GL_ARRAY_BUFFER, sizePts, sizeNrms, normals.data());
    glBufferSubData(GL_ARRAY_BUFFER, sizePts + sizeNrms, sizeUVS, uvs.data());
}

bool InitShader() {
    program = LinkProgramViaCode(&vertexShader, &pixelShader);
    if (!program)
        printf("can't init shader program\n");
    return program != 0;
}

// application

bool Shift(GLFWwindow* w) {
    return glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}

void MouseButton(GLFWwindow* w, int butn, int action, int mods) {
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    y = winH - y;
    if (action == GLFW_PRESS)
        camera.MouseDown((int)x, (int)y);
    if (action == GLFW_RELEASE)
        camera.MouseUp();
}

void MouseMove(GLFWwindow* w, double x, double y) {
    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) { // drag
        y = winH - y;
        camera.MouseDrag((int)x, (int)y, Shift(w));
    }
}

void MouseWheel(GLFWwindow* w, double ignore, double spin) {
    camera.MouseWheel(spin > 0, Shift(w));
}

void Keyboard(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (action == GLFW_PRESS && key == GLFW_KEY_G) {
        if (isGrav) {
            isGrav = !isGrav;
        } else {
            isGrav = !isGrav;
        }
    }
}

void ErrorGFLW(int id, const char *reason) {
    printf("GFLW error %i: %s\n", id, reason);
}

void Resize(GLFWwindow* window, int width, int height) {
    camera.Resize(winW = width, winH = height);
    glViewport(0, 0, winW, winH);
}

void Close() {
    // unbind vertex buffer and free GPU memory
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vBuffer);
}

int main() {
    glfwSetErrorCallback(ErrorGFLW);
    if (!glfwInit())
        return 1;
    GLFWwindow *w = glfwCreateWindow(winW, winH, "Obj Line File", NULL, NULL);
    if (!w) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    // read obj file and fit object into space
    if (!ReadAsciiObj((char*)objFileName, points, triangles, &normals, &uvs)) {
        printf("failed to read in the obj file\n");
    }
    printf("%i verticies, %i triangles \n ", points.size(), triangles.size());
    Normalize(points, .8f);
    printf("GL version: %s\n", glGetString(GL_VERSION));
    PrintGLErrors();
    // read image file
    textID = LoadTexture((char*)textureName, 0);
    if (!InitShader())
        return 0;
    InitVertexBuffer();
    // key callbacks
    glfwSetKeyCallback(w, Keyboard);
    glfwSetMouseButtonCallback(w, MouseButton);
    glfwSetScrollCallback(w, MouseWheel);
    glfwSetCursorPosCallback(w, MouseMove);
    glfwSetWindowSizeCallback(w, Resize);
    glfwSwapInterval(1); // ensure no generated frame backlog
    // event loop
    while (!glfwWindowShouldClose(w)) {
        
        Display();
        glfwSwapBuffers(w);
        glfwPollEvents();

    }
    Close();
    glfwDestroyWindow(w);
    glfwTerminate();
}
