// dCollisions.h - Colliders (sphere/AABB/OBB/convex hull) and related utilities

#include <vector>
#include <algorithm>
#include <limits>
#include "VecMat.h"
#include "GeomUtils.h"
#include "dCamera.h"

using std::vector;
using f_lim = std::numeric_limits<float>;
using std::max;

struct Collider {};

// Forward declaration for collision functions
struct Sphere;
struct AABB;
struct OBB;
struct ConvexHull;

// Sphere collider
struct Sphere : Collider {
    vec3 center = vec3(0.0f);
    float radius = 0.0f;
    Sphere(const vector<vec3>& points) {
        vec3 _min = vec3(f_lim::min());
        vec3 _max = vec3(f_lim::max());
        for (const vec3 pt : points) 
            for (int i = 0; i < 3; i++)
                if (pt[i] < _min[i]) _min[i] = pt[i];
                else if (pt[i] > _max[i]) _max[i] = pt[i];
        center = (_min + _max) / 0.5;
        radius = max({ 
            center.x - _min.x, 
            center.y - _min.y, 
            center.z - _min.z,
            _max.x - center.x, 
            _max.y - center.y, 
            _max.z - center.z 
        });
    }
    bool collides(mat4& transform, vec3 point) {
        vec3 t_center (transform * vec4(center, 1));
        if (dist(t_center, point) < radius) return true;
        return false;
    }
    bool collides(mat4& transform, Sphere obj, mat4& obj_transform) {
        vec3 t_center(transform * vec4(center, 1));
        vec3 obj_t_center(obj_transform * vec4(obj.center, 1));
        if (dist(t_center, obj_t_center) < radius + obj.radius) return true;
        return false;
    }
};

// Axis-aligned bounding box collider
struct AABB : Collider {
    AABB(const vector<vec3>& points) {
        
    }
};

// Oriented bounding box collider
struct OBB : Collider {
    OBB(const vector<vec3>& points) {
        
    }
};

// Convex hull collider
struct ConvexHull : Collider {
    ConvexHull(const vector<vec3>& points) {
        
    }
};


struct Plane {
    vec3 normal = vec3(0, 1, 0);
    vec3 point = vec3(0.0f);
};

struct Frustum {
    Plane top, bottom, left, right, near, far;
    Frustum(Camera cam) {
        vec3 camForward = normalize(cam.look - cam.loc);
        near = {};
        far = {};
        left = {};
        right = {};
        top = {};
        bottom = {};
    }
};