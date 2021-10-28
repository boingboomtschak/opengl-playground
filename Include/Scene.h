// Scene.h - camera and meshes

#ifndef SCENE_HDR
#define SCENE_HDR

#include <vector>
#include "CameraArcball.h"
#include "Mesh.h"
#include "VecMat.h"

class Scene {
public:
	std::vector<Mesh> meshes;
	int texUnit = 1;
	bool AddMesh(const char *objName, const char *texName = NULL, mat4 *m = NULL);
	void WriteMatrix(FILE *file, mat4 &m);
	bool ReadMatrix(FILE *file, mat4 &m);
	void SaveScene(const char *filename, CameraAB camera);
	int ReadScene(const char *filename, CameraAB *camera = NULL); // return number meshes read
	void ListScene();
	void DeleteMesh();
	Scene(int tUnit = 1) : texUnit(tUnit) { };
	~Scene();
};

#endif
