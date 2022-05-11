// dParticles.h : Particle emitting system

#ifndef DPARTICLES_HDR
#define DPARTICLES_HDR

#include <glad.h>
#include <vector>
#include <stdexcept>
#include "VecMat.h"
#include "GLXtras.h"
#include "GeomUtils.h"

using std::vector;
using std::runtime_error;

struct dParticle {
    vec3 pos, vel;
    vec4 color;
    float life = 0.0f;
};

struct dParticles {
    int lastUsed = 0;
    int max_particles = 500;
    float particle_size = 0.015f;
    float life_dt = 0.02f;
    vec2 y_variance = vec2(0.03f, 0.05f);
    vec2 xz_variance = vec2(-0.01f, 0.01f);
    vec3 gravity = vec3(0, -0.01f, 0);
    dParticles() { };
    vector<dParticle> particles;
    void setup();
    void cleanup();
    int findDead();
    void createParticle(vec3 pos, vec3 color);
    void draw(mat4 vp, GLuint texUnit = 0, GLuint texture = 0, float xzrange = 0);
};

#endif
