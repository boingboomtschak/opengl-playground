// GeomUtils.h : Devon McKee
// Series of functions to help calculate geometry

#ifndef GEOM_UTIL_HDR
#define GEOM_UTIL_HDR

#define _USE_MATH_DEFINES

#include <math.h>
#include <algorithm>
#include <vector>
#include "VecMat.h"

using std::vector;
using std::min;
using std::max;

// Returns float between min and max, defaults to 0 and 1, respectively
// Best practice would be to seed rand() beforehand, but not guaranteed in this fn
inline float rand_float(float min = 0, float max = 1) { return min + (float)rand() / (RAND_MAX / (max - min)); }

// Returns random vec3 with all members between 0 and 1
inline vec3 rand_vec3(float min = -1, float max = 1) { return vec3(min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min)), min + (float)rand() / (RAND_MAX / (max - min))); }

// Returns random vec3 with length of 1 (usually a direction / velocity vector)
inline vec3 rand_dir() {
	float theta = rand_float(0, 2.0f * (float)M_PI);
	float z = rand_float(-1, 1);
	return vec3(sqrt(1 - (z * z)) * cos(theta), sqrt(1 - (z * z)) * sin(theta), z);
}

// Returns float distance between two vec3 objs
inline float dist(vec3 p1, vec3 p2) { return (float)sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2) + pow(p2.z - p1.z, 2)); }

// Returns float distance from point p to segment formed by points v and w (VW)
inline float dist_to_segment(vec3 p, vec3 v, vec3 w) {
	float l = dist(v, w);
	if (l == 0.0) return dist(p, v);
	float t = max(0.0f, min(1.0f, dot(p - v, w - v)));
	vec3 proj = v + t * (w - v);
	return dist(p, proj);
}

// Returns true if the point p is to the left of the segment formed by points v and w (VW)
inline bool point_segment_left(vec3 p, vec3 v, vec3 w) { return ((w.x - v.x) * (p.y - v.y) - (w.y - v.y) * (p.x - v.x)) > 0; }

// Returns centroid of a set of points
vec3 centroid(vector<vec3> points) {
	vec3 c = vec3(0.0f);
	for (size_t i = 0; i < points.size(); i++) 
		c += points[i];
	c /= (float)points.size();
	return c;
}

// Samples 
vector<vec3> SampleCircle(vec3 center, int num, int radius) {
	vector<vec3> v;
	for (int i = 0; i < num; i++) {
		float r = radius * (float)sqrt(rand_float());
		float theta = rand_float(0.0f, 2.0f * (float)M_PI);
		float x = center.x + r * cos(theta);
		float y = center.y + r * sin(theta);
		v.push_back(vec3(x, y, center.z));
	}
	return v;
}

// Recursive function used for QuickHull(), unused otherwise
vector<vec3> _FindHull(vector<vec3> points, vec3 p, vec3 q) {
	vector<vec3> hull;
	// Return if no more points
	if (points.size() == 0) {
		return points;
	}
	// Find furthest point from PQ as M and add to hull
	float m_dist = 0;
	vec3 m;
	for (size_t i = 0; i < points.size(); i++) {
		float d = dist_to_segment(points[i], p, q);
		if (d > m_dist) {
            m_dist = d;
            m = points[i];
		}
	}
	hull.push_back(m);
	// Partition points to left of PM and to left of MQ
	vector<vec3> lpm, lmq;
	for (size_t i = 0; i < points.size(); i++) {
		if (&(points[i]) != &m) {
			if (point_segment_left(points[i], p, m)) {
				lpm.push_back(points[i]);
			}
			else if (point_segment_left(points[i], m, q)) {
				lmq.push_back(points[i]);
			}
		}
	}
	// Recursively call FindHull to find rest of hull from remaining points
	// Add returned points from recursive calls to hull
	vector<vec3> nhull;
	nhull = _FindHull(lpm, p, m);
	hull.insert(hull.end(), nhull.begin(), nhull.end());
	nhull = _FindHull(lmq, m, q);
	hull.insert(hull.end(), nhull.begin(), nhull.end());
	return hull;
}

