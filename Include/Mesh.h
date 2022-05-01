// Mesh.h - 3D mesh of triangles (c) 2019-2022 Jules Bloomenthal

#ifndef MESH_HDR
#define MESH_HDR

#include <glad.h>
#include <stdio.h>
#include <vector>
#include "CameraArcball.h"
#include "VecMat.h"

using std::string;
using std::vector;

// Mesh Class and Operations

GLuint GetMeshShader();
GLuint UseMeshShader();

class Frame {
public:
	Frame() { };
	Frame(Quaternion q, vec3 o, float s) : orientation(q), origin(o), scale(s) { };
	Quaternion orientation;
	vec3 origin;
	float scale = 1;
};

class Mesh {
public:
	Mesh() { };
	~Mesh() { glDeleteBuffers(1, &vBufferId); };
	string objFilename, texFilename;
	// vertices and facets
	vector<vec3> points;
	vector<vec3> normals;
	vector<vec2> uvs;
	vector<int3> triangles;
	vector<int4> quads;
	// position/orientation
	mat4 transform;						// object to world space, set during drag
	Frame frameDown;					// reference frame on mouse down
	// hierarchy
	vector<Mesh *> children;
	// GPU vertex buffer and texture
	GLuint vao = 0;						// vertex array object
	GLuint vBufferId = 0;
	GLuint textureName = 0, textureUnit = 0;
	// operations
	void Buffer();
	void Display(CameraAB camera, bool lines = false);
	bool Read(string objFile, mat4 *m = NULL, bool normalize = true);
		// read in object file (with normals, uvs), initialize matrix, build vertex buffer
	bool Read(string objFile, string texFile, int textureUnit, mat4 *m = NULL, bool normalize = true);
		// read in object file (with normals, uvs) and texture file, initialize matrix, build vertex buffer
		// textureUnit must be > 0
};

class MeshFramer {
public:
	Mesh *mesh = NULL;
	Arcball arcball;
	MeshFramer() { }
	void Set(Mesh *m, float radius, mat4 fullview);
	void SetFramedown(Mesh *m);
		// set m.qstart from m.transform and recurse on m.children
	void RotateTransform(Mesh *m, Quaternion qrot, vec3 *center = NULL);
		// apply qrot to qstart, optionally rotate base around center
		// set m.transform, recurse on m.children
	void TranslateTransform(Mesh *m, vec3 pDif);
	bool Hit(int x, int y);
	void Down(int x, int y, mat4 modelview, mat4 persp, bool control = false);
	void Drag(int x, int y, mat4 modelview, mat4 persp);
	void Up();
	void Wheel(double spin, bool shift);
	void Draw(mat4 fullview);
private:
	bool moverPicked = false;
	Mover mover;
};

// Read STL Format

struct VertexSTL {
	vec3 point, normal;
	VertexSTL() { }
	VertexSTL(float *p, float *n) : point(vec3(p[0], p[1], p[2])), normal(vec3(n[0], n[1], n[2])) { }
};

int ReadSTL(const char *filename, vector<VertexSTL> &vertices);
	// read vertices from file, three per triangle; return # triangles

// Read OBJ Format

struct Group {
	string name;
	int startTriangle = 0, nTriangles = 0;
	Group(int start = 0, string n = "") : startTriangle(start), name(n) { }
};

struct Mtl {
	string name;
	vec3 ka, kd, ks;
	int startTriangle = 0, nTriangles = 0;
	Mtl() {startTriangle = -1, nTriangles = 0; }
	Mtl(int start, string n, vec3 a, vec3 d, vec3 s) : startTriangle(start), name(n), ka(a), kd(d), ks(s) { }
};

bool ReadAsciiObj(const char    *filename,                  // must be ASCII file
				  vector<vec3>  &points,                    // unique set of points determined by vertex/normal/uv triplets in file
				  vector<int3>  &triangles,                 // array of triangle vertex ids
				  vector<vec3>  *normals  = NULL,           // if non-null, read normals from file, correspond with points
				  vector<vec2>  *textures = NULL,           // if non-null, read uvs from file, correspond with points
				  vector<Group> *triangleGroups = NULL,     // correspond with triangle groups
				  vector<Mtl>   *triangleMtls = NULL,		// correspond with triangle groups
				  vector<int4>  *quads = NULL,              // optional quadrilaterals
				  vector<int2>  *segs = NULL);				// optional line segments
	// set points and triangles; normals, textures, quads optional
	// return true if successful

bool WriteAsciiObj(const char   *filename,
				   vector<vec3> &points,
				   vector<vec3> &normals,
				   vector<vec2> &uvs,
				   vector<int3> *triangles = NULL,
				   vector<int4> *quads = NULL,
				   vector<int2> *segs = NULL);
	// write to file mesh points, normals, and uvs
	// optionally write triangles and/or quadrilaterals

// Bounding Box

void Normalize(vector<vec3> &points, float scale = 1);
	// translate and apply uniform scale so that vertices fit in -scale,+scale in X,Y,Z

void Normalize(vector<VertexSTL> &vertices, float scale = 1);
	// as above

// Normals

void SetVertexNormals(vector<vec3> &points, vector<int3> &triangles, vector<vec3> &normals);
	// compute/recompute vertex normals as the average of surrounding triangle normals

// Intersection with a Line

struct TriInfo {
	vec4 plane;
	int majorPlane = 0; // 0: XY, 1: XZ, 2: YZ
	vec2 p1, p2, p3;    // vertices projected to majorPlane
	TriInfo() { };
	TriInfo(vec3 p1, vec3 p2, vec3 p3);
};

void BuildTriInfos(vector<vec3> &points, vector<int3> &triangles, vector<TriInfo> &triInfos);
	// for interactive selection

int IntersectWithLine(vec3 p1, vec3 p2, vector<TriInfo> &triInfos, float &alpha);
	// return triangle index of nearest intersected triangle, or -1 if none
	// intersection = p1+alpha*(p2-p1)

#endif
