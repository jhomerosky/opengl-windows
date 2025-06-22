#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define __MAX_MESHES__ 2048
#define __MAX_MODELS__ 2048

struct Vertex {
	float pos[3];
	float normal[3];
};

struct Face {
	unsigned int vertexId[3];
};

struct Mesh {
	Vertex* vertices = nullptr;
	Face* faces = nullptr;

	size_t num_vertices;
	size_t num_faces;

	bool has_normals;

	GLuint VAO;
	GLuint VBO;
	GLuint EBO;

	~Mesh() { free(vertices); free(faces); }
};

struct MeshInstance {
	Mesh* mesh;
	glm::mat4 model;
	glm::vec3 color;
};

// yaw should be initialized to -90.0f
struct Camera {
	glm::vec3 pos;
	glm::vec3 front;
	glm::vec3 up;

	float pitch;
	float yaw;
	float speed;

	float TOP_MOVE_SPEED;
	float NORMAL_MOVE_SPEED;

	float PAN_SPEED;
	float TILT_SPEED;
};

// See initMouseInfo for default vals
struct MouseInfo {
	float lastX;
	float lastY;
	float sensitivity;
	bool firstMouse;

	float lastModeSwitchTime;
	float modeSwitchCooldown;
};

struct LightSource {
	glm::vec3 pos;
	glm::vec3 color;
};

struct Scene {
	MeshInstance* meshInstances[__MAX_MODELS__];
	int meshInstanceCount;

	Camera camera;
	LightSource lightSource;
	MouseInfo mouse;
};

struct ResourcePool {
	Mesh* meshes[__MAX_MESHES__];
	int meshCount;

	// @TODO: map string name --> resourceID
	//ResourceMap meshMap;

	~ResourcePool() {
		for (int i = 0; i < meshCount; i++) {
			free(meshes[i]);
		}
	}
};