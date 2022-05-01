// Mesh.cpp - mesh IO and operations (c) 2019-2022 Jules Bloomenthal

#include "CameraArcball.h"
#include "GLXtras.h"
#include "Draw.h"
#include "Mesh.h"
#include "Misc.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <direct.h>
#include <float.h>
#include <string.h>
#include <cstdlib>

using std::string;
using std::vector;
using std::ios;
using std::ifstream;

// Mesh Framer

bool MeshFramer::Hit(int x, int y) {
	return arcball.Hit(x, y);
}

void MeshFramer::Up() {
	arcball.Up();
}

void MeshFramer::Set(Mesh *m, float radius, mat4 fullview) {
	mesh = m;
	m->frameDown = Frame(Quaternion(m->transform), MatrixOrigin(m->transform), MatrixScale(m->transform));
	arcball.SetBody(m->transform, radius);
	arcball.SetCenter(ScreenPoint(m->frameDown.origin, fullview));
	moverPicked = false;
}

void MeshFramer::SetFramedown(Mesh *m) {
	m->frameDown = Frame(Quaternion(m->transform), MatrixOrigin(m->transform), MatrixScale(m->transform));
	for (int i = 0; i < (int) m->children.size(); i++)
		SetFramedown(m->children[i]);
}

void MeshFramer::Down(int x, int y, mat4 modelview, mat4 persp, bool control) {
	moverPicked = arcball.MouseOver(x, y);
	SetFramedown(mesh);
	if (moverPicked)
		mover.Down(&mesh->frameDown.origin, x, y, modelview, persp);
	else
		arcball.Down(x, y, control, &mesh->transform);
			// mesh->transform used by arcball.SetNearestAxis
}

void MeshFramer::Drag(int x, int y, mat4 modelview, mat4 persp) {
	if (moverPicked) {
		vec3 pDif = mover.Drag(x, y, modelview, persp);
		SetMatrixOrigin(mesh->transform, mesh->frameDown.origin);
		for (int i = 0; i < (int) mesh->children.size(); i++)
			TranslateTransform(mesh->children[i], pDif);
		arcball.SetCenter(ScreenPoint(mesh->frameDown.origin, persp*modelview));
	}
	else {
		Quaternion qrot = arcball.Drag(x, y);
		RotateTransform(mesh, qrot, NULL);
	}
}

void MeshFramer::RotateTransform(Mesh *m, Quaternion qrot, vec3 *center) {
	// rotate selected mesh and child meshes by qrot (returned by Arcball::Drag)
	//   apply qrot to rotation elements of m->transform (upper left 3x3)
	//   if non-null center, rotate origin of m about center
	// recursive routine initially called with null center
	Quaternion qq = m->frameDown.orientation*qrot; // arcball:use=Camera(?) works (qrot*m->qstart Body? fails)
	// rotate m
	qq.SetMatrix(m->transform, m->frameDown.scale);
	if (center) {
		// this is a child mesh: rotate origin of mesh around center
		mat4 rot = qrot.GetMatrix();
		mat4 x = Translate((*center))*rot*Translate(-(*center));
		vec4 xbase = x*vec4(m->frameDown.origin, 1);
		SetMatrixOrigin(m->transform, vec3(xbase.x, xbase.y, xbase.z));
	}
	for (int i = 0; i < (int) m->children.size(); i++)
		RotateTransform(m->children[i], qrot, center? center : &m->frameDown.origin);
			// rotate descendant children around initial mesh base  
}

void MeshFramer::TranslateTransform(Mesh *m, vec3 pDif) {
	SetMatrixOrigin(m->transform, m->frameDown.origin+pDif);
	for (int i = 0; i < (int) m->children.size(); i++)
		TranslateTransform(m->children[i], pDif);
}

void MeshFramer::Wheel(double spin, bool shift) {
	mesh->frameDown.scale *= (spin > 0? 1.01f : .99f);
	Scale3x3(mesh->transform, mesh->frameDown.scale/MatrixScale(mesh->transform));
}

void MeshFramer::Draw(mat4 fullview) {
	UseDrawShader(ScreenMode());
	arcball.Draw(Control(), &mesh->transform);
	UseDrawShader(fullview);
	Disk(mesh->frameDown.origin, 9, arcball.pink);
}

