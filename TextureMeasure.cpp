// TextureMeasure: associate vertices and textures (c) 2020 by Jules Bloomenthal
// UV-parameterization is simple if the geometry and parameter spaces coincide (eg, texture image and
// geometry from same photograph); if the spaces are not the same, this tool matches geometry with texture

#include <glad.h>
#include <glfw3.h>
#include <algorithm>
#include <cstdio>
#include <float.h>
#include <string.h>
#include <vector>
#include <Camera.h>
#include <Draw.h>
#include <GLXtras.h>
#include <Letters.h>
#include <Misc.h>
#include <Numbers.h>
#include <Sprite.h>
#include <VecMat.h>
#include <Widgets.h>

using namespace std;

// Persistence
const char *textureFilename = "textures/grass.tga";
const char *uvsFilename = "C:/users/Jules/Code/Aids/Kirk.uvs";

// Vertices, Normals, UVs (half face)
vec3 pointsHalfFace[] = {
	vec3(761, -268, 1225),  vec3(392, -429, 1167),  vec3(486, -629, 1281),  vec3(292, -726, 1084),  vec3(761, -669, 1344),
	vec3(761, -735, 1359),  vec3(630, -710, 1331),  vec3(397, -727, 1233),  vec3(324, -812, 1142),  vec3(303, -846, 1060),
	vec3(305, -934, 1070),  vec3(347, -918, 1124),  vec3(503, -823, 1254),  vec3(761, -885, 1312),  vec3(761, -951, 1335),
	vec3(601, -1058, 1250), vec3(394, -1044, 1210), vec3(761, -1197, 1433), vec3(242, -1002, 918),  vec3(315, -1179, 1100),
	vec3(761, -1364, 1316), vec3(400, -1190, 1185), vec3(534, -1296, 1255), vec3(761, -1434, 1318), vec3(602, -1421, 1256),
	vec3(360, -1356, 1137), vec3(329, -1374, 1023), vec3(335, -1447, 826),  vec3(360, -1479, 1042), vec3(453, -1513, 1157),
	vec3(761, -1552, 1287), vec3(761, -1587, 1267), vec3(761, -1804, 1279), vec3(529, -1458, 1206), vec3(622, -1563, 1235),
	vec3(516, -1693, 1131), vec3(410, -1594, 994),  vec3(494, -1710, 710),  vec3(492, -1816, 643),  vec3(761, -1979, 725),
	vec3(761, -1819, 878),  vec3(545, -271, 1218),  vec3(402, -925, 1190),  vec3(506, -865, 1223),  vec3(554, -852, 1223),
	vec3(583, -925, 1223),  vec3(641, -900, 1223),  vec3(450, -972, 1223),  vec3(535, -981, 1223),  vec3(599, -969, 1223),
	vec3(629, -937, 1223),  vec3(670, -1001, 1263), vec3(651, -1477, 1246), vec3(761, -1488, 1276), vec3(692, -1069, 1284),
	vec3(761, -1070, 1376), vec3(580, -1133, 1280), vec3(606, -1206, 1273), vec3(761, -1271, 1446), vec3(761, -1329, 1316)
};
const int nPointsHalfFace = sizeof(pointsHalfFace)/sizeof(vec3), nPoints = 2*nPointsHalfFace;
vec3 points[nPoints];
vec3 normals[nPoints];
vec2 uvs[nPoints];
int contours[nPoints];		// if > -1, group # = edgeContours index
const int sizePts = sizeof(points), sizeUVs = sizeof(uvs);

