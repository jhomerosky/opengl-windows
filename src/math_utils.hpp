#define _USE_MATH_DEFINES
#include <math.h>

// ===== header =====
unsigned int getSeed();
float randf();
static inline float radiansf(float degrees);
int isNumber(const char *str);
float maxf(float a, float b);
float dot(float u[3], float v[3]);
void quat_mult(float p[4], float q[4]);
void cross3f_to_vec3(float v1[3], float v2[3], float res[3]);
void normalize_in_place3f(float v[3]);
// ===== end header =====

// get a seed for the srand function
unsigned int getSeed() { 
	unsigned int seed; 
	seed = (unsigned int)(uintptr_t)&seed;
	return seed;
}

// clamp random between 
float randf() { 
	return (float)2 * (rand() - RAND_MAX/2) / RAND_MAX;
 }

int isNumber(const char *str) { 
	if (str == NULL || *str == '\0') return 0;
	char* end;
	strtol(str, &end, 10);
	return *end == '\0';
}

void cross3f_to_vec3(float v1[3], float v2[3], float res[3]) {
    res[0] = v1[1] * v2[2] - v1[2] * v2[1];
    res[1] = v1[2] * v2[0] - v1[0] * v2[2];
    res[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

float dot(float u[3], float v[3]) {
	return u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
}

// multiple quaternion p by q and store in res
// res must not be the same array as an input
void quat_mult(float p[4], float q[4], float res[4]) {
	// take a quaternion p, take p = (w, v) where w is scalar, v in R3
	// resw = pw*qw - dot(pv, qv)
	// resv = pw*qv + qw*pv + cross(pv, qv)
	res[0] = -(p[1] * q[1] + p[2]*q[2] + p[3]*q[3]);
	res[1] = p[2] * q[3] - p[3] * q[2];
    res[2] = p[3] * q[1] - p[1] * q[3];
    res[3] = p[1] * q[2] - p[2] * q[1];

	res[0] += p[0] * q[0];
	res[1] += p[0] * q[1] + q[0] * p[1];
	res[2] += p[0] * q[2] + q[0] * p[2];
	res[3] += p[0] * q[3] + q[0] * p[3];
}

// rotate a quaternion p by q and replace p with the result
// p,q normalized implies p' = qpq* is normalized, but maybe some float drift
void quat_rotate_in_place(float orientation[4], float rotation[4]) {
	// p' = qpq* = q(pq*)
	float temp[4];
	float rotate_conj[4] = {rotation[0], -rotation[1], -rotation[2], -rotation[3]};
	quat_mult(orientation, rotate_conj, temp);
	quat_mult(rotation, temp, orientation);
}

// does not check for 0 vector
void normalize_in_place3f(float v[3]) {
    float norm_factor = 1.0f / sqrtf(dot(v, v));
    v[0] *= norm_factor;
    v[1] *= norm_factor;
    v[2] *= norm_factor;
}

float maxf(float a, float b) { 
	return b < a ? a : b;
}

static inline float radiansf(float degrees) { return degrees * 0.01745329251994329576923690768489f; }