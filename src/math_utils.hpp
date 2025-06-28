#define _USE_MATH_DEFINES
#include <math.h>

// ===== header =====
unsigned int getSeed();
float randf();
int isNumber(const char *str);
void cross3f_to_vec3(float v1[3], float v2[3], float res[3]);
float dot(float u[3], float v[3]);
void normalize_in_place(float v[3]);
float maxf(float a, float b);
static inline float radiansf(float degrees);
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

// does not check for 0 vector
void normalize_in_place(float v[3]) {
    float norm_factor = 1.0f / sqrtf(dot(v, v));
    v[0] *= norm_factor;
    v[1] *= norm_factor;
    v[2] *= norm_factor;
}

float maxf(float a, float b) { 
	return b < a ? a : b;
}

static inline float radiansf(float degrees) { return degrees * 0.01745329251994329576923690768489f; }