// Triangles (half face)
int trianglesHalfFace[][3] = {
	{0, 1, 2},    {1, 3, 2},    {0, 2, 4},    {3, 7, 2},    {7, 12, 6},   {2, 7, 6},    {2, 6, 4},    {4, 6, 5},
	{5, 6, 13},   {6, 12, 13},  {3, 8, 7},    {7, 8, 12},   {3, 18, 9},   {9, 18, 10},  {3, 9, 8},    {9, 10, 8},
	{10, 11, 8},  {8, 11, 12},  {18, 16, 10}, {10, 16, 11}, {18, 19, 16}, {19, 21, 16}, {21, 22, 16}, {19, 26, 25},
	{19, 25, 21}, {25, 22, 21}, {26, 27, 28}, {26, 28, 25}, {28, 29, 25}, {25, 29, 22}, {22, 29, 33}, {33, 24, 22},
	{24, 23, 22}, {22, 23, 20}, {28, 36, 29}, {36, 35, 29}, {29, 35, 34}, {29, 34, 33}, {33, 34, 30}, {34, 31, 30},
	{36, 37, 35}, {37, 38, 39}, {35, 32, 34}, {34, 32, 31}, {37, 40, 35}, {37, 39, 40}, {35, 40, 32}, {41, 1, 0},
	{11, 42, 12}, {11, 16, 42}, {42, 16, 47}, {42, 47, 43}, {42, 43, 12}, {43, 44, 12}, {47, 48, 43}, {43, 48, 45},
	{48, 49, 45}, {49, 50, 45}, {45, 50, 46}, {44, 45, 46}, {46, 50, 14}, {49, 51, 50}, {50, 51, 14}, {47, 16, 48},
	{16, 15, 48}, {48, 15, 49}, {49, 15, 51}, {33, 52, 24}, {33, 30, 52}, {52, 30, 53}, {52, 53, 23}, {24, 52, 23},
	{16, 56, 15}, {15, 56, 54}, {15, 54, 51}, {51, 54, 14}, {54, 55, 14}, {54, 17, 55}, {16, 22, 56}, {22, 57, 56},
	{56, 57, 54}, {22, 59, 57}, {57, 59, 58}, {57, 17, 54}, {57, 58, 17}, {22, 20, 59}, {12, 44, 13}, {44, 46, 13},
	{46, 14, 13}, {44, 43, 45}, {18, 26, 19}, {18, 27, 26}, {27, 36, 28}, {27, 37, 36}
};
const int nTrianglesHalfFace = sizeof(trianglesHalfFace)/(3*sizeof(int));
const int nTriangles = 2*nTrianglesHalfFace;
int triangles[nTriangles][3];

// User-Defined Contours
typedef vector<int2> Contour;
vector<Contour> edgeContours;

// OpenGL IDs
GLuint vBuffer = 0, program = 0;

// App Window, Camera
int winWidth = 1600, winHeight = 800;
Camera camera(winHeight, winHeight, vec3(0, 0, 0), vec3(0, 0, -5), 30, 0.001f, 500, false);

// Texture Image Display
int textureX = 1000, textureW = winWidth-textureX, textureH = 0;
Sprite texture;

// User Selection
void *picked = NULL; // togglers, camera, points, uvs, edgeContours
int pointPick = -1, texturePick = -1, contourPick = -1;
bool showTexture = false, showNumbers = false, showLines = true;

// Buttons
Toggler togLines(&showLines, "Lines", 620, 20, 16);
Toggler togNumbers(&showNumbers, "Numbers", 620, 50, 16);
Toggler togTexture(&showTexture, "Texture", 620, 80, 16);
Toggler togRead(NULL, "Read UVs", 770, 20, 16);
Toggler togWrite(NULL, "Write UVs", 770, 50, 16);
Toggler togTransform(NULL, "", 770, 80, 16);
Toggler *togs[] =  { &togRead, &togWrite, &togTransform, &togNumbers, &togTexture, &togLines };
int nTogs = sizeof(togs)/sizeof(Toggler *);

// Shaders
const char *vShader = R"(
	#version 130
	in vec3 point;
	in vec3 normal;
	in vec2 uv;
	uniform mat4 modelview;
	uniform mat4 persp;
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vUV;
	void main() {
		vPoint = (modelview*vec4(point, 1)).xyz;	// point multiply
		vNormal = (modelview*vec4(normal, 0)).xyz;	// vector multiply
		vUV = uv;
		gl_Position = persp*vec4(vPoint, 1);
	}
)";

const char *pShader = R"(
	#version 130
	in vec3 vPoint;									// 3D point
	in vec3 vNormal;								// 3D normal
	in vec2 vUV;									// 2D texture coordinates
	uniform float a = 0.1;							// ambient light term
	uniform vec3 lightPos = vec3(-1, 0, -2);
	uniform vec3 color = vec3(1, 1, 1);				// surface color
	uniform sampler2D textureImage;
	uniform bool useTexture = false;
	out vec4 pColor;
	void main() {
		vec3 N = normalize(vNormal);				// must be unit length
		vec3 L = normalize(lightPos-vPoint);		// light vector
		vec3 R = reflect(L, N);						// hightlight vector
		vec3 E = normalize(vPoint);					// eye vector
		float d = abs(dot(L, N));					// diffuse term
		float h = max(0, dot(R, E));
		float s = pow(h, 100);						// specular term
		float intensity = clamp(a+d+s, 0, 1);
		vec3 col = useTexture? texture(textureImage, vUV).rgb : color;
		pColor = vec4(intensity*col, 1);
	}
)";

