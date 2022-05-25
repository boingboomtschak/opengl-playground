// dColliders.h - Colliders (sphere/AABB/OBB/convex hull)

struct Collider {
    bool collides() {
        
    }
};

// Sphere collider
struct Sphere : Collider {
    Sphere(const vector<vec3> points) {
        
    }
};

// Axis-aligned bounding box collider
struct AABB : Collider {
    AABB(const vector<vec3> points) {
    
    }
};

// Oriented bounding box collider
struct OBB : Collider {
    OBB(const vector<vec3> points) {
        
    }
};

// Convex hull collider
struct ConvexHull : Collider {
    ConvexHull(const vector<vec3> points) {
        
    }
};
