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
    float distance(vec3 pt) {
        return normal.x * pt.x + normal.y * pt.y + normal.z * pt.z + dot(-pt, normal);
    }
    bool onOrBehindPlane(vec3 pt) {
        return dot(normal, pt - point) <= 0;
    }
    bool onOrBehindPlane(vec3 center, float radius) {
        return distance(center) <= radius || onOrBehindPlane(center);
    }
};

struct Frustum {
    Plane top, bottom, left, right, near, far;
    Frustum(Camera cam) {
        const float aspect = cam.width / cam.height;
        const float halfX = cam.zFar * tanf(cam.fov * 0.5f);
        const float halfY = halfX / aspect;
        const vec3 camForward = normalize(cam.look - cam.loc);
        const vec3 camLeft = cross(cam.up, camForward);
        const vec3 farMiddle = cam.zFar * camForward;
        near = { -camForward, cam.pos + cam.zNear * camForward };
        far = { camForward, cam.pos + farMiddle };
        left = { normalize(cross(cam.up, farMiddle + camLeft * halfX)), cam.pos };
        right = { normalize(cross(cam.up, farMiddle - camLeft * halfX)), cam.pos };
        top = { normalize(cross(camLeft, farMiddle + cam.up * halfY)), cam.pos };
        bottom = { normalize(cross(camLeft, farMiddle - cam.up * halfY)), cam.pos };
    }
    bool inFrustum(vec3 point) {
        return top.onOrBehindPlane(point) && bottom.onOrBehindPlane(point) && left.onOrBehindPlane(point) && right.onOrBehindPlane(point) && near.onOrBehindPlane(point) && far.onOrBehindPlane(point);
    }
    bool inFrustum(mat4& transform, Sphere collider) {
        vec3 tf_center (transform * vec4(collider.center, 1));
        return top.onOrBehindPlane(tf_center, collider.radius) && bottom.onOrBehindPlane(tf_center, collider.radius) && left.onOrBehindPlane(tf_center, collider.radius) && right.onOrBehindPlane(tf_center, collider.radius) && near.onOrBehindPlane(tf_center, collider.radius) && far.onOrBehindPlane(tf_center, collider.radius);
    }
};