// Transformation (entire face or selected contours)

enum TransformMode { TM_None = 0, TM_Move, TM_ScaleX, TM_ScaleY, TM_RotZ, TM_NModes };
TransformMode transformMode = TM_None;
vec2 mouseDown, uvsPreTransform[nPoints];

void Transform(vec2 mouse) {
	vec2 dif(mouse.x-mouseDown.x, mouseDown.y-mouse.y);
	mat4 m = transformMode == TM_Move? Translate(vec3(.005f*dif, 0)) :
			 transformMode == TM_ScaleX? Scale(1+.005f*dif.x, 1, 0) :
			 transformMode == TM_ScaleY? Scale(1, 1+.005f*dif.y, 0) :
			 transformMode == TM_RotZ? Translate(vec3(.5f,.5f,0))*RotateZ(.1f*(dif.x+dif.y))*Translate(vec3(-.5f,-.5f,0)) :
			 mat4();
	for (int i = 0; i < nPoints; i++) {
		bool doAllPoints = contourPick == -1;
		bool moveThisContour = contours[i] == contourPick;
		if (doAllPoints || moveThisContour) {
			vec4 x = m*vec4(uvsPreTransform[i], 0, 1);
			uvs[i] = vec2(x.x, x.y);
		}
	}
}

void TransformInit() {
	for (int i = 0; i < nPoints; i++)
		uvsPreTransform[i] = uvs[i];
}

// Display

vec2 TfromUV(int i) { return vec2(textureX+textureW*uvs[i].x, textureH*uvs[i].y); }
vec2 UVfromT(vec2 uv) { return vec2((uv.x-textureX)/textureW, uv.y/textureH); }
vec3 cols[] = { vec3(1,0,0), vec3(0,1,0), vec3(1,1,0), vec3(1,0,1), vec3(0,1,1) };
int ncols = sizeof(cols)/sizeof(vec3);

void DrawUVs(vec3 col) {
	UseDrawShader(ScreenMode());
	// draw mesh in blue
	for (int i = 0; i < nTriangles; i++) {
		int *t = triangles[i];
		for (int k = 0; k < 3; k++)
			Line(TfromUV(t[k]), TfromUV(t[(k+1)%3]), 2, vec3(0, 0, 1));
	}
	// draw contours in color
	for (int i = 0; i < (int) edgeContours.size(); i++) {
		Contour &c = edgeContours[i];
		vec3 col = cols[i % ncols];
		float width = i == contourPick? 5.f : 2.f;
		for (int k = 0; k < (int) c.size(); k++)
			Line(TfromUV(c[k].i1), TfromUV(c[k].i2), width, col);
	}
	if (texturePick >= 0)
		Disk(TfromUV(texturePick), 7, vec3(1, 0, 0));
	for (int i = 0; i < nPoints; i++)
		Disk(TfromUV(i), 10, contours[i] >= 0? cols[contours[i] & ncols] : col);
	if (pointPick >= 0)
		Disk(TfromUV(pointPick), 7, vec3(1, 1, 0));
}

