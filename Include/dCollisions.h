// dCollisions.h - Colliders (sphere/AABB/OBB/convex hull) and related utilities

#ifndef DCOLLISIONS_HDR
#define DCOLLISIONS_HDR

#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include "VecMat.h"
#include "GeomUtils.h"
#include "dCamera.h"

using std::vector;
using std::runtime_error;
using f_lim = std::numeric_limits<float>;
using std::max;

enum class ColliderType { None, Sphere, AABB, OBB, ConvexHull };

// Abstract collider type
struct Collider {
    ColliderType type = ColliderType::None;
    vec3 center = vec3(0, 0, 0);
};

// Forward declaration for collision functions
struct Sphere;
struct AABB;
struct OBB;
struct ConvexHull;

// Sphere collider
struct Sphere : Collider {
    float radius = 0.0f;
    Sphere(const vector<vec3>& points) {
        type = ColliderType::Sphere;
        vec3 _min = vec3(f_lim::max());
        vec3 _max = vec3(-f_lim::max());
        for (const vec3& pt : points) {
            if (pt.x < _min.x) _min.x = pt.x;
            if (pt.y < _min.y) _min.y = pt.y;
            if (pt.z < _min.z) _min.z = pt.z;
            if (pt.x > _max.x) _max.x = pt.x;
            if (pt.y > _max.y) _max.y = pt.y;
            if (pt.z > _max.z) _max.z = pt.z;
        }
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
    vec3 min, max;
    AABB(const vector<vec3>& points) {
        type = ColliderType::AABB;
        for (const vec3& pt : points) {
            if (pt.x < min.x) min.x = pt.x;
            if (pt.y < min.y) min.y = pt.y;
            if (pt.z < min.z) min.z = pt.z;
            if (pt.x > max.x) max.x = pt.x;
            if (pt.y > max.y) max.y = pt.y;
            if (pt.z > max.z) max.z = pt.z;
        }
        center = (min + max) / 0.5;
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
    float distance(vec3 pt) {
        return dot(normal, pt - point);
    }
    bool onOrBehindPlane(vec3 pt) {
        return distance(pt) <= 0;
    }
    bool onOrBehindPlane(vec3 center, float radius) {
        return distance(center) <= radius;
    }
};

struct Frustum {
    Plane topFace, bottomFace, leftFace, rightFace, nearFace, farFace;
    Frustum(Camera cam) {
        const float aspect = (float)cam.width / (float)cam.height;
        const vec3 camForward = normalize(cam.look - cam.loc);
        const vec3 camLeft = normalize(cross(cam.up, camForward)); // right-handed cross-product
        const float t = tanf(.5f * cam.fov * M_PI / 180.f);               // fov in degrees
        const vec3 topTangent = camForward + t * cam.up;                // cam.up assumed unit-length
        const vec3 bottomTangent = camForward - t * cam.up;
        const vec3 leftTangent = camForward + t * aspect * camLeft;
        const vec3 rightTangent = camForward - t * aspect * camLeft;
        topFace = { normalize(cross(topTangent, camLeft)), cam.loc };
        bottomFace = { normalize(cross(camLeft, bottomTangent)), cam.loc };
        leftFace = { normalize(cross(cam.up, leftTangent)), cam.loc };
        rightFace = { normalize(cross(rightTangent, cam.up)), cam.loc };
        nearFace = { -camForward, cam.loc + cam.zNear * camForward };
        farFace = { camForward, cam.loc + cam.zFar * camForward };
    }
    bool inFrustum(vec3 point) {
        return topFace.onOrBehindPlane(point) 
            && bottomFace.onOrBehindPlane(point) 
            && leftFace.onOrBehindPlane(point) 
            && rightFace.onOrBehindPlane(point) 
            && nearFace.onOrBehindPlane(point) 
            && farFace.onOrBehindPlane(point);
    }
    bool inFrustum(mat4 transform, Sphere* collider) {
        vec3 tf_center (transform * vec4(collider->center, 1));
        return topFace.onOrBehindPlane(tf_center, collider->radius) 
            && bottomFace.onOrBehindPlane(tf_center, collider->radius) 
            && leftFace.onOrBehindPlane(tf_center, collider->radius) 
            && rightFace.onOrBehindPlane(tf_center, collider->radius) 
            && nearFace.onOrBehindPlane(tf_center, collider->radius) 
            && farFace.onOrBehindPlane(tf_center, collider->radius);
    }

};

vector<mat4> cull_instances_sphere(Frustum frustum, Collider* collider, vector<mat4>& instance_transforms) {
    if (collider->type != ColliderType::Sphere)
        throw runtime_error("Invalid collider type, not Sphere!");
    vector<mat4> culled;
    for (mat4& tf : instance_transforms)
        if (frustum.inFrustum(tf, (Sphere*)collider)) { culled.push_back(tf); }
    return culled;
}

vector<mat4> cull_instances_aabb(Collider* collider, vector<mat4>& instance_transforms, mat4 vp, mat4 model) {
    if (collider->type != ColliderType::AABB)
        throw runtime_error("Invalid collider type, not AABB!");
    AABB* c = (AABB*)collider;
    vector<mat4> culled; 
    for (mat4& tf : instance_transforms) {
        const mat4 m = tf * model;
        const vec4 tf_min = m * c->min;
        const vec4 tf_max = m * c->max;
        const vec4 corners[8] = {
            {tf_min.x, tf_min.y, tf_min.z, 1.0}, 
            {tf_max.x, tf_min.y, tf_min.z, 1.0}, 
            {tf_min.x, tf_max.y, tf_min.z, 1.0}, 
            {tf_max.x, tf_max.y, tf_min.z, 1.0}, 
            {tf_min.x, tf_min.y, tf_max.z, 1.0}, 
            {tf_max.x, tf_min.y, tf_max.z, 1.0}, 
            {tf_min.x, tf_max.y, tf_max.z, 1.0}, 
            {tf_max.x, tf_max.y, tf_max.z, 1.0}, 
        };
        bool inside = false;
        for (size_t i = 0; i < 8; i++) {
            const vec4 corner = vp * corners[i];
            inside = inside || (corner.x > -corner.w && corner.x < corner.w && corner.y > -corner.w && corner.y < corner.w && corner.z > -corner.w && corner.z < corner.w);
        }
        if (inside) { culled.push_back(tf); }
    }
    return culled;
}

#endif
