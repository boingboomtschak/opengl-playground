// dMisc.h - Miscellaneous utilities

#ifndef DMISC_HDR
#define DMISC_HDR

#include <string>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <map>
#include <utility>
#include <iterator>
#include "stb_image.h"
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::map;
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

namespace {

struct int3compare {
	bool operator() (const int3& a, const int3& b) const {
		return a.i1 == b.i1 ? (a.i2 == b.i2 ? a.i3 < b.i3 : a.i2 < b.i2) : a.i1 < b.i1;
	}
};

};

inline ObjData readObj(string filename) {
	ObjData obj;
	// Open OBJ file
	ifstream file(filename);
	if (!file.is_open())
		throw runtime_error("Failed to read '" + filename + "'");
	// Read lines from file
	string line;
	vector<vec3> pts;
	vector<vec2> uvs;
	vector<vec3> nms;
	map<int3, int, int3compare> faceMap;
	while (getline(file, line)) {
		if (line[0] == '#') continue;
		size_t sp = line.find(" ");
		string cmd = line.substr(0, sp);
		string val = line.substr(sp + 1, line.length() - sp);
		if (cmd == "v") {
			vec3 v;
			if (sscanf(val.c_str(), "%f %f %f", &v.x, &v.y, &v.z) < 3)
				throw runtime_error("Error reading vertex position!");
			pts.push_back(v);
		} else if (cmd == "vt") {
			vec2 vt;
			if (sscanf(val.c_str(), "%f %f", &vt.x, &vt.y) < 2)
				throw runtime_error("Error reading vertex uv!");
			uvs.push_back(vt);
		} else if (cmd == "vn") {
			vec3 vn;
			if (sscanf(val.c_str(), "%f %f %f", &vn.x, &vn.y, &vn.z) < 3)
				throw runtime_error("Error reading vertex normal!");
			nms.push_back(vn);
		} else if (cmd == "f") {
			int3 v1, v2, v3, v4;
			int inds = sscanf(val.c_str(), "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", &v1.i1, &v1.i2, &v1.i3, &v2.i1, &v2.i2, &v2.i3, &v3.i1, &v3.i2, &v3.i3, &v4.i1, &v4.i2, &v4.i3);
			v1 = v1 - int3(1, 1, 1); v2 = v2 - int3(1, 1, 1); v3 = v3 - int3(1, 1, 1);
			// i1
			int i1;
			if (faceMap.find(v1) == faceMap.end()) {
				i1 = obj.points.size();
				faceMap[v1] = i1;
				obj.points.push_back(pts[v1.i1]);
				obj.uvs.push_back(uvs[v1.i2]);
				obj.normals.push_back(nms[v1.i3]);
			} else {
				i1 = faceMap[v1];
			}
			// i2
			int i2;
			if (faceMap.find(v2) == faceMap.end()) {
				i2 = obj.points.size();
				faceMap[v2] = i2;
				obj.points.push_back(pts[v2.i1]);
				obj.uvs.push_back(uvs[v2.i2]);
				obj.normals.push_back(nms[v2.i3]);
			} else {
				i2 = faceMap[v2];
			}
			// i3
			int i3;
			if (faceMap.find(v3) == faceMap.end()) {
				i3 = obj.points.size();
				faceMap[v3] = i3;
				obj.points.push_back(pts[v3.i1]);
				obj.uvs.push_back(uvs[v3.i2]);
				obj.normals.push_back(nms[v3.i3]);
			} else {
				i3 = faceMap[v3];
			}
			obj.indices.push_back(int3(i1, i2, i3));
			if (inds == 12) {
				v4 = v4 - int3(1, 1, 1);
				// i4
				int i4;
				if (faceMap.find(v4) == faceMap.end()) {
					i4 = obj.points.size();
					faceMap[v4] = i4;
					obj.points.push_back(pts[v4.i1]);
					obj.uvs.push_back(uvs[v4.i2]);
					obj.normals.push_back(nms[v4.i3]);
				} else {
					i4 = faceMap[v4];
				}
				obj.indices.push_back(int3(i3, i4, i1));
			}
		}
	}
	file.close();
	return obj;
}

#endif
