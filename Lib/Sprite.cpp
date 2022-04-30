// Sprite.cpp

#include "CameraArcball.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Misc.h"
#include "Sprite.h"
#include <assert.h>
#include <iostream>
#include <fstream>
#include <float.h>
#include <string.h>
#include <cstdlib>

using namespace std;

GLuint spriteShader = 0;
int spriteTextureUnit = 1;

vec2 PtTransform(vec2 p, mat4 &m) {
	vec4 x = m*vec4(p, 0, 1);
	return vec2(x.x, x.y);
}

bool Sprite::Intersect(Sprite &s) {
	vec2 pts[] = {{-1,-1}, {-1,1}, {1,1}, {1,-1}};
	float x1min = FLT_MAX, x1max = -FLT_MAX, y1min = FLT_MAX, y1max = -FLT_MAX;
	float x2min = FLT_MAX, x2max = -FLT_MAX, y2min = FLT_MAX, y2max = -FLT_MAX;
	for (int i = 0; i < 4; i++) {
		vec2 p1 = PtTransform(pts[i], ptTransform);
		vec2 p2 = PtTransform(pts[i], s.ptTransform);
		x1min = p1.x < x1min? p1.x : x1min; x1max = p1.x > x1max? p1.x : x1max;
		y1min = p1.y < y1min? p1.y : y1min; y1max = p1.y > y1max? p1.y : y1max;
		x2min = p2.x < x2min? p2.x : x2min; x2max = p2.x > x2max? p2.x : x2max;
		y2min = p2.y < y2min? p2.y : y2min; y2max = p2.y > y2max? p2.y : y2max;
	}
	return !(x1min > x2max || x1max < x2min || y1min > y2max || y1max < y2min);
}

#ifdef GL_QUADS
	bool useQuads = true;
#else
	bool useQuads = false;
#endif

void Sprite::Initialize(const char *imageFile, const char *matFile) {
	textureUnit = spriteTextureUnit++;
	textureName = LoadTexture(imageFile, textureUnit);
	if (matFile && strlen(matFile) > 0) {
		matUnit = spriteTextureUnit++;
		matName = LoadTexture(matFile, matUnit);
	}
}

void Sprite::Initialize(const char *imageFile, const char *matFile, int texUnit) {
	spriteTextureUnit = texUnit;
	Initialize(imageFile, matFile);
}

void Sprite::Initialize(string imageFile, string matFile) {
	Initialize(imageFile.c_str(), matFile.c_str());
}

void Sprite::Initialize(vector<string> &imageFiles, string matFile) {
	Initialize(imageFiles, matFile.c_str());
}

void Sprite::Initialize(vector<string> &imageFiles, const char *matFile) {
	textureUnit = spriteTextureUnit++;
	nFrames = imageFiles.size();
	textureNames.resize(nFrames);
	for (size_t i = 0; i < nFrames; i++)
		textureNames[i] = LoadTexture(imageFiles[i].c_str(), textureUnit);
	if (matFile) {
		matUnit = spriteTextureUnit++;
		matName = LoadTexture(matFile, matUnit);
	}
	change = clock()+(time_t)(frameDuration*CLOCKS_PER_SEC);
}

void Sprite::SetPosition(vec2 p) { position = p; UpdateTransform(); }

vec2 Sprite::GetPosition() { return position; }

void Sprite::UpdateTransform() {
	ptTransform = Translate(position.x, position.y, 0)*Scale(scale.x, scale.y, 1)*RotateZ(rotation);
}

void Sprite::MouseDown(vec2 mouse) {
	oldMouse = position;
	GetViewportSize(winWidth, winHeight);
	mouseDown = mouse;
}

vec2 Sprite::MouseDrag(vec2 mouse) {
	vec2 dif(mouse-mouseDown), difScale(dif.x/winWidth, dif.y/winHeight);
	SetPosition(oldMouse+difScale);
	return difScale;
}

void Sprite::MouseWheel(double spin) {
	scale += .1f*(float)spin;
	UpdateTransform();
}

vec2 Sprite::GetScale() { return scale; }

void Sprite::SetScale(vec2 s) {
	scale = s;
	UpdateTransform();
}

mat4 Sprite::GetPtTransform() { return ptTransform; }

void Sprite::SetPtTransform(mat4 m) { ptTransform = m; }

void Sprite::SetUvTransform(mat4 m) { uvTransform = m; }

void Sprite::Display(mat4 *fullview) {
	if (!spriteShader) {
		const char *vShaderQ = R"(
			#version 330
			uniform mat4 view;
			out vec2 uv;
			void main() {
				vec2 pts[] = vec2[4](vec2(-1,-1), vec2(-1,1), vec2(1,1), vec2(1,-1));
				uv = (vec2(1,1)+pts[gl_VertexID])/2;
				gl_Position = view*vec4(pts[gl_VertexID], 0, 1);
			}
		)";
		const char *vShaderT = R"(
			#version 330
			uniform mat4 view;
			out vec2 uv;
			void main() {
				vec2 pts[] = vec2[6](vec2(-1,-1),vec2(-1,1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(1,-1));
				uv = (vec2(1,1)+pts[gl_VertexID])/2;
				gl_Position = view*vec4(pts[gl_VertexID], 0, 1);
			}
		)";
		const char *pShader = R"(
			#version 330
			in vec2 uv;
			out vec4 pColor;
			uniform mat4 uvTransform;
			uniform sampler2D textureImage;
			uniform sampler2D textureMat;
			uniform bool useMat;
			void main() {
				vec2 st = (uvTransform*vec4(uv, 0, 1)).xy;
				pColor.rgb = texture(textureImage, st).rgb;
				pColor.a = useMat? texture(textureMat, uv).r : 1;
				if (pColor.a < .02) discard;
			}
		)";
		spriteShader = LinkProgramViaCode(useQuads? &vShaderQ : &vShaderT, &pShader);
	}
	glUseProgram(spriteShader);
	glActiveTexture(GL_TEXTURE0+textureUnit);
	if (nFrames) { // animation
		time_t now = clock();
		if (now > change) {
			frame = (frame+1)%nFrames;
			change = now+(time_t)(frameDuration*CLOCKS_PER_SEC);
		}
		glBindTexture(GL_TEXTURE_2D, textureNames[frame]);
	}
	else glBindTexture(GL_TEXTURE_2D, textureName);
	SetUniform(spriteShader, "textureImage", (int) textureUnit);
	SetUniform(spriteShader, "useMat", matName > 0);
	if (matName > 0) {
		glActiveTexture(GL_TEXTURE0+matUnit);
		glBindTexture(GL_TEXTURE_2D, matName);
		SetUniform(spriteShader, "textureMat", (int) matUnit);
	}
	SetUniform(spriteShader, "view", fullview? *fullview*ptTransform : ptTransform);
	SetUniform(spriteShader, "uvTransform", uvTransform);
	if (useQuads) glDrawArrays(GL_QUADS, 0, 4); else glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Sprite::SetFrameDuration(float dt) { frameDuration = dt; }

void Sprite::Release() {
	glDeleteBuffers(1, &textureName);
	if (matUnit > 0)
		glDeleteBuffers(1, &matName);
}