namespace {

GLuint meshShader = 0;

// Mesh Shaders

const char *meshVertexShader = R"(
	#version 330
	layout (location = 0) in vec3 point;
	layout (location = 1) in vec3 normal;
	layout (location = 2) in vec2 uv;
	layout (location = 3) in mat4 instance; // for use with glDrawArrays/ElementsInstanced
	layout (location = 7) in vec3 color;	// unused?
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vUv;
	out vec3 vColor;
	uniform bool useInstance = false;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		mat4 m = useInstance? modelview*instance : modelview;
		vPoint = (m*vec4(point, 1)).xyz;
		vNormal = (m*vec4(normal, 0)).xyz;
		gl_Position = persp*vec4(vPoint, 1);
		vUv = uv;
		vColor = color;
	}
)";

const char *meshPixelShader = R"(
	#version 330
	in vec3 vPoint;
	in vec3 vNormal;
	in vec2 vUv;
	in vec3 vColor;
	out vec4 pColor;
	uniform vec3 light;
	uniform vec3 lights[20];
	uniform int nlights = 0;
	uniform vec3 defaultColor = vec3(1);
	uniform bool useDefaultColor = true;
	uniform float opacity = 1;
	uniform sampler2D textureName;
	uniform bool useTexture = false;
	uniform bool useTint = false;
	uniform bool fwdFacing = false;
	float Intensity(vec3 normalV, vec3 eyeV, vec3 point, vec3 light) {
		vec3 lightV = normalize(light-point);		// light vector
		vec3 reflectV = reflect(lightV, normalV);   // highlight vector
		float d = max(0, dot(normalV, lightV));     // one-sided diffuse
		float s = max(0, dot(reflectV, eyeV));      // one-sided specular
		return clamp(d+pow(s, 50), 0, 1);
	}
	void main() {
		vec3 N = normalize(vNormal);				// surface normal
		if (fwdFacing && N.z < 0) discard;
		vec3 E = normalize(vPoint);					// eye vector
		float intensity = 0;
		if (nlights == 0) intensity = Intensity(N, E, vPoint, light);
		for (int i = 0; i < nlights; i++)
			intensity += Intensity(N, E, vPoint, lights[i]);
		intensity = clamp(intensity, 0, 1);
		vec3 color = useTexture? texture(textureName, vUv).rgb : useDefaultColor? defaultColor : vColor;
		if (useTexture && useTint) {
			color.r *= defaultColor.r;
			color.g *= defaultColor.g;
			color.b *= defaultColor.b;
		}
		pColor = vec4(intensity*color, opacity); // 1);
	}
)";

} // end namespace

GLuint GetMeshShader() {
	if (!meshShader)
		meshShader = LinkProgramViaCode(&meshVertexShader, &meshPixelShader);
	return meshShader;
}

GLuint UseMeshShader() {
	GLuint s = GetMeshShader();
	glUseProgram(s);
	return s;
}

// Mesh Class

void Enable(int id, int ncomps, int offset) {
	glEnableVertexAttribArray(id);
	glVertexAttribPointer(id, ncomps, GL_FLOAT, GL_FALSE, 0, (void *) offset);
}

