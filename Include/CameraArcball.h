// CameraArcball.h (c) 2019-2022 Jules Bloomenthald

#ifndef CAMERA_AB_HDR
#define CAMERA_AB_HDR

#include <time.h>
#include "VecMat.h"
#include "Widgets.h"

// simple camera parameters and methods for mouse
// no-shift
//   drag: rotate about X and Y axes
//   wheel: rotate about Z axis
// shift
//   drag: translate along X and Y axes
//   wheel: translate along Z axis
// control
//   drag: constrain rotation/translation to major axes

class CameraAB {
private:
	time_t	lastArcballEvent = 0;
	float   aspectRatio = 1;
	float   fov = 30;
	float   nearDist = .001f, farDist = 500;
	bool    invertVertical = true;      // OpenGL defines origin lower left; Windows defines it upper left
	vec2    mouseDown;                  // for each mouse down, need start point
	vec3    rotateCenter;               // world rotation origin
	vec3    rotateOffset;               // for temp change in world rotation origin
	mat4    rot;                        // rotations controlled by arcball
	float   tranSpeed = .001f;
	vec3    tran, tranOld;              // translation controlled directly by mouse
public:
	bool	shift = false;
	Arcball arcball;
	mat4    modelview, persp, fullview; // read-only
	mat4    GetRotate();
	mat4	GetRotMat() { return rot; }
	vec3	Position();
	void    SetRotateCenter(vec3 r);
	void    MouseUp();
	void    MouseDown(double x, double y, bool shift = false, bool control = false);
	void    MouseDown(int x, int y, bool shift = false, bool control = false);
	void    MouseDrag(double x, double y);
	void    MouseDrag(int x, int y);
	void    MouseWheel(double spin, bool shift = false);
	void	MoveTo(vec3 t);
	void	Move(vec3 m);
	void    Resize(int w, int h);
	float   GetFOV();
	void    SetFOV(float fov);
	void    SetSpeed(float tranSpeed);
	void    SetModelview(mat4 m);
	vec3    GetRot(); // return x, y, z rotations (in radians)
	vec3    GetTran();
	float	TimeSinceArcballEvent();
	char   *Usage();
	// formerly private:
	void    Set(int *vp);
	void    Set(int scrnX, int scrnY, int scrnW, int scrnH);
	void    Set(int *viewport, mat4 rot, vec3 tran,
			 float fov = 30, float nearDist = .001f, float farDist = 500, bool invVrt = true);
	void    Set(int scrnX, int scrnY, int scrnW, int scrnH, mat4 rot, vec3 tran,
			 float fov = 30, float nearDist = .001f, float farDist = 500, bool invVrt = true);
	void    Set(int scrnX, int scrnY, int scrnW, int scrnH, Quaternion qrot, vec3 tran,
			 float fov = 30, float nearDist = .001f, float farDist = 500, bool invVrt = true);
	void    Save(const char *filename);
	bool    Read(const char *filename);
	CameraAB() { };
	CameraAB(int *vp, vec3 rot = vec3(0,0,0), vec3 tran = vec3(0,0,0),
			 float fov = 30, float nearDist = .001f, float farDist = 500, bool invVrt = true);
	CameraAB(int scrnX, int scrnY, int scrnW, int scrnH, vec3 rot = vec3(0,0,0), vec3 tran = vec3(0,0,0),
			 float fov = 30, float nearDist = .001f, float farDist = 500, bool invVrt = true);
	CameraAB(int scrnX, int scrnY, int scrnW, int scrnH, Quaternion rot, vec3 tran = vec3(0,0,0),
			 float fov = 30, float nearDist = .001f, float farDist = 500, bool invVrt = true);
friend class Arcball;
};

#endif