void Display(GLFWwindow* w) {
	// clear to gray, use app's shader
	glClearColor(0.5, 0.5, 0.5, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);
	// use program, bind vertex buffer, set vertex feed, set uniforms
	glUseProgram(program);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	VertexAttribPointer(program, "point", 3, 0, (void *) 0);
	VertexAttribPointer(program, "normal", 3, 0, (void *) sizePts);
	VertexAttribPointer(program, "uv", 2, 0, (void *) (2*sizePts));
	SetUniform(program, "modelview", camera.modelview);
	SetUniform(program, "persp", camera.persp);
	// set color, draw shape
	SetUniform(program, "color", vec3(1, 1, 1));
	SetUniform(program, "textureImage",  (int) texture.textureUnit); // texture.textureName); // 0);
	SetUniform(program, "useTexture", showTexture);
	glViewport(0, 0, textureX, winHeight);
	glEnable(GL_DEPTH_TEST);
	glDrawElements(GL_TRIANGLES, 3*nTriangles, GL_UNSIGNED_INT, triangles);
	// vertex and triangle annotation
	glDisable(GL_DEPTH_TEST);
	UseDrawShader(camera.fullview);
	if (showLines)
		for (int i = 0; i < nTriangles; i++) {
			int *t = triangles[i];
			for (int k = 0; k < 3; k++)
				Line(points[t[k]], points[t[(k+1)%3]], 1, vec3(0, 0, 1));
		}
	if (pointPick >= 0)
		Disk(points[pointPick], 9, vec3(1, 1, 0)); 
	if (texturePick >= 0)
		Disk(points[texturePick], 9, vec3(1, 0, 0));
	if (showNumbers)
		for (int i = 0; i < nPoints; i++)
			Number(points[i], camera.fullview, i, vec3(.3f, 0, 0), 12);
	// right side: texture image
	glViewport(textureX, 0, textureW, textureH);
	texture.Display();
	DrawUVs(vec3(0, 0, 1));
	// controls
	glViewport(0, 0, winWidth, winHeight);
	UseDrawShader(ScreenMode());
	const char *xformNames[] = { "No Transform", "Move", "ScaleX", "ScaleY", "RotateZ" };
	togTransform.SetName(xformNames[transformMode]);
	for (int i = 0; i < nTogs; i++)
		togs[i]->Draw(NULL, 12);
	glFlush();
}

// Edge Operations

int2 OrderedEdge(int i1, int i2) { return int2(i1 < i2? i1 : i2, i1 > i2? i1 : i2); }
int2 OrderedEdge(int2 e) { return int2(e.i1 < e.i2? e.i1 : e.i2, e.i1 > e.i2? e.i1 : e.i2); }

int GetIndex(Contour &c, int2 e) {
	for (int k = 0; k < (int) c.size(); k++)
		if (c[k] == e)
			return k;
	return -1;
}

int GetPoint(vec2 mouse) {
	for (int i = 0; i < nPoints; i++)
		if (length(mouse-TfromUV(i)) < 10)
			return i;
	return -1;
}

bool IsZero(float f) { return f < FLT_EPSILON && f > -FLT_EPSILON; };

vec2 NearestToSegment(vec2 p, vec2 p1, vec2 p2, float &alpha) {
	vec2 delta(p2-p1);
	if (IsZero(delta.x) && IsZero(delta.y))
		return p1;
	alpha = dot(delta, p-p1)/dot(delta, delta);
	return alpha < 0.? p1 : alpha > 1.? p2 : p1+alpha*delta;
}

int2 GetEdge(vec2 mouse) {
	for (int i = 0; i < nTriangles; i++) {
		int *t = triangles[i];
		for (int k = 0; k < 3; k++) {
			int i1 = t[k], i2 = t[(k+1)%3];
			float alpha = 0;
			vec2 nearest = NearestToSegment(mouse, TfromUV(i1), TfromUV(i2), alpha);
			if (alpha > .25f && alpha < .75f && length(mouse-nearest) < 5)
				return OrderedEdge(i1, i2);
		}
	}
	return int2(-1, -1);
}

int GetContour(int2 e) {
	for (int i = 0; i < (int) edgeContours.size(); i++)
		if (GetIndex(edgeContours[i], e) >= 0)
			return i;
	return -1;
}

// Contours

void RemoveIfThereAddIfNot(Contour &edges, int2 e) {
	for (int i = 0; i < (int) edges.size(); i++) {
		int2 ee = edges[i];
		if (e == ee || (e.i1 == ee.i2 && e.i2 == ee.i1)) {
			edges.erase(edges.begin()+i);
			return;
		}
	}
	edges.push_back(e);
}

bool ConnectToContour(Contour &contour, int2 e) {
	for (int i = 0; i < (int) contour.size(); i++) {
		int2 c = contour[i];
		if (c.i1 == e.i1 || c.i1 == e.i2 || c.i2 == e.i1 || c.i2 == e.i2)
			return true;
	}
	return false;
}