void Mesh::Buffer() {
	int nPts = points.size(), nNrms = normals.size(), nUvs = uvs.size();
	if (!nPts) { printf("mesh missing points\n"); return; }
	// create vertex buffer
	glGenBuffers(1, &vBufferId);
	glBindBuffer(GL_ARRAY_BUFFER, vBufferId);
	// allocate GPU memory for vertex locations and colors
	int sizePoints = nPts*sizeof(vec3), sizeNormals = nNrms*sizeof(vec3), sizeUvs = nUvs*sizeof(vec2);
	int bufferSize = sizePoints+sizeUvs+sizeNormals;
	glBufferData(GL_ARRAY_BUFFER, bufferSize, NULL, GL_STATIC_DRAW);
	// load data to buffer
	if (nPts) glBufferSubData(GL_ARRAY_BUFFER, 0, sizePoints, &points[0]);
	if (nNrms) glBufferSubData(GL_ARRAY_BUFFER, sizePoints, sizeNormals, &normals[0]);
	if (nUvs) glBufferSubData(GL_ARRAY_BUFFER, sizePoints+sizeNormals, sizeUvs, &uvs[0]);
	// create vertex array object for mesh
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	// enable attributes
	if (nPts) Enable(0, 3, 0);						// VertexAttribPointer(shader, "point", 3, 0, (void *) 0);
	if (nNrms) Enable(1, 3, sizePoints);			// VertexAttribPointer(shader, "normal", 3, 0, (void *) sizePoints);
	if (nUvs) Enable(2, 2, sizePoints+sizeNormals); // VertexAttribPointer(shader, "uv", 2, 0, (void *) (sizePoints+sizeNormals));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Mesh::Display(CameraAB camera, bool lines) {
	int nTris = triangles.size(), nQuads = quads.size();
	bool useTexture = textureUnit > 0 && uvs.size() > 0;
	// enable shader and vertex array object
	int shader = UseMeshShader();
	glBindVertexArray(vao);
	// texture
	SetUniform(shader, "useTexture", useTexture);
	if (useTexture) {
		glActiveTexture(GL_TEXTURE0+textureName);   // Unit? active texture corresponds with textureUnit or textureName?
		glBindTexture(GL_TEXTURE_2D, textureName);  // bound texture and shader id correspond with textureName
		SetUniform(shader, "textureName", (int) textureName);
	}
	// set custom transform and draw (xform = mesh transforms X view transform)
	SetUniform(shader, "modelview", camera.modelview*transform);
	SetUniform(shader, "persp", camera.persp);
	if (lines) {
		for (int i = 0; i < nTris; i++)
			glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, &triangles[i]);
		for (int i = 0; i < nQuads; i++)
			glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, &quads[i]);
	}
	else {
		glDrawElements(GL_TRIANGLES, 3*nTris, GL_UNSIGNED_INT, triangles.data());
		glDrawElements(GL_QUADS, 4*nQuads, GL_UNSIGNED_INT, quads.data());
	}
	glBindVertexArray(0);
}

bool Mesh::Read(string objFile, mat4 *m, bool normalize) {
	if (!ReadAsciiObj((char *) objFile.c_str(), points, triangles, &normals, &uvs, NULL, NULL, &quads)) {
		printf("Mesh.Read: can't read %s\n", objFile.c_str());
		return false;
	}
	objFilename = objFile;
	if (normalize)
		Normalize(points, 1);
	Buffer();
	if (m)
		transform = *m;
	return true;
}

bool Mesh::Read(string objFile, string texFile, int texUnit, mat4 *m, bool normalize) {
	if (!Read(objFile, m, normalize))
		return false;
	if (!texUnit) {
		printf("Mesh.Read: bad texture unit\n");
		return false;
	}
	objFilename = objFile;
	texFilename = texFile;
	textureUnit = texUnit;
	textureName = LoadTexture((char *) texFile.c_str(), textureUnit);
	if (!textureName)
		printf("Mesh.Read: bad texture name\n");
	return textureName > 0;
}

// intersections

vec2 MajPln(vec3 &p, int mp) { return mp == 1? vec2(p.y, p.z) : mp == 2? vec2(p.x, p.z) : vec2(p.x, p.y); }

TriInfo::TriInfo(vec3 a, vec3 b, vec3 c) {
	vec3 v1(b-a), v2(c-b), x = normalize(cross(v1, v2));
	plane = vec4(x.x, x.y, x.z, -dot(a, x));
	float ax = fabs(x.x), ay = fabs(x.y), az = fabs(x.z);
	majorPlane = ax > ay? (ax > az? 1 : 3) : (ay > az? 2 : 3);
	p1 = MajPln(a, majorPlane);
	p2 = MajPln(b, majorPlane);
	p3 = MajPln(c, majorPlane);
}

bool LineIntersectPlane(vec3 p1, vec3 p2, vec4 plane, vec3 *intersection, float *alpha) {
  vec3 normal(plane.x, plane.y, plane.z);
  vec3 axis(p2-p1);
  float pdDot = dot(axis, normal);
  if (fabs(pdDot) < FLT_MIN)
	  return false;
  float a = (-plane.w-dot(p1, normal))/pdDot;
  if (intersection != NULL)
	  *intersection = p1+a*axis;
  if (alpha)
	  *alpha = a;
  return true;
}

