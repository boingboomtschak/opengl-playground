// dMesh.h - Expanded version of Bloomenthal's 3D mesh class

#ifndef DMESH_HDR
#define DMESH_HDR

#include <glad.h>
#include <stdio.h>
#include <vector>
#include "Camera.h"
#include "VecMat.h"

using std::vector;

class dMesh {
public:
	dMesh() { };
	~dMesh();
	// obj and tex files
	char* objFilename = NULL;
	char* texFilename = NULL;
	// model data
	vector<vec3> points;
	vector<vec3> normals;
	vector<vec2> uvs;
	vector<int3> triangles;
	// model transform
	mat4 transform;
	//     vert array, vert buffer, index buffer, tex name, tex unit
	GLuint vArray = 0, vBuffer = 0, iBuffer = 0, texName = 0, texUnit = 0;
	bool preDisp = false;
	GLuint GetMeshShader();
	GLuint UseMeshShader();
	void Buffer();
	void PreDisplay();
	void Display(Camera camera, mat4 *m = NULL);
	bool Read(char* objFilename, mat4* m = NULL);
	bool Read(char* objFilename, char* texFilename, int texUnit, mat4* m = NULL);
};

#endif