#ifndef __my_structs__

// early design decision to statically allocate 
#define __MAX_MESHES__ 64
#define __MAX_MODELS__ 65536
#define __MAX_TEXTURES__ 64
#define __MAX_SHADERS__ 64

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

	bool has_convex_hull;
	int hullId; // -1 if we are a hull, otherwise point into global mesh pool

	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;
};

// frees the mesh and mesh contents
void free_mesh(Mesh* mesh) {
	free(mesh->vertices);
	free(mesh->faces);
	free(mesh);
}

// Texture is a resource containing metadata
// in the future it might have more data
struct Texture {
	unsigned int textureID;
};

// Shader wrapper
struct Shader {
	unsigned int shaderID;

	// @TODO?: hashmap : string (uniform name) -> int (loc)
	// not going to be many uniforms, maybe 10-20 max. maybe it's faster to just O(n) lookup.
	
	// until (if) we implement hashmap, do a lookup on uniformNames, use index in uniformLoc
	char** uniformNames;
	unsigned int* uniformLoc;
	int uniformCount;
};

void free_shader(Shader* shader) {
	free(shader->uniformNames);
	free(shader->uniformLoc);
	shader->uniformCount = 0;
}

// @TODO: get rid of this if this is bad
unsigned int get_shader_loc(const Shader* shader, const char* name) {
	int index = -1;
	for (int i = 0; i < shader->uniformCount; i++) {
		if (strcmp(name, shader->uniformNames[i]) == 0) {
			index = i;
			break;
		}
	}
	return index;
}

// Skybox is a cubemap loaded from 6 individual texture images
struct Skybox {
	unsigned int cubemapID;
	float vertices[108];

	unsigned int VAO;
	unsigned int VBO;
};

// MeshInstance is a world model which references a mesh and a texture
struct MeshInstance {
	unsigned int globalMeshId;
	unsigned int globalTextureId;
	float pos[3];
	float scale[3];
	float rotation[4]; // orientation as a quaternion rotation on (1, 0, 0, 0)
	float color[3];

	// convex hull
	float hullColor[3];

	// physics
	float velocity[3];
	
	// bitfield
	// physics = enabled
	// physics & 1 = is_falling
	// physics & 2 = is_collidable
	unsigned int physics;
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

	// these are here so we can allocate memory once and reuse it in the render loop
	// column major order because OpenGL uses it
	// col major means mat[0:4] is col0, mat[4:8] is col1, mat[8:12] is col2, mat[12:16] is col3
	float model[16];
	float view[16];
	float proj[16];
	float projview[16]; // precompute for shader
	float normal[9]; // normal matrix instead of model matrix for normal vectors to keep the transformation linear

	int windowWidth;
	int windowHeight; 

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

	Shader* shaders[__MAX_SHADERS__];
	int shaderCount;
	
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
	for (int i = 0; i < pool->textureCount; i++) {
		free(pool->textures[i]);
	}
	for (int i = 0; i < pool->shaderCount; i++) {
		free_shader(pool->shaders[i]);
	}
}

// wrapper for handling FPS metrics
struct Metrics {
	float lastTime;
	float currTime;
	float deltaTime;
	float fpsWindowTimeStart;
	float fpsWindowTimeEnd;
	float heartBeat;
	unsigned int frameCount;
	int FRAMES_TO_COUNT;
	float fps;
};

#define __my_structs__
#endif