static bool IsZero(float d) { return d < FLT_EPSILON && d > -FLT_EPSILON; };

int CompareVs(vec2 &v1, vec2 &v2) {
	if ((v1.y > 0 && v2.y > 0) ||           // edge is fully above query point p'
		(v1.y < 0 && v2.y < 0) ||           // edge is fully below p'
		(v1.x < 0 && v2.x < 0))             // edge is fully left of p'
		return 0;                           // can't cross
	float zcross = v2.y*v1.x-v1.y*v2.x;     // right-handed cross-product
	zcross /= length(v1-v2);
	if (IsZero(zcross) && (v1.x <= 0 || v2.x <= 0))
		return 1;                           // on or very close to edge
	if ((v1.y > 0 || v2.y > 0) && ((v1.y-v2.y < 0) != (zcross < 0)))
		return 2;                           // edge is crossed
	else
		return 0;                           // edge not crossed
}

bool IsInside(const vec2 &p, const vec2 &a, const vec2 &b, const vec2 &c) {
	bool odd = false;
	vec2 q = p, v2 = c-q;
	for (int n = 0; n < 3; n++) {
		vec2 v1 = v2;
		v2 = (n==0? a : n==1? b : c)-q;
		if (CompareVs(v1, v2) == 2)
			odd = !odd;
	}
	return odd;
}

void BuildTriInfos(vector<vec3> &points, vector<int3> &triangles, vector<TriInfo> &triInfos) {
	triInfos.resize(triangles.size());
	for (size_t i = 0; i < triangles.size(); i++) {
		int3 &t = triangles[i];
		triInfos[i] = TriInfo(points[t.i1], points[t.i2], points[t.i3]);
	}
}

int IntersectWithLine(vec3 p1, vec3 p2, vector<TriInfo> &triInfos, float &retAlpha) {
	int picked = -1;
	float alpha, minAlpha = FLT_MAX;
	for (size_t i = 0; i < triInfos.size(); i++) {
		TriInfo &t = triInfos[i];
		vec3 inter;
		if (LineIntersectPlane(p1, p2, t.plane, &inter, &alpha)) {
			if (alpha < minAlpha) {
				if (IsInside(MajPln(inter, t.majorPlane), t.p1, t.p2, t.p3)) {
					minAlpha = alpha;
					picked = i;
				}
			}
		}
	}
	retAlpha = minAlpha;
	return picked;
}

// center/scale for unit size models

void UpdateMinMax(vec3 p, vec3 &min, vec3 &max) {
	for (int k = 0; k < 3; k++) {
		if (p[k] < min[k]) min[k] = p[k];
		if (p[k] > max[k]) max[k] = p[k];
	}
}

float GetScaleCenter(vec3 &min, vec3 &max, float scale, vec3 &center) {
	center = .5f*(min+max);
	float maxrange = 0;
	for (int k = 0; k < 3; k++)
		if ((max[k]-min[k]) > maxrange)
			maxrange = max[k]-min[k];
	return scale*2.f/maxrange;
}

// normalize STL models

void MinMax(vector<VertexSTL> &points, vec3 &min, vec3 &max) {
	min.x = min.y = min.z = FLT_MAX;
	max.x = max.y = max.z = -FLT_MAX;
	for (int i = 0; i < (int) points.size(); i++)
		UpdateMinMax(points[i].point, min, max);
}

void Normalize(vector<VertexSTL> &vertices, float scale) {
	vec3 min, max, center;
	MinMax(vertices, min, max);
	float s = GetScaleCenter(min, max, scale, center);
	for (int i = 0; i < (int) vertices.size(); i++) {
		vec3 &v = vertices[i].point;
		v = s*(v-center);
	}
}

// normalize vec3 models

void MinMax(vector<vec3> &points, vec3 &min, vec3 &max) {
	min[0] = min[1] = min[2] = FLT_MAX;
	max[0] = max[1] = max[2] = -FLT_MAX;
	for (int i = 0; i < (int) points.size(); i++) {
		vec3 &v = points[i];
		for (int k = 0; k < 3; k++) {
			if (v[k] < min[k]) min[k] = v[k];
			if (v[k] > max[k]) max[k] = v[k];
		}
	}
}

