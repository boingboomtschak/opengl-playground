// dCube.cpp : Implementation for dCube.h

#include "dCube.h"
#include "GLXtras.h"
#include "Misc.h"

namespace {
	GLuint cubeShader = 0;

	const char* cubeVertShader = R"(
		#version 410 core
		in vec3 point;
		uniform mat4 modelview;
		uniform mat4 persp;
		void main() {
			gl_Position = persp * modelview * vec4(point, 1);
		}
	)";

	const char* cubeFragShader = R"(
		#version 410 core
		out vec4 pColor;
		void main() {
			pColor = vec4(1);
		}
	)";
}

void dCube::loadBuffer() {
	int shader = getCubeShader();
	glGenVertexArrays(1, &vArray);
	glBindVertexArray(vArray);
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	size_t sizePts = points.size() * sizeof(vec3);
	glBufferData(GL_ARRAY_BUFFER, sizePts, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizePts, points.data());
	glGenBuffers(1, &iBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size()*sizeof(int4), faces.data(), GL_STATIC_DRAW);
	VertexAttribPointer(shader, "point", 3, 0, (void*)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void dCube::unloadBuffer() {
	glDeleteVertexArrays(1, &vArray);
	glDeleteBuffers(1, &vBuffer);
	glDeleteBuffers(1, &iBuffer);
}

void dCube::display(Camera camera, mat4 *m) {
	if (vBuffer == 0) {
		fprintf(stderr, "dCube: Cube buffer not loaded into memory\n");
		return;
	}
	glBindVertexArray(vArray);
	int shader = useCubeShader();
	SetUniform(shader, "persp", camera.persp);
	mat4 modelview = camera.modelview;
	if (m != NULL) modelview = modelview * *m;
	modelview = modelview * transform;
	SetUniform(shader, "modelview", modelview);
	for (size_t i = 0; i < 6; i++) 
		glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, (GLvoid*)(i * sizeof(int4)));
	//glDrawElements(GL_LINE_LOOP, 25, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

GLuint dCube::getCubeShader() {
	if (!cubeShader)
		cubeShader = LinkProgramViaCode(&cubeVertShader, &cubeFragShader);
	return cubeShader;
}

GLuint dCube::useCubeShader() {
	GLuint s = getCubeShader();
	GLint prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
	if (prog != s) {
		glUseProgram(s);
	}
	return s;
}
