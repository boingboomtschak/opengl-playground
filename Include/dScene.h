// dScene.h - Static scene graph management and serialization/deserialization

#ifndef DSCENE_HDR
#define DSCENE_HDR

#include <vector>
#include <stdexcept>
#include <fstream>
#include <map>
#include "VecMat.h"
#include "dCamera.h"
#include "dMesh.h"
#include "dRenderPass.h"
#include "dMisc.h"

using std::vector;
using std::map;
using std::runtime_error;
using std::ifstream;
using std::getline;

struct MeshDetails {
	bool instanced = false;
	char culling = 'N'; // N = None, F = Frustum
};

struct SceneGraph {
	vector<Mesh> meshes;
	vector<vector<mat4>> transforms;
	vector<string> objs;
	vector<string> texs;
	vector<MeshDetails> details;
	SceneGraph() { };
	SceneGraph(string scgPath) {
		deserialize(scgPath);
	}
	void serialize(string path) {

	}
	void deserialize(string path) {
		// Open scg file
		ifstream file(path);
		if (!file.is_open())
			throw runtime_error("Failed to read scene graph file '" + path + "'");
		// Read file
		string line;
		int cur_mesh = -1;
		vector<mat4> instance_transforms;
		MeshDetails meshDetails;
		while (getline(file, line)) {
			if (line[0] == 'm') {
				instance_transforms.clear();
				char colliderType;
				char objPath[128]{ '\0' };
				char texPath[128]{ '\0' };
				if (sscanf(line.c_str(), "m %c %c '%s' '%s'", &colliderType, &meshDetails.culling, objPath, texPath) < 3)
					throw runtime_error("Error reading mesh details!");
				string objStr(objPath), texStr(texPath);
				Mesh m = Mesh(objStr, texStr, mat4());
				switch (colliderType) {
					case 'S':
						m.createCollider<Sphere>();
						break;
					case 'A':
						m.createCollider<AABB>();
						break;
					case 'O':
						m.createCollider<OBB>();
						break;
					case 'C':
						m.createCollider<ConvexHull>();
						break;
				}
				cur_mesh = meshes.size();
				meshes.push_back(m);
			} else if (strncmp(line.c_str(), "mdl", 3) == 0) {
				if (cur_mesh == -1)
					throw runtime_error("Received model matrix before mesh specifier!");
				mat4 m;
				if (sscanf(line.c_str(), "mdl\t%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f", &m[0][0], &m[0][1], &m[0][2], &m[0][3], &m[1][0], &m[1][1], &m[1][2], &m[1][3], &m[2][0], &m[2][1], &m[2][2], &m[2][3], &m[3][0], &m[3][1], &m[3][2], &m[3][3]) < 16)
					throw runtime_error("Error reading model matrix!");
				meshes[cur_mesh].model = m;
			} else if (line[0] == '!') {
				if (cur_mesh == -1)
					throw runtime_error("Received instance matrix before mesh specified!");
				mat4 m;
				if (sscanf(line.c_str(), "!\t%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f", &m[0][0], &m[0][1], &m[0][2], &m[0][3], &m[1][0], &m[1][1], &m[1][2], &m[1][3], &m[2][0], &m[2][1], &m[2][2], &m[2][3], &m[3][0], &m[3][1], &m[3][2], &m[3][3]) < 16)
					throw runtime_error("Error reading instance matrix!");
				instance_transforms.push_back(m);
			} else if (line[0] == 'e') {
				if (instance_transforms.size() <= 1) {
					if (instance_transforms.size() == 1)
						meshes[cur_mesh].model = instance_transforms[0] * meshes[cur_mesh].model;
				} else {
					meshDetails.instanced = true;
					meshes[cur_mesh].setupInstanceBuffer((GLsizei)instance_transforms.size());
					meshes[cur_mesh].loadInstances(instance_transforms);
					transforms.resize(meshes.size());
					transforms[cur_mesh] = instance_transforms;
				}
				details.push_back(meshDetails);
				meshDetails = MeshDetails();
				cur_mesh = -1;
			} 
		}
	}
	void addMesh() {

	}
	void addMesh() {

	}
	void render(RenderPass pass) {

	}
	void frustumCull(Camera cam) {
		Frustum frustum(cam);
		// TODO iterate through meshes, cull, load instances
	}
	void cleanup() {

	}
};


#endif