void Normalize(vector<vec3> &points, float scale) {
	vec3 min, max;
	MinMax(points, min, max);
	vec3 center(.5f*(min[0]+max[0]), .5f*(min[1]+max[1]), .5f*(min[2]+max[2]));
	float maxrange = 0;
	for (int k = 0; k < 3; k++)
		if ((max[k]-min[k]) > maxrange)
			maxrange = max[k]-min[k];
	float s = scale*2.f/maxrange;
	for (int i = 0; i < (int) points.size(); i++) {
		vec3 &v = points[i];
		for (int k = 0; k < 3; k++)
			v[k] = s*(v[k]-center[k]);
	}
}

void SetVertexNormals(vector<vec3> &points, vector<int3> &triangles, vector<vec3> &normals) {
	// size normals array and initialize to zero
	int nverts = (int) points.size();
	normals.resize(nverts, vec3(0,0,0));
	// accumulate each triangle normal into its three vertex normals
	for (int i = 0; i < (int) triangles.size(); i++) {
		int3 &t = triangles[i];
		vec3 &p1 = points[t.i1], &p2 = points[t.i2], &p3 = points[t.i3];
		vec3 a(p2-p1), b(p3-p2), n(normalize(cross(a, b)));
		normals[t.i1] += n;
		normals[t.i2] += n;
		normals[t.i3] += n;
	}
	// set to unit length
	for (int i = 0; i < nverts; i++)
		normals[i] = normalize(normals[i]);
}

// ASCII support

bool ReadWord(char* &ptr, char *word, int charLimit) {
	ptr += strspn(ptr, " \t");                  // skip white space
	int nChars = strcspn(ptr, " \t");           // get # non-white-space characters
	if (!nChars)
		return false;                           // no non-space characters
	int nRead = charLimit-1 < nChars? charLimit-1 : nChars;
	strncpy(word, ptr, nRead);
	word[nRead] = 0;                            // strncpy does not null terminate
	ptr += nChars;
		return true;
}

// STL

char *Lower(char *word) {
	for (char *c = word; *c; c++)
		*c = tolower(*c);
	return word;
}

int ReadSTL(const char *filename, vector<VertexSTL> &vertices) {
	// the facet normal should point outwards from the solid object; if this is zero,
	// most software will calculate a normal from the ordered triangle vertices using the right-hand rule
	class Helper {
	public:
		bool status = false;
		int nTriangles = 0;
		vector<VertexSTL> *verts;
		vector<string> vSpecs;                              // ASCII only
		Helper(const char *filename, vector<VertexSTL> *verts) : verts(verts) {
			char line[1000], word[1000], *ptr = line;
			ifstream inText(filename, ios::in);             // text default mode
			inText.getline(line, 10);
			bool ascii = ReadWord(ptr, word, 10) && !strcmp(Lower(word), "solid");
			// bool ascii = ReadWord(ptr, word, 10) && !_stricmp(word, "solid");
			ascii = false; // hmm!
			if (ascii)
				status = ReadASCII(inText);
			inText.close();
			if (!ascii) {
				FILE *inBinary = fopen(filename, "rb");     // inText.setmode(ios::binary) fails
				if (inBinary) {
					nTriangles = 0;
					status = ReadBinary(inBinary);
					fclose(inBinary);
				}
				else
					status = false;
			}
		}
		bool ReadASCII(ifstream &in) {
			printf("can't read ASCII STL\n");
			return true;
		}
		bool ReadBinary(FILE *in) {
				  // # bytes      use                  significance
				  // -------      ---                  ------------
				  //      80      header               none
				  //       4      unsigned long int    number of triangles
				  //      12      3 floats             triangle normal
				  //      12      3 floats             x,y,z for vertex 1
				  //      12      3 floats             vertex 2
				  //      12      3 floats             vertex 3
				  //       2      unsigned short int   attribute (0)
				  // endianness is assumed to be little endian
			// in.setmode(ios::binary); doc says setmode good, but compiler says not so
			// sizeof(bool)=1, sizeof(char)=1, sizeof(short)=2, sizeof(int)=4, sizeof(float)=4
			char buf[81];
			int nTriangle = 0;//, vid1, vid2, vid3;
			if (fread(buf, 1, 80, in) != 80) // header
				return false;
			if (fread(&nTriangles, sizeof(int), 1, in) != 1)
				return false;
			while (!feof(in)) {
				vec3 v[3], n;
				if (nTriangle == nTriangles)
					break;
				if (nTriangles > 5000 && nTriangle && nTriangle%1000 == 0)
					printf("\rread %i/%i triangles", nTriangle, nTriangles);
				if (fread(&n.x, sizeof(float), 3, in) != 3)
					printf("\ncan't read triangle %d normal\n", nTriangle);
				for (int k = 0; k < 3; k++)
					if (fread(&v[k].x, sizeof(float), 3, in) != 3)
						printf("\ncan't read vid %d\n", verts->size());
				vec3 a(v[1]-v[0]), b(v[2]-v[1]);
				vec3 ntmp = cross(a, b);
				if (dot(ntmp, n) < 0) {
					vec3 vtmp = v[0];
					v[0] = v[2];
					v[2] = vtmp;
				}
				for (int k = 0; k < 3; k++)
					verts->push_back(VertexSTL((float *) &v[k].x, (float *) &n.x));
				unsigned short attribute;
				if (fread(&attribute, sizeof(short), 1, in) != 1)
					printf("\ncan't read attribute\n");
				nTriangle++;
			}
			printf("\r\t\t\t\t\t\t\r");
			return true;
		}
	};
	Helper h(filename, &vertices);
	return h.nTriangles;
} // end ReadSTL