void UpdateContours() {
	for (int i = 0; i < nPoints; i++)
		contours[i] = -1;
	for (int i = 0; i < (int) edgeContours.size(); i++) {
		Contour &c = edgeContours[i];
		for (int k = 0; k < (int) c.size(); k++)
			contours[c[k].i1] = contours[c[k].i2] = i;
	}
}

// I/O

string GetFilename(bool read) {
	string dir = GetDirectory();
	printf("(current directory is %s)\n%s filename: ", dir.c_str(), read? "input" : "output");
	char name[500];
	fgets(name, 500, stdin);
	if (strlen(name))
		name[strlen(name)-1] = 0;
	return dir+string(name);
}

void WriteUVs() {
	string filename = GetFilename(false);
	FILE *out = fopen(filename.c_str(), "w");
	if (out) {
		fprintf(out, "u, v, contour\n");
		for (int i = 0; i < nPoints; i++)
			fprintf(out, "%5.4f %5.4f %i\n", uvs[i][0], uvs[i][1], contours[i]);
		fclose(out);
	}
	printf("%s%s written\n", filename.c_str(), out? "" : " not");
}

bool ReadUVs(const char *name = NULL) {
	string filename = name? string(name) : GetFilename(true);
	FILE *in = fopen(filename.c_str(), "r");
	if (in) {
		char line[500];
		fgets(line, 500, in); // skip header
		for (int i = 0; i < nPoints; i++) {
			fgets(line, 500, in);
			if (feof(in))
				break;
			if (sscanf(line, "%f%f%i", &uvs[i][0], &uvs[i][1], &contours[i]) != 3) {
				printf("bad line %d in file", i);
				break;
			}
		}
		printf("%s%s read\n", filename.c_str(), in? "" : " not");
		int highest = -1;
		for (int i = 0; i < nPoints; i++)
			highest = contours[i] > highest? contours[i] : highest;
		edgeContours.resize(highest+1);
		for (int i = 0; i < nTriangles; i++) {
			int *t = triangles[i];
			for (int k = 0; k < 3; k++) {
				int i1 = t[k], i2 = t[(k+1)%3], c = contours[i1];
				if (c >= 0 && c == contours[i2]) {
					int2 e = OrderedEdge(i1, i2);
					if (GetIndex(edgeContours[c], e) < 0)
						edgeContours[c].push_back(e);
				}
			}
		}
	}
	return in != NULL;
}

// Mouse

int WindowHeight(GLFWwindow *w) {
	int width, height;
	glfwGetWindowSize(w, &width, &height);
	return height;
}

bool Shift(GLFWwindow *w) {
	return glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
		   glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}

void MouseButton(GLFWwindow* w, int butn, int action, int mods) {
	picked = NULL;
	if (action == GLFW_PRESS) {
		double x, y, yInv;
		glfwGetCursorPos(w, &x, &y);
		yInv = WindowHeight(w)-y;
		mouseDown = vec2(x, y);
		if (transformMode != TM_None)
			TransformInit();
		// test for button
		for (int i = 0; i < nTogs; i++)
			if (togs[i]->DownHit(x, yInv, action)) {
				picked = togs;
				if (i == 0) ReadUVs();
				if (i == 1) WriteUVs();
				if (i == 2) transformMode = (TransformMode) ((int) (transformMode+1) % TM_NModes);
			}
		// test for texture selection
		if (!picked && x >= textureX) {
			vec2 mouse(x, yInv);
			if (butn == GLFW_MOUSE_BUTTON_LEFT) {
				//if (pickMode == PM_PickPoint) {
				int mousePoint = GetPoint(mouse);
				if (mousePoint >= 0) {
					picked = uvs;
					texturePick = mousePoint;
				}
			}
			if (butn == GLFW_MOUSE_BUTTON_RIGHT) {
				int2 mouseEdge = GetEdge(mouse);
				if (mouseEdge.i1 < 0 || mouseEdge.i2 < 0)
					contourPick = -1;
				if (mouseEdge.i1 >= 0 && mouseEdge.i2 >= 0) {
					picked = uvs;
					int2 e = OrderedEdge(mouseEdge);
					int newContourPick = GetContour(e);
					if (contourPick >= 0) { // contour previously selected
						if (newContourPick == contourPick) {
							// remove selected edge from selected contour
							Contour &c = edgeContours[contourPick];
							int index = GetIndex(c, e);
							if (index >= 0)
								c.erase(c.begin()+index);
							contours[e.i1] = contours[e.i2] = -1;
						}
						if (newContourPick != contourPick && newContourPick >= 0)
							// select a different contour
							contourPick = newContourPick;
						if (newContourPick < 0) {
							// add selected non-contour edge to selected contour
							edgeContours[contourPick].push_back(e);
							contours[e.i1] = contours[e.i2] = contourPick;
						}
					}
					if (contourPick < 0) {
						// no currently selected contour: choose contour or create new
						if (newContourPick >= 0)
							contourPick = newContourPick;
						if (newContourPick < 0) {
							int nContours = edgeContours.size();
							edgeContours.resize(nContours+1);
							edgeContours[nContours].push_back(e);
							contours[e.i1] = contours[e.i2] = nContours;
							contourPick = nContours;
						}
					}
				}
			} // GLFW_MOUSE_BUTTON_RIGHT
		}
		// test for point selection
		if (!picked && x < textureX && butn == GLFW_MOUSE_BUTTON_RIGHT) {
			glViewport(0, 0, textureX, winHeight); // needed by MouseOver
			for (int i = 0; i < nPoints; i++)
				if (MouseOver(x, WindowHeight(w)-y, points[i], camera.fullview, 20)) {
					picked = points;
					pointPick = i;
				}
		}
		// test for camera
		if (!picked) {
			picked = &camera;
			camera.MouseDown((int) x, (int) y);
		}
	}
	if (action == GLFW_RELEASE)
		camera.MouseUp();
}

