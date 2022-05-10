// dParticles.cpp : Implementation for dParticles.h

#include "dParticles.h"

namespace {

GLuint particleShader = 0;
GLuint particleVArray = 0;
GLuint particleVBuffer = 0;
GLuint particleIBuffer = 0;

vector<vec3> particlePoints {
    {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}, // front (0, 1, 2, 3)
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, // back (4, 5, 6, 7)
    {-1, -1, 1}, {-1, -1, -1}, {-1, 1, -1}, {-1, 1, 1}, // left (8, 9, 10, 11)
    {1, -1, 1}, {1, -1, -1}, {1, 1, -1}, {1, 1, 1}, // right (12, 13, 14, 15)
    {-1 , 1, 1}, {1, 1, 1}, {1, 1, -1}, {-1, 1, -1}, // top  (16, 17, 18, 19)
    {-1, -1, 1}, {1, -1, 1}, {1, -1, -1}, {-1, -1, -1}  // bottom (20, 21, 22, 23)
};

vector<int3> particleTriangles {
    {0, 1, 2}, {2, 3, 0}, // front
    {4, 5, 6}, {6, 7, 4}, // back
    {8, 9, 10}, {10, 11, 8}, // left
    {12, 13, 14}, {14, 15, 12}, // right
    {16, 17, 18}, {18, 19, 16}, // top
    {20, 21, 22}, {22, 23, 20}  // bottom
};

const char* particleVert = R"(
    #version 410 core
    in vec3 point;
    uniform mat4 mvp;
    void main() {
        
    }
)";

const char* particleFrag = R"(
    #version 410 core
    uniform vec4 color;
    out vec4 fragColor;
    void main() {
        fragColor = vec4(pColor, 1.0);
    }
)";

}



void dParticles::setup() {
    if (!particleShader)
        particleShader = LinkProgramViaCode(&particleVert, &particleFrag);
    if (!particleVArray) {
        glGenVertexArrays(1, &particleVArray);
        glBindVertexArray(particleVArray);
        glGenBuffers(1, &particleVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, particleVBuffer);
        glBufferData(GL_ARRAY_BUFFER, particlePoints.size() * sizeof(vec3), particlePoints.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &particleIBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particleIBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, particleTriangles.size() * sizeof(int3), particleTriangles.data(), GL_STATIC_DRAW);
        VertexAttribPointer(particleShader, "point", 3, 0, 0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    // Initialize particles
    for (int i = 0; i < max_particles; i++)
        particles.push_back(dParticle());
    
}

void dParticles::cleanup() {
    if (particleVArray) {
        glDeleteVertexArrays(1, &particleVArray);
        glDeleteBuffers(1, &particleVBuffer);
    }
}

int dParticles::findDead() {
    // Search for dead particle starting from last used particle
    for (int i = lastUsed; i < num_particles; i++) {
        if (particles[i].life <= 0.0f) {
            lastUsed = i;
            return i;
        }
    }
    // Search linearly through rest of particles
    for (int i = 0; i < lastUsed; i++) {
        if (particles[i].life <= 0.0f) {
            lastUsed = i;
            return i;
        }
    }
    // If no dead particles exist, simply take the first particle
    lastUsed = 0;
    return 0;
}

void dParticles::createParticle(vec3 pos, vec3 color) {
    int p = findDead();
    particles[p].pos = pos;
    particles[p].color = color;
    particles[p].life = 0.0f;
    particles[p].vel = vec3(
        rand_float(xz_variance.x, xz_variance.y),
        rand_float(y_variance.x, y_variance.y),
        rand_float(xz_variance.x, xz_variance.y)
    );
}

void dParticles::draw(mat4 vp) {
    glUseProgram(particleShader);
    glBindVertexArray(particleVArray);
    for (dParticle particle : particles) {
        if (particle.life > 0.0f) {
            // Run particle
            particle.life -= life_dt;
            particle.pos += particle.vel;
            particle.vel -= gravity;
            // Draw particle
            SetUniform(particleShader, "mvp", Translate(particle.pos) * vp);
            SetUniform(particleShader, "color", particle.color);
            glDrawArrays(GL_TRIANGLES, 0, 0); // TODO
        }
    }
    glBindVertexArray(0);
}