// ASCII OBJ

#include <map>

static const int LineLim = 10000, WordLim = 1000;

struct CompareS {
	bool operator() (const string &a, const string &b) const { return (a < b); }
};

typedef std::map<string, Mtl, CompareS> MtlMap;
	// string is key, Mtl is value

MtlMap ReadMaterial(const char *filename) {
	MtlMap mtlMap;
	char line[LineLim], word[WordLim];
	Mtl m;
	FILE *in = fopen(filename, "r");
	string key;
	Mtl value;
	if (in)
		for (int lineNum = 0;; lineNum++) {
			line[0] = 0;
			fgets(line, LineLim, in);                   // \ line continuation not supported
			if (feof(in))                               // hit end of file
				break;
			if (strlen(line) >= LineLim-1) {            // getline reads LineLim-1 max
				printf("line %d too long\n", lineNum);
				continue;
			}
			line[strlen(line)-1] = 0;							// remove carriage-return
			char *ptr = line;
			if (!ReadWord(ptr, word, WordLim) || *word == '#')
				continue;
			Lower(word);
			if (!strcmp(word, "newmtl") && ReadWord(ptr, word, WordLim)) {
				key = string(word);
				value.name = string(word);
			}
			if (!strcmp(word, "kd")) {
				if (sscanf(ptr, "%g%g%g", &value.kd.x, &value.kd.y, &value.kd.z) != 3)
					printf("bad line %d in material file", lineNum);
				else
					mtlMap[key] = value;
			}
		}
	else printf("can't open %s\n", filename);
	return mtlMap;
}

struct CompareVid {
	bool operator() (const int3 &a, const int3 &b) const {
		return (a.i1==b.i1? (a.i2==b.i2? a.i3 < b.i3 : a.i2 < b.i2) : a.i1 < b.i1);
	}
};

typedef std::map<int3, int, CompareVid> VidMap;
	// int3 is key, int is value

