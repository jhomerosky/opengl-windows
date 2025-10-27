#ifndef __my_math__
#include <math.h>

// ===== header =====
unsigned int getSeed();
float randf();
static inline float radiansf(float degrees);
static inline float degreesf(float radians);
static inline int isNumber(const char *str);
static inline void set3u(unsigned int out[3], const unsigned int n0, const unsigned int n1, const unsigned int n2);
static inline void set3uv(unsigned int out[3], const unsigned int in[3]);
static inline void set3i(int out[3], const int n0, const int n1, const int n2);
static inline void set3iv(int out[3], const int in[3]);
static inline void set3f(float out[3], const float f0, const float f1, const float f2);
static inline void set3fv(float out[3], const float in[3]);
static inline void set4f(float out[4], const float f0, const float f1, const float f2, const float f3);
static inline void set4fv(float out[4], const float in[4]);
static inline float maxf(float a, float b);
static inline float dot3f(const float u[3], const float v[3]);
static inline float dot4f(const float u[4], const float v[4]);
static inline float fast_rsqrt(float number);
static inline void quat_mult(float p[4], float q[4], float res[4]);
static inline void negate3f_inplace(float out[3]);
static inline void add3f(float out[3], const float v1[3], const float v2[3]);
static inline void sub3f(float out[3], const float v1[3], const float v2[3]);
static inline void mult3f(float out[3], const float in[3], const float factor);
static inline void cross3f(float out[3], const float v1[3], const float v2[3]);
static inline void normalize3f_inplace(float v[3]);
static inline bool equals3f(const float v1[3], const float v2[3], const float eps);
static inline void mat4_mul(float out[16], const float A[16], const float B[16]);
static inline void set_perspective_mat(float out[16], const float fovy, const float aspect, const float near, const float far);
static inline void set_lookat_mat(float out[16], const float eye[3], const float front[3], const float up[3]);
static inline void set_translate_mat(float out[16], const float pos[3]);
static inline void set_rotation_mat(float out[16], const float quat[4]);
static inline void set_scale_mat(float out[16], const float scale[3]);
static inline void set_normal_mat(float out[9], const float rotate[16], const float scale[3]);
static inline void remove_translation_mat4(float mat[16]);
static inline void set_triangle_normal(float out[3], const float A[3], const float B[3], const float C[3]);
// ===== end header =====

// get a seed for the srand function
unsigned int getSeed() { 
	unsigned int seed; 
	seed = (unsigned int)(uintptr_t)&seed;
	return seed;
}

// random in (-1, 1)
float randf() { 
	return (float)2 * (rand() - RAND_MAX/2) / RAND_MAX;
 }

static inline int isNumber(const char *str) { 
	if (str == NULL || *str == '\0') return 0;
	char* end;
	strtol(str, &end, 10);
	return *end == '\0';
}

static inline bool equals3f(const float v1[3], const float v2[3], const float eps) {
	return (
		fabsf(v1[0] - v2[0]) < eps &&
		fabsf(v1[1] - v2[1]) < eps &&
		fabsf(v1[2] - v2[2]) < eps
	);
}

// out = -out
static inline void negate3f_inplace(float out[3]) {
	out[0] = -out[0];
	out[1] = -out[1];
	out[2] = -out[2];
}

// out = v1 + v2, safe to use A = A + B
static inline void add3f(float out[3], const float v1[3], const float v2[3]) {
	out[0] = v1[0] + v2[0];
	out[1] = v1[1] + v2[1];
	out[2] = v1[2] + v2[2];
}

// out = v1 - v2; safe to use A = A - B
static inline void sub3f(float out[3], const float v1[3], const float v2[3]) {
	out[0] = v1[0] - v2[0];
	out[1] = v1[1] - v2[1];
	out[2] = v1[2] - v2[2];
}

// out = in * factor; safe to use A = A*factor
static inline void mult3f(float out[3], const float in[3], const float factor) {
	out[0] = in[0]*factor;
	out[1] = in[1]*factor;
	out[2] = in[2]*factor;
}

