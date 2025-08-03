#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define __MAX_MESHES__ 2048
#define __MAX_MODELS__ 65536

// vertex object is a collection of 3d point in space, 3d normal vector, and 2d texture coord
// more than just a point in space, it is an object that encodes the corner of a polygon
struct Vertex {
	float pos[3];
	float normal[3];
	float texture[2];
};

// face encodes a triangle, which references 3 points in space as corners of the triangle
// the meaning of the ID is left to the mesh which owns the face
struct Face {
	unsigned int vertexId[3];
};

// Mesh is a resource containing a list of vertices, a list of faces and some metadata
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

// Texture is a resource containing metadata
struct Texture {
	GLuint textureID;
};

// MeshInstance is a world model which references a mesh and a texture
// TODO: store world data here and compute model on the fly
struct MeshInstance {
	Mesh* mesh;
	Texture* texture;
	glm::mat4 model;
	glm::vec3 color;
};

// Camera encodes the view
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

// MouseInfo stores some state related to the mouse input
// See initMouseInfo for default vals
struct MouseInfo {
	float lastX;
	float lastY;
	float sensitivity;
	bool firstMouse;

	float lastModeSwitchTime;
	float modeSwitchCooldown;
};

// lightsource is an emitter of light for dynamic lighting
struct LightSource {
	glm::vec3 pos;
	glm::vec3 color;
};

// Scene is meant to be a global scope singleton containing world state
struct Scene {
	MeshInstance* meshInstances[__MAX_MODELS__];
	int meshInstanceCount;

	Camera camera;
	LightSource lightSource;
	MouseInfo mouse;

	~Scene() {
		for (int i = 0; i < meshInstanceCount; i++) {
			free(meshInstances[i]);
		}
	}
};

// ResourcePool is meant to be a store of assets with IDs for MeshInstances to reference
struct ResourcePool {
	Mesh* meshes[__MAX_MESHES__];
	int meshCount;
	
	// @TODO: add textures here? Or have 2 resourcePools?
	// ResourcePool globalMeshPool;
	// ResourcePool globalTexturePool;
	// VS
	// ResourcePool { Mesh* meshes[]; Texture* textures[]; }

	// @TODO?: map string name --> resourceID
	// Why should this pool own a map to its resources?
	// The index is already an ID for the array. 
	//ResourceMap meshMap;

	~ResourcePool() {
		for (int i = 0; i < meshCount; i++) {
			free(meshes[i]);
		}
	}
};