bool ReadAsciiObj(const char    *filename,
				  vector<vec3>  &points,
				  vector<int3>  &triangles,
				  vector<vec3>  *normals,
				  vector<vec2>  *textures,
				  vector<Group> *triangleGroups,
				  vector<Mtl>   *triangleMtls,
				  vector<int4>  *quads,
				  vector<int2>  *segs) {
	// read 'object' file (Alias/Wavefront .obj format); return true if successful;
	// polygons are assumed simple (ie, no holes and not self-intersecting);
	// some file attributes are not supported by this implementation;
	// obj format indexes vertices from 1
	FILE *in = fopen(filename, "r");
	if (!in)
		return false;
	vec2 t;
	vec3 v;
	int group = 0;
	char line[LineLim], word[WordLim];
	vector<vec3> tmpVertices, tmpNormals;
	vector<vec2> tmpTextures;
	VidMap vidMap;
	MtlMap mtlMap;
	for (int lineNum = 0;; lineNum++) {
		line[0] = 0;
		fgets(line, LineLim, in);                           // \ line continuation not supported
		if (feof(in))                                       // hit end of file
			break;
		if (strlen(line) >= LineLim-1) {                    // getline reads LineLim-1 max
			printf("line %d too long\n", lineNum);
			return false;
		}
		line[strlen(line)-1] = 0;							// remove carriage-return
		char *ptr = line;
		if (!ReadWord(ptr, word, WordLim))
			continue;
		Lower(word);
		if (*word == '#')
			continue;
		else if (!strcmp(word, "mtllib")) {
			if (ReadWord(ptr, word, WordLim)) {
				char name[100];
				const char *p = strrchr(filename, '/'); //-filename, count = 0;
				if (p) {
					int nchars = p-filename;
					strncpy(name, filename, nchars+1);
					name[nchars+1] = 0;
					strcat(name, word);
				}
				else
					strcpy(name, word);
				mtlMap = ReadMaterial(name);
				if (false) {
					int count = 0;
					for (MtlMap::iterator iter = mtlMap.begin(); iter != mtlMap.end(); iter++) {
						string s = (string) iter->first;
						Mtl m = (Mtl) iter->second;
						printf("m[%i].name=%s,.kd=(%3.2f,%3.2f,%3.2f),s=%s\n", count++, m.name.c_str(), m.kd.x, m.kd.y, m.kd.z, s.c_str());
					}
				}					
			}
		}
		else if (!strcmp(word, "usemtl")) {
			if (ReadWord(ptr, word, WordLim)) {
				MtlMap::iterator it = mtlMap.find(string(word));
				if (it == mtlMap.end())
					printf("no such material: %s\n", word);
				else {
					Mtl m = it->second;
					m.startTriangle = triangles.size();
					if (triangleMtls)
						triangleMtls->push_back(m);
				}
			}
		}
		else if (!strcmp(word, "g")) {
			if (ReadWord(ptr, word, WordLim)) {				// read group name
				if (triangleGroups)
					triangleGroups->push_back(Group(triangles.size(), string(word)));
			}
		}
		else if (!strcmp(word, "v")) {                      // read vertex coordinates
			if (sscanf(ptr, "%g%g%g", &v.x, &v.y, &v.z) != 3) {
				printf("bad line %d in object file", lineNum);
				return false;
			}
			tmpVertices.push_back(vec3(v.x, v.y, v.z));
		}
		else if (!strcmp(word, "vn")) {                     // read vertex normal
			if (sscanf(ptr, "%g%g%g", &v.x, &v.y, &v.z) != 3) {
				printf("bad line %d in object file", lineNum);
				return false;
			}
			tmpNormals.push_back(vec3(v.x, v.y, v.z));
		}
		else if (!strcmp(word, "vt")) {                     // read vertex texture
			if (sscanf(ptr, "%g%g", &t.x, &t.y) != 2) {
				printf("bad line in object file");
				return false;
			}
			tmpTextures.push_back(vec2(t.x, t.y));
		}
		else if (!strcmp(word, "f")) {                      // read triangle or polygon
			static vector<int> vids;
			vids.resize(0);
			while (ReadWord(ptr, word, WordLim)) {          // read arbitrary # face vid/tid/nid
				// set texture and normal pointers to preceding /
				char *tPtr = strchr(word+1, '/');           // pointer to /, or null if not found
				char *nPtr = tPtr? strchr(tPtr+1, '/') : NULL;
				// use of / is optional (ie, '3' is same as '3/3/3')
				// convert to vid, tid, nid indices (vertex, texture, normal)
				int vid = atoi(word);
				if (!vid)                                   // atoi returns 0 if failure to convert
					break;
				int tid = tPtr && *++tPtr != '/'? atoi(tPtr) : vid;
				int nid = nPtr && *++nPtr != 0? atoi(nPtr) : vid;
				// standard .obj is indexed from 1, mesh indexes from 0
				vid--;
				tid--;
				nid--;
				if (vid < 0 || tid < 0 || nid < 0) {        // atoi = 0 is conversion failure
					printf("bad format on line %d\n", lineNum);
					break;
				}
				int3 key(vid, tid, nid);
				VidMap::iterator it = vidMap.find(key);
				if (it == vidMap.end()) {
					int nvrts = points.size();
					vidMap[key] = nvrts;
					points.push_back(tmpVertices[vid]);
					if (normals && (int) tmpNormals.size() > nid)
						normals->push_back(tmpNormals[nid]);
					if (textures && (int) tmpTextures.size() > tid)
						textures->push_back(tmpTextures[tid]);
					vids.push_back(nvrts);
				}
				else
					vids.push_back(it->second);
			}
			int nids = vids.size();
			if (nids == 3) {
				int id1 = vids[0], id2 = vids[1], id3 = vids[2];
				if (normals && (int) normals->size() > id1) {
					vec3 &p1 = points[id1], &p2 = points[id2], &p3 = points[id3];
					vec3 a(p2-p1), b(p3-p2), n(cross(a, b));
					if (dot(n, (*normals)[id1]) < 0) {
						int tmp = id1;
						id1 = id3;
						id3 = tmp;
					}
				}
				// create triangle
				triangles.push_back(int3(id1, id2, id3));
			}
			else if (nids == 4 && quads)
				quads->push_back(int4(vids[0], vids[1], vids[2], vids[3]));
			else if (nids == 2 && segs)
				segs->push_back(int2(vids[0], vids[1]));
			else
				// create polygon as nvids-2 triangles
				for (int i = 1; i < nids-1; i++) {
					triangles.push_back(int3(vids[0], vids[i], vids[(i+1)%nids]));
				}
		} // end "f"
		else if (*word == 0 || *word == '\n')               // skip blank line
			continue;
		else {                                              // unrecognized attribute
			// printf("unsupported attribute in object file: %s", word);
			continue; // return false;
		}
	} // end read til end of file
	if (triangleGroups) {
		int nGroups = triangleGroups->size();
		for (int i = 0; i < nGroups; i++) {
			int next = i < nGroups-1? (*triangleGroups)[i+1].startTriangle : triangles.size();
			(*triangleGroups)[i].nTriangles = next-(*triangleGroups)[i].startTriangle;
		}
	}
	if (triangleMtls) {
		int nMtls = triangleMtls->size();
		for (int i = 0; i < nMtls; i++) {
			int next = i < nMtls-1? (*triangleMtls)[i+1].startTriangle : triangles.size();
			(*triangleMtls)[i].nTriangles = next-(*triangleMtls)[i].startTriangle;
		}
	}
	return true;
} // end ReadAsciiObj

