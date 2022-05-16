// dMisc.h - Miscellaneous utilities

#ifndef DMISC_HDR
#define DMISC_HDR

#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <iterator>
#include "stb_image.h"
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::unordered_map;
using std::runtime_error;
using std::string;
using std::ifstream;
using std::getline;
using std::distance;
using std::find;

inline GLuint loadTexture(string filename, bool mipmap = true, GLint min_filter = GL_LINEAR, GLint mag_filter = GL_NEAREST) {
	// Load image from file, force RGBA
	int w, h;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc* data = stbi_load(filename.c_str(), &w, &h, 0, STBI_rgb_alpha);
	if (!data) 
		throw runtime_error("Failed to read '" + filename + "' : " + string(stbi_failure_reason()));
	// Create GL texture, copy image data
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	// Generate mipmap and set min / mag filters
	if (mipmap) glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	// Free image data and return texture
	stbi_image_free(data);
	return texture;
}

struct ObjData {
	vector<vec3> points;
	vector<vec3> normals;
	vector<vec2> uvs;
	vector<int3> indices;
};

inline ObjData readObj(string filename) {
	ObjData obj;
	// Open OBJ file
	ifstream file(filename);
	if (!file.is_open())
		throw runtime_error("Failed to read '" + filename + "'");
	// Read lines from file
	vector<string> v, vt, vn, f;
	string line;
	while (getline(file, line)) {
		if (line[0] == '#') continue;
		size_t sp = line.find(" ");
		string cmd = line.substr(0, sp);
		string val = line.substr(sp + 1, line.length() - sp);
		if (cmd == "v") v.push_back(val);
		else if (cmd == "vt") vt.push_back(val);
		else if (cmd == "vn") vn.push_back(val);
		else if (cmd == "f") f.push_back(val);
	}
	// Create unique set of vertices from faces and create int3 indices
	vector<string> verts;
	unordered_map<string, size_t> vertMap;
	for (string face : f) {
		char v1[20] = { '\0' }, v2[20] = { '\0' }, v3[20] = { '\0' }, v4[20] = { '\0' };
		int inds = sscanf(face.c_str(), "%s %s %s %s", &v1, &v2, &v3, &v4);
		string sv1(v1), sv2(v2), sv3(v3), sv4(v4);
		// i1
		int i1 = 0;
		if (vertMap.find(sv1) != vertMap.end()) {
			i1 = vertMap[sv1];
		} else {
			i1 = verts.size();
			vertMap[sv1] = i1;
			verts.push_back(sv1);
		}
		// i2
		int i2 = 0;
		if (vertMap.find(sv2) != vertMap.end()) {
			i2 = vertMap[sv2];
		} else {
			i2 = verts.size();
			vertMap[sv2] = i2;
			verts.push_back(sv2);
		}
		// i3
		int i3 = 0;
		if (vertMap.find(sv3) != vertMap.end()) {
			i3 = vertMap[sv3];
		} else {
			i3 = verts.size();
			vertMap[sv3] = i3;
			verts.push_back(sv3);
		}
		int3 tri = int3(i1, i2, i3);
		obj.indices.push_back(tri);
		if (inds == 4) {
			int i4 = 0;
			if (vertMap.find(sv4) != vertMap.end()) {
				i4 = vertMap[sv4];
			} else {
				i4 = verts.size();
				vertMap[sv4] = i4;
				verts.push_back(sv4);
			}
			int3 tri2 = int3(i3, i4, i1);
			obj.indices.push_back(tri2);
		}
	}
	// Create vertices from set
	for (size_t i = 0; i < verts.size(); i++) {
		int v_i, vt_i, vn_i;
		if (sscanf(verts[i].c_str(), "%d/%d/%d", &v_i, &vt_i, &vn_i) < 3)
			throw runtime_error("Error reading vertex attribute indices!");
		vec3 point;
		if (sscanf(v[v_i - 1].c_str(), "%f %f %f", &point.x, &point.y, &point.z) < 3)
			throw runtime_error("Error reading vertex position!");
		obj.points.push_back(point);
		vec2 uv;
		if (sscanf(vt[vt_i - 1].c_str(), "%f %f", &uv.x, &uv.y) < 2)
			throw runtime_error("Error reading vertex UV!");
		obj.uvs.push_back(uv);
		vec3 normal;
		if (sscanf(vn[vn_i - 1].c_str(), "%f %f %f", &normal.x, &normal.y, &normal.z) < 3)
			throw runtime_error("Error reading vertex normal!");
		obj.normals.push_back(normal);
	}
	file.close();
	return obj;
}

#endif
