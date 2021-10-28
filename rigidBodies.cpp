// rigidBodies.cpp : Devon McKee

#define _USE_MATH_DEFINES

#include "glad.h"
#include "GLFW/glfw3.h"
#include "GLXtras.h"
#include <time.h>
#include <vector>
#include "VecMat.h"
#include "Camera.h"
#include "Mesh.h"

GLuint vBuffer = 0;
GLuint program = 0;

int win_width = 800;
int win_height = 800;

Camera camera((float)win_width / win_height, vec3(0, 0, 0), vec3(0, 0, -5));

vec3 lightSource = vec3(1, 1, 0);