bool WriteAsciiObj(const char *filename,
				   vector<vec3> &points,
				   vector<vec3> &normals,
				   vector<vec2> &uvs,
				   vector<int3> *triangles,
				   vector<int4> *quads,
				   vector<int2> *segs) {
	FILE *file = fopen(filename, "w");
	if (!file) {
		printf("can't write %s\n", filename);
		return false;
	}
	for (size_t i = 0; i < points.size(); i++)
		fprintf(file, "v %f %f %f \n", points[i].x, points[i].y, points[i].z);
	fprintf(file, "\n");
	for (size_t i = 0; i < normals.size(); i++)
		fprintf(file, "vn %f %f %f \n", normals[i].x, normals[i].y, normals[i].z);
	fprintf(file, "\n");
	for (size_t i = 0; i < uvs.size(); i++)
		fprintf(file, "vt %f %f \n", uvs[i].x, uvs[i].y);
	fprintf(file, "\n");
	// write triangles, quads (adding 1 to all vertex indices per OBJ format)
	if (triangles) {
		for (size_t i = 0; i < triangles->size(); i++)
			fprintf(file, "f %d %d %d \n", 1+(*triangles)[i].i1, 1+(*triangles)[i].i2, 1+(*triangles)[i].i3);
		fprintf(file, "\n");
	}
	if (quads)
		for (size_t i = 0; i < quads->size(); i++)
			fprintf(file, "f %d %d %d %d \n", 1+(*quads)[i].i1, 1+(*quads)[i].i2, 1+(*quads)[i].i3, 1+(*quads)[i].i4);
	if (segs)
		for (size_t i = 0; i < segs->size(); i++)
			fprintf(file, "f %d %d \n", 1+(*segs)[i].i1, 1+(*segs)[i].i2);
	fclose(file);
	return true;
}