void MouseWheel(GLFWwindow *w, double ignore, double spin) {
	camera.MouseWheel(spin > 0, Shift(w));
}

void MouseMove(GLFWwindow* w, double x, double y) {
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		if  (x > textureX) {
			bool modified = false;
			if (transformMode != TM_None) {
				Transform(vec2(x, y));
				modified = true;
			}
			else if (picked == uvs &&texturePick >= 0) {
				uvs[texturePick] = UVfromT(vec2(x, WindowHeight(w)-y));
				modified = true;
			}
			if (modified) {
				glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
				glBufferSubData(GL_ARRAY_BUFFER, 2*sizePts, sizeUVs, &uvs[0]);
			}
		}
		if (x < textureX && picked == &camera)
			camera.MouseDrag((int) x, (int) y, Shift(w));
	}
}

// Initialization

void UnitSize() {
	// scale/center points to lie within +/-1
	vec3 mn(FLT_MAX), mx(-FLT_MAX);
	for (int i = 0; i < nPoints; i++) {
		vec3 p = points[i];
		for (int k = 0; k < 3; k++) {
			if (p[k] < mn[k]) mn[k] = p[k];
			if (p[k] > mx[k]) mx[k] = p[k];
		}
	}
	vec3 center = .5f*(mn+mx), range = mx-mn;
	float maxrange = max(max(range.x, range.y), range.z), s = 2/maxrange;
	for (int i = 0; i < nPoints; i++)
		points[i] = s*(points[i]-center);
}

void InitUVs() {
	if (!uvsFilename || !ReadUVs(uvsFilename)) {
		// initialize uvs to correspond with points.xy, clear keys and contours
		for (int i = 0; i < nPoints; i++) {
			// shift +/-1 to 0-1
			vec3 pt = points[i];
			uvs[i] = vec2(.5f*pt.x+.5f, .5f*pt.y+.5f);
			contours[i] = -1;
		}
		// initialize boundary edges (shared by only one triangle)
		Contour boundaries;
		for (int i = 0; i < nTriangles; i++) {
			int *t = triangles[i];
			RemoveIfThereAddIfNot(boundaries, OrderedEdge(t[0], t[1]));
			RemoveIfThereAddIfNot(boundaries, OrderedEdge(t[1], t[2]));
			RemoveIfThereAddIfNot(boundaries, OrderedEdge(t[2], t[0]));
		}
		// drain connected boundary edges
		while (boundaries.size() > 0) {
			bool foundConnection = false;
			for (int i = 0; i < (int) boundaries.size(); i++) {
				int2 e = boundaries[i];
				for (int k = 0; k < (int) edgeContours.size(); k++)
					if (ConnectToContour(edgeContours[k], e)) {
						edgeContours[k].push_back(e);
						foundConnection = true;
						break;
					}
				if (foundConnection) {
					boundaries.erase(boundaries.begin()+i);
					break;
				}
			}
			if (!foundConnection) {
				int nBoundaries = boundaries.size(), nEdgeContours = edgeContours.size();
				edgeContours.resize(nEdgeContours+1);
				edgeContours[nEdgeContours].push_back(boundaries[nBoundaries-1]);
				boundaries.resize(nBoundaries-1);
			}
		}
		UpdateContours();
	}
	// set barycentric weights based on points.xy
}

