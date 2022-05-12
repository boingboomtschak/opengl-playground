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
    {6, 5, 4}, {4, 7, 6}, // back
    {10, 9, 8}, {8, 11, 10}, // left
    {12, 13, 14}, {14, 15, 12}, // right
    {16, 17, 18}, {18, 19, 16}, // top
    {20, 21, 22}, {22, 23, 20}  // bottom
};

const char* particleVert = R"(
    #version 410 core
    in vec3 point;
    uniform mat4 mvp;
    void main() {
        gl_Position = mvp * vec4(point, 1);
    }
)";

const char* particleFrag = R"(
    #version 410 core
    uniform vec4 color;
    uniform vec2 uv;
    uniform int useTexture = 0;
    uniform sampler2D colorTexture;
    out vec4 fragColor;
    void main() {
        if (useTexture == 0) {
            fragColor = color;
        } else { 
            fragColor = 1.2 * texture(colorTexture, uv) + color;
        }
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
    for (int i = lastUsed; i < max_particles; i++) {
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
    particles[p].life = 1.0f;
    particles[p].vel = vec3(
        rand_float(xz_variance.x, xz_variance.y),
        rand_float(y_variance.x, y_variance.y),
        rand_float(xz_variance.x, xz_variance.y)
    );
}

void dParticles::draw(float dt, mat4 vp, GLuint texture, float xzrange) {
    glUseProgram(particleShader);
    glBindVertexArray(particleVArray);
    for (int i = 0; i < max_particles; i++) {
        if (particles[i].life > 0.0f) {
            // Run particle
            particles[i].life -= dt * life_dt;
            particles[i].pos += dt * particles[i].vel;
            particles[i].vel += dt * gravity;
            if (particles[i].pos.y < 0.0f) particles[i].pos.y = 0.0f;
            // Draw particle
            SetUniform(particleShader, "mvp", vp * Translate(particles[i].pos) * Scale(particle_size));
            if (texture > 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);
                vec2 uv = vec2(particles[i].pos.x / (xzrange * 2.0f) + 0.5, particles[i].pos.z / (xzrange * 2.0f) + 0.5);
                float value = rand_float(-0.1, 0.1);
                vec4 color = vec4(value, value, value, 1.0f);
                SetUniform(particleShader, "useTexture", 1);
                SetUniform(particleShader, "uv", uv);
                SetUniform(particleShader, "colorTexture", 0);
                SetUniform(particleShader, "color", color);
            } else {
                SetUniform(particleShader, "useTexture", 0);
                SetUniform(particleShader, "color", particles[i].color);
            }
            glDrawElements(GL_TRIANGLES, (GLsizei)(particleTriangles.size() * sizeof(int3)), GL_UNSIGNED_INT, 0);
        }
    }
    glBindVertexArray(0);
}
