#ifndef __my_structs__

#define __MAX_MESHES__ 2048
#define __MAX_MODELS__ 65536
#define __MAX_TEXTURES__ 65536

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
	Vertex* vertices;
	Face* faces;

	size_t num_vertices;
	size_t num_faces;

	bool has_normals;

	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;
};

void init_mesh(Mesh* mesh) {
	mesh->vertices = nullptr;
	mesh->faces = nullptr;
	mesh->num_vertices = 0;
	mesh->num_faces = 0;
	mesh->has_normals = false;
}

void free_mesh(Mesh* mesh) {
	free(mesh->vertices);
	free(mesh->faces);
	free(mesh);
}

// Texture is a resource containing metadata
struct Texture {
	unsigned int textureID;
};

// Skybox is a cubemap loaded from 6 individual texture images
struct Skybox {
	unsigned int cubemapID;
	float vertices[108];

	unsigned int VAO;
	unsigned int VBO;
};

// MeshInstance is a world model which references a mesh and a texture
struct MeshInstance {
	Mesh* mesh; // todo: make this a ptr to mesh's ID with global pool?
	Texture* texture;
	float pos[3];
	float scale[3];
	float rotation[4]; // orientation as a quaternion rotation on (1, 0, 0, 0)
	float color[3];
};

// Camera encodes the view
// yaw should be initialized to -90.0f
struct Camera {
	float pos[3];
	float front[3];
	float up[3];

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
	bool canModeSwitch;
};

// lightsource is an emitter of light for dynamic lighting
struct LightSource {
	float pos[3];
	float color[3];
};

// Scene is meant to be a global scope singleton containing world state
struct Scene {
	MeshInstance* meshInstances[__MAX_MODELS__];
	int meshInstanceCount;

	Camera camera;
	LightSource lightSource;
	Skybox skybox;
	MouseInfo mouse;
};

void free_scene(Scene* scene) {
	for (int i = 0; i < scene->meshInstanceCount; i++) {
		free(scene->meshInstances[i]);
	}
}

// ResourcePool is meant to be a store of assets with IDs for MeshInstances to reference
struct ResourcePool {
	Mesh* meshes[__MAX_MESHES__];
	int meshCount;

	Texture* textures[__MAX_TEXTURES__];
	int textureCount;
	
	// @TODO: add textures here? Or have 2 resourcePools?
	// ResourcePool globalMeshPool;
	// ResourcePool globalTexturePool;
	// VS
	// ResourcePool { Mesh* meshes[]; Texture* textures[]; }

	// @TODO?: map string name --> resourceID
	// Why should this pool own a map to its resources?
	// The index is already an ID for the array. 
	//ResourceMap meshMap;
};

void free_resource_pool(ResourcePool* pool) {
	for (int i = 0; i < pool->meshCount; i++) {
		free_mesh(pool->meshes[i]);
	}
}

#define __my_structs__
#endif