// Finds the convex hull of a series of points
vector<vec3> QuickHull(vector<vec3> points) {
	vector<vec3> hull;
	vec3 c = centroid(points);
	// Find left and rightmost points
	vec3 l = c, r = c;
	for (size_t i = 0; i < points.size(); i++) {
		if (points[i].x < l.x)
			l = points[i];
		if (points[i].x > r.x)
			r = points[i];
	}
	hull.push_back(l);
	hull.push_back(r);
	// Divide set into above and below line
	vector<vec3> above;
	vector<vec3> below;
	for (size_t i = 0; i < points.size(); i++) {
		if (point_segment_left(points[i], l, r) && &(points[i]) != &l && &(points[i]) != &r)
			above.push_back(points[i]);
		else
			below.push_back(points[i]);
	}
	// Find above and below hull points with FindHull
	vector<vec3> ahull = _FindHull(above, l, r);
	vector<vec3> bhull = _FindHull(below, r, l);
	// Attach above and below hulls to total hull
	hull.insert(hull.end(), ahull.begin(), ahull.end());
	hull.insert(hull.end(), bhull.begin(), bhull.end());
	return hull;
}

// Creates an orientation matrix 
inline mat4 Orientation(vec3 forward, vec3 up) {
	vec3 z = normalize(forward);
	vec3 x = normalize(cross(up, z));
	vec3 y = normalize(cross(z, x));
	return mat4(vec4(x.x, y.x, z.x, 0), vec4(x.y, y.y, z.y, 0), vec4(x.z, y.z, z.z, 0), vec4(0, 0, 0, 1));
}

// Create bias matrix to convert points from [-1, 1] to [0, 1] for shadow map sampling
inline mat4 DepthBias() {
	return mat4(
		vec4(0.5, 0.0, 0.0, 0.0), 
		vec4(0.0, 0.5, 0.0, 0.0), 
		vec4(0.0, 0.0, 0.5, 0.0), 
		vec4(0.5, 0.5, 0.5, 1.0)
	);
}

// 3x3 matrix determinant
inline float determinant(mat3 m) {
	return (m[0][0]*m[1][1]*m[2][2]) 
		+ (m[0][1]*m[1][2]*m[2][0]) 
		+ (m[0][2]*m[1][0]*m[2][1]) 
		- (m[0][2]*m[1][1]*m[2][0]) 
		- (m[0][1]*m[1][0]*m[2][2]) 
		- (m[0][0]*m[1][2]*m[2][1]);
}

// Get matrix produced by removing row i and col j
inline mat3 submatrix(mat4 m, int i, int j) {
	mat3 sm;
	for (int a = 0; a < 3; a++)
		for (int b = 0; b < 3; b++)
			sm[a][b] = m[a >= i ? a + 1 : a][b >= j ? b + 1 : b];
	return sm;
}

// 4x4 matrix determinant
inline float determinant(mat4 m) {
	return m[0][0] * determinant(submatrix(m, 0, 0))
		- m[1][0] * determinant(submatrix(m, 1, 0))
		+ m[2][0] * determinant(submatrix(m, 2, 0))
		- m[3][0] * determinant(submatrix(m, 3, 0));
}

// Inverse of mat4
inline mat4 inverse(mat4 m) {
	mat4 adj;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			adj[i][j] = (float)pow(-1, i + j) * determinant(submatrix(m, j, i));
	return (1.0f / determinant(m)) * adj;
}

inline vector<vec4> frustumCorners(mat4 proj, mat4 view) {
	mat4 inv = inverse(proj * view);
	vector<vec4> corners;
	for (int x = 0; x < 2; x++) {
		for (int y = 0; y < 2; y++) {
			for (int z = 0; z < 2; z++) {
				vec4 p = inv * vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
				corners.push_back(p / p.w);
			}
		}
	}
	return corners;
}

// Rodrigues' rotation formula for rotation matrix around a vector 
inline mat4 RotateAxis(vec3 axis, float theta) {
	float a = DegreesToRadians*theta; // angle (converted to rads)
	vec3 o = normalize(axis); // omega (unit vector)
	return mat4(
		vec4(cos(a) + (o.x*o.x)*(1-cos(a)), o.x*o.y*(1-cos(a))-o.z*sin(a), o.y*sin(a)+o.x*o.z*(1-cos(a)), 0.0),
		vec4(o.z*sin(a)+o.x*o.y*(1-cos(a)), cos(a)+(o.y*o.y)*(1-cos(a)), -o.x*sin(a)+o.y*o.z*(1-cos(a)), 0.0),
		vec4(-o.y*sin(a)+o.x*o.z*(1-cos(a)), o.x*sin(a)+o.y*o.z*(1-cos(a)), cos(a)+(o.z*o.z)*(1-cos(a)), 0.0),
		vec4(0.0, 0.0, 0.0, 1.0)
	);
}

#endif // GEOM_UTIL_HDR
