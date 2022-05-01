// Scene.cpp - camera and meshes

#include <glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <time.h>
#include "CameraArcball.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Mesh.h"
#include "Misc.h"
#include "Scene.h"

Scene::~Scene() {
	for (size_t i = 0; i < meshes.size(); i++) {
		glDeleteBuffers(1, &meshes[i].vBufferId);
		glDeleteTextures(1, &meshes[i].textureName);
	}
}

bool Scene::AddMesh(const char *objName, const char *texName, mat4 *m) {
	int nmeshes = meshes.size();
	meshes.resize(nmeshes+1);
	Mesh &mesh = meshes[nmeshes];
	bool ok = texName && strlen(texName)?
			mesh.Read(string(objName), string(texName), texUnit++, m) :
			mesh.Read(string(objName), m);
	if (!ok) meshes.resize(nmeshes);
	return ok;
}

void Scene::WriteMatrix(FILE *file, mat4 &m) {
	fprintf(file, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n",
		m[0][0], m[0][1], m[0][2], m[0][3],
		m[1][0], m[1][1], m[1][2], m[1][3],
		m[2][0], m[2][1], m[2][2], m[2][3],
		m[3][0], m[3][1], m[3][2], m[3][3]);
}

bool Scene::ReadMatrix(FILE *file, mat4 &m) {
	char buf[500];
	if (fgets(buf, 500, file) == NULL)
		return false;
	int nitems = sscanf(buf, "%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f",
		&m[0][0], &m[0][1], &m[0][2], &m[0][3],
		&m[1][0], &m[1][1], &m[1][2], &m[1][3],
		&m[2][0], &m[2][1], &m[2][2], &m[2][3],
		&m[3][0], &m[3][1], &m[3][2], &m[3][3]);
	return nitems == 16;
}

void Scene::SaveScene(const char *filename, CameraAB camera) {
	FILE *file = fopen(filename, "w");
	if (!file) {
		printf("Can't write %s\n", filename);
		return;
	}
	WriteMatrix(file, camera.modelview);
	for (size_t i = 0; i < meshes.size(); i++) {
		Mesh &mesh = meshes[i];
		fprintf(file, "%s\n", mesh.objFilename.c_str());
		WriteMatrix(file, mesh.transform);
	}
	fclose(file);
	printf("Saved %i meshes\n", (int)meshes.size());
}

int Scene::ReadScene(const char *filename, CameraAB *camera) {
	char meshName[500], texName[500];
	FILE *file = fopen(filename, "r");
	if (!file)
		return 0;
	mat4 mv;
	if (!ReadMatrix(file, mv)) {
		printf("Can't read modelview\n");
		return 0;
	}
	if (camera) camera->SetModelview(mv);
	meshes.resize(0);
	while (fgets(meshName, 500, file) != NULL) {
		meshName[strlen(meshName)-1] = 0; // remove carriage-return
		fgets(texName, 500, file);
		texName[strlen(texName)-1] = 0;
		mat4 m;
		if (!ReadMatrix(file, m)) {
			printf("Can't read matrix\n");
			return 0;
		}
		AddMesh(meshName, texName, &m);
	}
	fclose(file);
	return meshes.size();
}

void Scene::ListScene() {
	int nmeshes = meshes.size();
	printf("%i meshes:\n", nmeshes);
	for (int i = 0; i < nmeshes; i++)
		printf("  %i: %s\n", i, meshes[i].objFilename.c_str());
}

void Scene::DeleteMesh() {
	char buf[501];
	int n = -1;
	printf("delete mesh number: ");
	gets(buf);
	if (sscanf(buf, "%i", &n) == 1 && n >= 0 && n < (int) meshes.size()) {
		printf("deleted mesh[%i]\n", n);
		meshes.erase(meshes.begin()+n);
	}
}