void InitNormals() {
	for (int i = 0; i < nPoints; ++i)
		normals[i] = vec3(0, 0, 0);
	for (int i = 0; i < nTriangles; ++i) {
		int *t = triangles[i];
		vec3 p1(points[t[0]]), p2(points[t[1]]), p3(points[t[2]]);
		vec3 n = normalize(cross(p3-p2, p2-p1));
		for (int k = 0; k < 3; k++)
			normals[t[k]] += n;
	}
	for (int i = 0; i < nPoints; ++i)
		normals[i] = normalize(normals[i]);
}

void InitTexture() {
	int imageW, imageH;
	texture.Initialize(textureFilename);
	TargaSize(textureFilename, imageW, imageH);
	textureH = (int) (textureW*(float)imageH/imageW);
}

void InitVertexBuffer() {
	glGenBuffers(1, &vBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
	glBufferData(GL_ARRAY_BUFFER, 2*sizePts+sizeUVs, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizePts, &points[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizePts, sizePts, &normals[0]);
	glBufferSubData(GL_ARRAY_BUFFER, 2*sizePts, sizeUVs, &uvs[0]);
}

void SetFullFace() {
	float midX = pointsHalfFace[0].x;
   // vertices for full face
	for (int i = 0; i < nPointsHalfFace; i++)
		points[i] = pointsHalfFace[i];
	for (int i = 0; i < nPointsHalfFace; i++) {
		vec3 p = pointsHalfFace[i];
		points[nPointsHalfFace+i] = vec3(2*midX-p.x, p.y, p.z);
	}
	// triangles for full face
	for (int i = 0; i < nTrianglesHalfFace; i++)
		memcpy(triangles[i], trianglesHalfFace[i], 3*sizeof(int));
	for (int i = 0; i < nTrianglesHalfFace; i++)
		for (int k = 0; k < 3; k++) {
			int p = trianglesHalfFace[i][k];
			bool mid = abs(points[p].x-midX) < .01f;
			triangles[i+nTrianglesHalfFace][2-k] = mid? p : p+nPointsHalfFace;
				// reverse direction of triangle
				// reuse point on mid-face, or use reflected point
		}
}

// App

void Resize(GLFWwindow* w, int width, int height) {
	winWidth = width;
	winHeight = height;
	camera.Resize(width, height);
	glViewport(0, 0, width, height);
}

const char *usage = R"(Usage:
  Left Screen
	left-mouse: rotate XY, shift: translate XY
	mouse wheel: rotate Z, shift: translate Z
  Right Screen
	left-mouse if no transform selected: select/move point
	left-mouse if transform selected: control transform
	right-mouse: select/add/del edge
)";

int main() {
	if (!glfwInit())
		return 1;
	GLFWwindow *w = glfwCreateWindow(winWidth, winHeight, "Texture Adjust", NULL, NULL);
	if (!w) {
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(w);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PrintGLErrors();
	program = LinkProgramViaCode(&vShader, &pShader);
	SetFullFace();
	UnitSize();
	InitNormals();
	InitUVs();
	InitVertexBuffer();
	InitTexture();
	glfwSetScrollCallback(w, MouseWheel);
	glfwSetMouseButtonCallback(w, MouseButton);
	glfwSetCursorPosCallback(w, MouseMove);
	glfwSetWindowSizeCallback(w, Resize);
	glfwSwapInterval(1);
	printf(usage);
	while (!glfwWindowShouldClose(w)) {
		Display(w);
		glfwSwapBuffers(w);
		glfwPollEvents();
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vBuffer);
	glfwDestroyWindow(w);
	glfwTerminate();
}