// out = v1 cross v2; not safe for A = A cross B
static inline void cross3f(float out[3], const float v1[3], const float v2[3]) {
    out[0] = v1[1] * v2[2] - v1[2] * v2[1];
    out[1] = v1[2] * v2[0] - v1[0] * v2[2];
    out[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static inline float dot3f(const float u[3], const float v[3]) {
	return u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
}

static inline float dot4f(const float u[4], const float v[4]) {
	return u[0]*v[0] + u[1]*v[1] + u[2]*v[2] + u[3]*v[3];
}

// multiple quaternion p by q and store in res
// res must not be the same array as an input
static inline void quat_mult(float p[4], float q[4], float res[4]) {
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
static inline void quat_rotate_in_place(float orientation[4], float rotation[4]) {
	// p' = qpq* = q(pq*)
	float temp[4];
	float rotate_conj[4] = {rotation[0], -rotation[1], -rotation[2], -rotation[3]};
	quat_mult(orientation, rotate_conj, temp);
	quat_mult(rotation, temp, orientation);
}

// quake 3 fast inv sqrt
// this is probably not optimal on modern hardware, but it's here for fun
static inline float fast_rsqrt(float number) {
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    i  = *(long*)&y;             // treat float bits as int
    i  = 0x5f3759df - (i >> 1);  // magic
    y  = *(float*)&i;
    y  = y * (threehalfs - (x2 * y * y)); // 1 NR iteration
    return y;
}

// does not check for 0 vector
static inline void normalize3f_inplace(float v[3]) {
    float norm_factor = 1.0f / sqrtf(dot3f(v, v));
	//float norm_factor = fast_rsqrt(dot3f(v,v));
    v[0] *= norm_factor;
    v[1] *= norm_factor;
    v[2] *= norm_factor;
}

static inline float maxf(float a, float b) { 
	return b < a ? a : b;
}

static inline float radiansf(float degrees) { return degrees * 0.01745329251994329576923690768489f; }
static inline float degreesf(float radians) { return radians * 57.295779513082320876798154814105f; }

// vector setting
static inline void set3u(unsigned int out[3], const unsigned int n0, const unsigned int n1, const unsigned int n2) { out[0] = n0; out[1] = n1; out[2] = n2; }
static inline void set3uv(unsigned int out[3], const unsigned int in[3]) { memcpy(out, in, 3*sizeof(unsigned int)); }

static inline void set3i(int out[3], const int n0, const int n1, const int n2) { out[0] = n0; out[1] = n1; out[2] = n2; }
static inline void set3iv(int out[3], const int in[3]) { memcpy(out, in, 3*sizeof(int)); }

static inline void set3f(float out[3], const float f0, const float f1, const float f2) { out[0] = f0; out[1] = f1; out[2] = f2; }
static inline void set3fv(float out[3], const float in[3]) { memcpy(out, in, 3*sizeof(float)); }

static inline void set4f(float out[4], const float f0, const float f1, const float f2, const float f3) { out[0] = f0; out[1] = f1; out[2] = f2; out[3] = f3; }
static inline void set4fv(float out[4], const float in[4]) { memcpy(out, in, 4*sizeof(float)); }

// mat4_mul(out, A, B) --> out = AB (safe for A=AB)
static inline void mat4_mul(float out[16], const float A[16], const float B[16]) {
	float temp[16];
	temp[0]  = A[0]*B[0]  + A[4]*B[1]  + A[8]*B[2]   + A[12]*B[3];
	temp[1]  = A[1]*B[0]  + A[5]*B[1]  + A[9]*B[2]   + A[13]*B[3];
	temp[2]  = A[2]*B[0]  + A[6]*B[1]  + A[10]*B[2]  + A[14]*B[3];
	temp[3]  = A[3]*B[0]  + A[7]*B[1]  + A[11]*B[2]  + A[15]*B[3];

	temp[4]  = A[0]*B[4]  + A[4]*B[5]  + A[8]*B[6]   + A[12]*B[7];
	temp[5]  = A[1]*B[4]  + A[5]*B[5]  + A[9]*B[6]   + A[13]*B[7];
	temp[6]  = A[2]*B[4]  + A[6]*B[5]  + A[10]*B[6]  + A[14]*B[7];
	temp[7]  = A[3]*B[4]  + A[7]*B[5]  + A[11]*B[6]  + A[15]*B[7];

	temp[8]  = A[0]*B[8]  + A[4]*B[9]  + A[8]*B[10]  + A[12]*B[11];
	temp[9]  = A[1]*B[8]  + A[5]*B[9]  + A[9]*B[10]  + A[13]*B[11];
	temp[10] = A[2]*B[8]  + A[6]*B[9]  + A[10]*B[10] + A[14]*B[11];
	temp[11] = A[3]*B[8]  + A[7]*B[9]  + A[11]*B[10] + A[15]*B[11];

	temp[12] = A[0]*B[12] + A[4]*B[13] + A[8]*B[14]  + A[12]*B[15];
	temp[13] = A[1]*B[12] + A[5]*B[13] + A[9]*B[14]  + A[13]*B[15];
	temp[14] = A[2]*B[12] + A[6]*B[13] + A[10]*B[14] + A[14]*B[15];
	temp[15] = A[3]*B[12] + A[7]*B[13] + A[11]*B[14] + A[15]*B[15];
	memcpy(out, temp, sizeof(float)*16);
}

// build perspective matrix, which is a type of projection matrix
// perspective(fovy, aspect, near, far) = 
// 1/(aspect*tan(fovy/2))               0                       0                         0
//                      0   1/tan(fovy/2)                       0                         0
//                      0               0   (near+far)/(near-far)   (2*near*far)/(near-far)
//                      0               0                      -1                         0
static inline void set_perspective_mat(float out[16], const float fovy, const float aspect, const float near, const float far) {
	float tanHalfFovy = tan(fovy/2.0f);
	out[0] = 1.0f/(aspect*tanHalfFovy);
	out[5] = 1.0f/(tanHalfFovy);
	out[10] = (near+far)/(near-far);
	out[11] = -1.0;
	out[14] = (2.0f*near*far)/(near-far);

	out[1] = 0.0f;
	out[2] = 0.0f;
	out[3] = 0.0f;

	out[4] = 0.0f;
	out[6] = 0.0f;
	out[7] = 0.0f;

	out[8] = 0.0f;
	out[9] = 0.0f;

	out[12] = 0.0f;
	out[13] = 0.0f;
	out[15] = 0.0f;
}

// build the view matrix via lookAt parameters
// given f = normalize(center - eye)
//       r = normalize(cross(f, up))
//       u = cross(r, f)
// note: normally takes center as input. center = pos + front (where front is direction camera is looking)
// lookat(eye, center, up) = 
//         r[0]           u[0]         -f[0]   0
//         r[1]           u[1]         -f[1]   0
//         r[2]           u[2]         -f[2]   0
// -dot(r, eye)   -dot(u, eye)   dot(f, eye)   1
static inline void set_lookat_mat(float out[16], const float eye[3], const float front[3], const float up[3]) {
	float f[3];
	float r[3];
	float u[3];

	// build f
	set3f(f, front[0], front[1], front[2]);
	normalize3f_inplace(f);

	// build r
	cross3f(r, f, up);
	normalize3f_inplace(r);

	// build u. right and forward are orthogonal unit vectors so no need to normalize the cross product
	cross3f(u, r, f);

	// set out matrix
	out[0]  = r[0];
	out[4]  = r[1];
	out[8]  = r[2];
	out[12] = -dot3f(r, eye);

	out[1]  = u[0];
	out[5]  = u[1];
	out[9]  = u[2];
	out[13] = -dot3f(u, eye);

	out[2]  = -f[0];
	out[6]  = -f[1];
	out[10] = -f[2];
	out[14] = dot3f(f, eye);

	out[3]  = 0.0f;
	out[7]  = 0.0f;
	out[11] = 0.0f;
	out[15] = 1.0f;
}

// build the translate matrix
static inline void set_translate_mat(float out[16], const float pos[3]) {
	// setting column vectors
	set4f(&out[0],    1.0f,   0.0f,   0.0f, 0.0f);
	set4f(&out[4],    0.0f,   1.0f,   0.0f, 0.0f);
	set4f(&out[8],    0.0f,   0.0f,   1.0f, 0.0f);
	set4f(&out[12], pos[0], pos[1], pos[2], 1.0f);
}

// build the rotation matrix from a quaternion
static inline void set_rotation_mat(float out[16], const float quat[4]) {
	const float xx = quat[1]*quat[1];
	const float yy = quat[2]*quat[2];
	const float zz = quat[3]*quat[3];

	const float xy = quat[1]*quat[2];
	const float xz = quat[1]*quat[3];
	const float yz = quat[2]*quat[3];
	
	const float wx = quat[0]*quat[1];
	const float wy = quat[0]*quat[2];
	const float wz = quat[0]*quat[3];

	out[0]  = 1.0f - 2.0f*(yy + zz);
	out[1]  =        2.0f*(xy + wz);
	out[2]  =        2.0f*(xz - wy);
	out[3]  = 0.0f;

	out[4]  =        2.0f*(xy - wz);
	out[5]  = 1.0f - 2.0f*(xx + zz);
	out[6]  =        2.0f*(yz + wx);
	out[7]  = 0.0f;

	out[8]  =        2.0f*(xz + wy);
	out[9]  =        2.0f*(yz - wx);
	out[10] = 1.0f - 2.0f*(xx - yy);
	out[11] = 0.0f;

	out[12] = 0.0f;
	out[13] = 0.0f;
	out[14] = 0.0f;
	out[15] = 1.0f;
}

// build the scale matrix
static inline void set_scale_mat(float out[16], const float scale[3]) {
	// setting column vectors
	set4f(&out[0], scale[0],     0.0f,     0.0f,   0.0f);
	set4f(&out[4],     0.0f, scale[1],     0.0f,   0.0f);
	set4f(&out[8],     0.0f,     0.0f, scale[2],   0.0f);
	set4f(&out[12],    0.0f,     0.0f,     0.0f,   1.0f);
}

// build the normal matrix from the model matrix.
// NOTE: 
//   Normal = mat3(Transpose(Inverse(Model))) = mat3(Transpose(Inverse(Translate*Rotate*Scale)))
//   Translate can be ignored since normals are not affected by it:
//       mat3((TRS)^(-T)) = mat3(T^(-T)(RS)^(-T)) = mat3(T^(-T))mat3((RS)^(-T)) where mat3(T^(-T)) = I
//   Rotate is orthogonal -> inverse is equal to the transpose
//   Scale is diagonal -> inverse is easy; transpose is identical; mult is easy
//       Model^(-T) = (Rotate*Scale)^(-T) = Rotate^(-T)Scale^(-T) = Rotate * Scale^(-1)
static inline void set_normal_mat(float out[9], const float rotate[16], const float scale[3]) {
	float sxInv = 1/scale[0];
	float syInv = 1/scale[1];
	float szInv = 1/scale[2];

	out[0] = rotate[0]*sxInv;
	out[1] = rotate[1]*sxInv;
	out[2] = rotate[2]*sxInv;

	out[3] = rotate[4]*syInv;
	out[4] = rotate[5]*syInv;
	out[5] = rotate[6]*syInv;

	out[6] = rotate[8]*szInv;
	out[7] = rotate[9]*szInv;
	out[8] = rotate[10]*szInv;
}

// this removes the translation component of a 4x4 mat; similar to glm::mat4(glm::mat3(my_matrix))
static inline void remove_translation_mat4(float mat[16]) {
	mat[3] = 0.0f;
	mat[7] = 0.0f;
	mat[11] = 0.0f;
	mat[12] = 0.0f;
	mat[13] = 0.0f;
	mat[14] = 0.0f;
	mat[15] = 1.0f;
}

// Given a triangle A, B, C, return the unit normal vector (B-A) x (C-B)
static inline void set_triangle_normal(float out[3], float A[3], float B[3], float C[3]) {
	float seg1[3];
	float seg2[3];
	sub3f(seg1, B, A);
	sub3f(seg2, C, B);
	cross3f(out, seg1, seg2);
	normalize3f_inplace(out);
}

#define __my_math__
#endif