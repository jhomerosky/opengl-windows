#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "math_utils.hpp"
#include "gl_profile.hpp"

// Global Macros
// @TODO: come up with a system to handle physics constants
#define G_ACCEL 9.8f
// early design decision to statically allocate
#define __MAX_MESHES__ 64
#define __MAX_MODELS__ 65536
#define __MAX_TEXTURES__ 64
#define __MAX_SHADERS__ 64

// ===== FORWARD DECLATIONS =====
// structs
struct Vertex;
struct Face;
struct Mesh;
struct Texture;
struct Shader;
struct Skybox;
struct MeshInstance;
struct Camera;
struct MouseInfo;
struct LightSource;
struct Scene;
struct ResourcePool;
struct Metrics;

// custom initializers
// @TODO: sort the initializers here

// custom deallocators
void free_mesh(Mesh* mesh);
void free_shader(Shader* shader);
void free_scene(Scene* scene);
void free_resource_pool(ResourcePool* pool);

// callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);

// hashes
static inline unsigned long long float3Hash(const float in[3], const int decimalFilter);
static inline uint64_t hash_pair(const uint32_t a, const uint32_t b);

// geometry
int compute_vnormal_smooth(Mesh* mesh);
int deduplicate_mesh_vertices(Mesh* mesh, const int tol);
int realloc_mesh_with_face_vertices(Mesh* mesh);
int compute_vnormal_flat(Mesh* mesh);
Mesh* makeConvexHull(Mesh* mesh);
unsigned int support(Mesh* mesg, float dir[3]);
bool GJK_intersect(MeshInstance* objectA, MeshInstance* objectB);

// shader handling
GLuint compileShader(GLenum type, const char* source);
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);

// load from file
int malloc_mesh_fields_from_obj_file(const char* filename, Mesh* mesh);
unsigned int loadCubemap(const char** textureFiles);
char* mallocTextFromFile(const char* filename);
GLuint loadShader(char* vertexShaderSource, char* fragmentShaderSource);

// register
int addMeshInstanceToGlobalScene(MeshInstance* meshInstance);
int addMeshToGlobalPool(Mesh* mesh);
int addTextureToGlobalPool(Texture* texture);
int addShaderToGlobalPool(Shader* shader);
void uploadMeshBuffers(const Mesh *mesh);

// ????
void swapCursorInputMode(GLFWwindow* window);
void rotateCamera(GLFWwindow* window, float yaw, float pitch);

// main loop
void processInput(GLFWwindow* window, float deltaTime);
void updateTime(Metrics* metrics);
void updateScene(GLFWwindow* window, float deltaTime);
void renderScene(GLFWwindow* window);

// set defaults
void setDefaultMeshInstance(MeshInstance* meshInstance, const int resourceId);
void setDefaultScene();

// init
void initOpenGL();
void initCamera(Camera& camera);
void initLightSource(LightSource& lightSource);
void initMouseInfo(MouseInfo& mouse);
void initMetrics(Metrics* metrics);
int initMesh(Mesh* mesh);
void initShaders();
void initSkybox();
int initGlobalResourcePoolMallocMeshAndMeshFields();
void initGlobalScene();

// algorithm handlers
void executeConvexHulls();
void executeCollisionCheck();

// print
void printGlobalResourcePool();
// ===== END FORWARD DECLARATIONS ====

// ===== STRUCT DEFINITIONS =====
#pragma region STRUCTDEF // this allows the region to be collapsible in vscode
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

	char* name; // string name to recognize the hull

	unsigned int VAO;
	unsigned int VBO;
	unsigned int EBO;
};

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
#pragma endregion STRUCTDEF 
// ===== END STRUCT DEFINITIONS =====

// ===== GLOBAL VARS =====
ResourcePool global_resource_pool;
Scene global_scene;
// ===== END GLOBAL VARS =====

// ===== CUSTOM DEALLOCATORS =====
// frees the mesh and mesh contents
void free_mesh(Mesh* mesh) {
	free(mesh->name);
	free(mesh->vertices);
	free(mesh->faces);
	free(mesh);
}

void free_shader(Shader* shader) {
	free(shader->uniformNames);
	free(shader->uniformLoc);
	free(shader);
}

void free_scene(Scene* scene) {
	for (int i = 0; i < scene->meshInstanceCount; i++) {
		free(scene->meshInstances[i]);
	}
}

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
// ===== END CUSTOM DEALLOCATORS =====

// ===== CALLBACK FUNCTIONS =====
// function to execute when the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

// function to execute when mouse input is received
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	MouseInfo *mouse = &(global_scene.mouse);
	Camera *camera = &(global_scene.camera);
	if (mouse->firstMouse) {
		mouse->lastX = xpos;
		mouse->lastY = ypos;
		mouse->firstMouse = false;
	}

	float xoffset = xpos - mouse->lastX;
	float yoffset = mouse->lastY - ypos;
	mouse->lastX = xpos;
	mouse->lastY = ypos;

	xoffset *= mouse->sensitivity;
	yoffset *= mouse->sensitivity;

	camera->yaw += xoffset;
	camera->pitch += yoffset;

	if (camera->pitch > 89.0f)
		camera->pitch = 89.0f;
	else if (camera->pitch < -89.0f)
		camera->pitch = -89.0f;

	float direction[3];
	camera->front[0] = cosf(radiansf(camera->yaw)) * cosf(radiansf(camera->pitch));
	camera->front[1] = sinf(radiansf(camera->pitch));
	camera->front[2] = sinf(radiansf(camera->yaw)) * cosf(radiansf(camera->pitch));
	normalize3f_inplace(camera->front);
}
// ===== END CALLBACK FUNCTIONS =====


// ===== HASH FUNCTIONS =====
// hashing strategy: hash = x * prime1 ^ y * prime2 ^ z * prime3
// (x,y,z) = (int)((x,y,z) * 1e5)
static inline unsigned long long float3Hash(const float in[3], const int decimalFilter) {

	// 1.1234567 * 1e5 = 112345.67 --> 112345
	// 1.1234587 * 1e5 = 112345.87 --> 112345
	// 1.1234587 - 1.1234567 = 0.0000020 < 0.00001 = 1e-5
	const long long primes[3] = {19349669, 83492791, 73856093};
	//const int primes[3] = {113, 569, 877};
	const long long vecints[3] = {
		(long long)(in[0] * decimalFilter),
		(long long)(in[1] * decimalFilter),
		(long long)(in[2] * decimalFilter)
	};
	return (primes[0] * vecints[0]) ^ (primes[1] * vecints[1]) ^ (primes[2] * vecints[2]);
}

// hash pair of uint32_t into one uint64_t
static inline uint64_t hash_pair(const uint32_t a, const uint32_t b) {
	return ((uint64_t)a << 32) | (uint64_t)b;
}
// ===== END HASH FUNCTIONS =====

// ===== GEOMETRY FUNCTIONS =====
// @NOTE: assumes vertices are deduplicated
// compute each face's normal vector, add vnormal to each component vector, normalize all vectors
int compute_vnormal_smooth(Mesh* mesh) {
	// @TODO: can we parallelize computing normals and do addition later?
	for (int i = 0; i < mesh->num_faces; i++) {
		float e1[3];
		float e2[3];
		float res[3];
		// vertices
		Vertex *v0 = &(mesh->vertices[mesh->faces[i].vertexId[0]]);
		Vertex *v1 = &(mesh->vertices[mesh->faces[i].vertexId[1]]);
		Vertex *v2 = &(mesh->vertices[mesh->faces[i].vertexId[2]]);
		
		// edges of triangle
		sub3f(e1, v1->pos, v0->pos);
		sub3f(e2, v2->pos, v1->pos);

		// get normal of triangle
		cross3f(res, e1, e2);

		// add this face's vnormal to each vertex in the face
		// these should all face relatively same direction so we don't norm a 0 vector
		add3f(v0->normal, v0->normal, res);
		add3f(v1->normal, v1->normal, res);
		add3f(v2->normal, v2->normal, res);
	}
	// normalize every vertex's vnormal;
	// @NOTE: Benchmark shows omp threads speeds this up but simd does not
	#pragma omp parallel for
	for (int i = 0; i < mesh->num_vertices; i++) {
		normalize3f_inplace(mesh->vertices[i].normal);
	}
	mesh->has_normals = true;
	return 0;
}

// deduplicate a mesh's vertex list on position with tolerance of 1e-(tol)
// @TODO: some vertices are already used by many different faces and this will still check those anyways
//        which results in a very large number of unnecessary checks. can we eliminate that behavior?
// @NOTE: deduplication helps with smooth shading, but in the end especially with texture uv coords we will still need
//        to use face-vertices on gpu. Time spent improving this is probably not productive.
// returns -1 on error or num of duplicates dropped on success
int deduplicate_mesh_vertices(Mesh* mesh, const int tol) {
	struct HashNode {
		unsigned int data; // map vertex.pos -> index in the new vertex array (vertex_list[data].pos is the key to match)
		HashNode* next;
	};

	int decimalFilter = 1;
	for (int i = 0; i < tol; i++) decimalFilter *= 10;
	const float eps = 1.0/(float)decimalFilter;
	
	// allocate memory for map and new vertex list
	size_t map_size = mesh->num_faces/2;
	HashNode** map = (HashNode**)calloc(map_size, sizeof(HashNode*));
	Vertex *new_vertex_list = (Vertex*)malloc(sizeof(Vertex) * 3 * mesh->num_faces); // upper bound size, resize later
	if (new_vertex_list == nullptr) { fprintf(stderr, "Failed to malloc the new vertex list in deduplicate_mesh_vertices\n"); free(map); return -1; }
	unsigned int vertex_index = 0;

	for (int i = 0; i < mesh->num_faces; i++) {
		Face* temp = &(mesh->faces[i]);
		for (int j = 0; j < 3; j++) {
			// hash each vertex
			Vertex* v = &(mesh->vertices[temp->vertexId[j]]);
			unsigned int hash = float3Hash(v->pos, decimalFilter);
			HashNode** ptr = &map[hash % map_size];
			
			// check if vertex is already in map
			bool found = false;
			while (*ptr != NULL) {
				if (equals3f(new_vertex_list[(*ptr)->data].pos, v->pos, eps)) {
					found = true;
					break;
				}
				ptr = &((*ptr)->next);
			}

			// if vertex is not found in the map then add it
			if (!found) {
				*ptr = (HashNode*)calloc(1, sizeof(HashNode));
				(*ptr)->data = vertex_index++;
				set3fv(new_vertex_list[(*ptr)->data].pos, v->pos);
			}
			temp->vertexId[j] = (*ptr)->data;
		}
	}

	int dupe_count = mesh->num_vertices - vertex_index;

	// free the map's contents
	for (int i = 0; i < map_size; i++) {
		HashNode* ptr = map[i];
		HashNode* temp;
		while (ptr != NULL) {
			temp = ptr->next;
			free(ptr);
			ptr = temp;
		}
	}
	free(map);
	free(mesh->vertices);
	mesh->vertices = (Vertex*)realloc(new_vertex_list, vertex_index*sizeof(Vertex));
	mesh->num_vertices = vertex_index;

	return dupe_count;
}

// Mutates the size of the mesh to 3*(num_faces) vertices.
// Face-vertices: a vertex contains { pos, vn, uv } not just position. So consider a face-vertex as the unique combination of vertex and the face that uses it.
int realloc_mesh_with_face_vertices(Mesh* mesh) {
	Vertex *new_vertex_list = (Vertex*)malloc(sizeof(Vertex) * 3 * mesh->num_faces);
	if (new_vertex_list == nullptr) { fprintf(stderr, "Failed to malloc the new vertex list in realloc_mesh_with_face_vertices\n"); return -1; }

	// NOTE: benchmark demonstrated >2x speedup multithreading speedup
	#pragma omp parallel for
	for (int i = 0; i < mesh->num_faces; i++) {
		// add to new list; note no overlap between threads
		new_vertex_list[i*3]     = mesh->vertices[mesh->faces[i].vertexId[0]];
		new_vertex_list[i*3 + 1] = mesh->vertices[mesh->faces[i].vertexId[1]];
		new_vertex_list[i*3 + 2] = mesh->vertices[mesh->faces[i].vertexId[2]];

		// point faces to new vertex index
		mesh->faces[i].vertexId[0] = i*3;
		mesh->faces[i].vertexId[1] = i*3 + 1;
		mesh->faces[i].vertexId[2] = i*3 + 2;
	}
	free(mesh->vertices);
	mesh->vertices = new_vertex_list;
	mesh->num_vertices = 3 * mesh->num_faces;

	return 0;
}

// Compute vnormals for flat shading. Each vertex of a face uses the face's normal.
// @NOTE: assuming CCW face orientation, faces are triangles, vertex list is face-vertex
int compute_vnormal_flat(Mesh* mesh) {
	// for each face:
	// e1 = v2 - v1, e2 = v3 - v2
	// fnormal = normalize(cross(e1, e2))
	// assign this normal to all vertices in this face

	#pragma omp parallel for
	for (int i = 0; i < mesh->num_faces; i++) {
		// private memory
		float e1[3];
		float e2[3];
		float res[3];
		// vertices
		Vertex *v0 = &mesh->vertices[mesh->faces[i].vertexId[0]];
		Vertex *v1 = &mesh->vertices[mesh->faces[i].vertexId[1]];
		Vertex *v2 = &mesh->vertices[mesh->faces[i].vertexId[2]];

		// edges of triangle
		sub3f(e1, v1->pos, v0->pos);
		sub3f(e2, v2->pos, v1->pos);

		// cross product will not be near 0 if triangle is not degenerate
		cross3f(res, e1, e2);
		normalize3f_inplace(res);

		// write normal to vertex
		set3fv(v0->normal, res);
		set3fv(v1->normal, res);
		set3fv(v2->normal, res);
	}
	mesh->has_normals = true;

	return 0;
}

// input: mesh
// output: mesh representing the convex hull
// quickhull algorithm
Mesh* makeConvexHull(Mesh* mesh) {
	const float __CONVEX_HULL_DEDUP_EPS__     = 1e-6f;
	const int   __CONVEX_HULL_DEDUP_INV_EPS__ = 1e6;
	// smaller vertex for algorithm
	struct Point {
		float pos[3];
		unsigned int vertexIndex; // index of this point into the output vertex list
		bool assigned; // if vertexIndex is assigned yet
	};

	// Dynamic array of uint
	struct IndexList {
		unsigned int* items;
		size_t length;
		size_t capacity;
	};
	
	// the literature calls these facets but they are just faces
	struct Facet {
		unsigned int points[3]; // index in a list of points
		float normal[3]; // normal facing outward
		float offset; // plane = { x : n dot x = d }; d is the offset

		bool isNew; // this facet was newly created in the main loop
		bool isVisible; // visible facets are marked for deletion
		bool isActive; // instead of deleting, just mark as inactive
		IndexList extPointListDA; // dynamic list of exterior point indices
	};

	// This map is used to lookup the unique active facet containing the oriented ridge.
	// A ridge is an edge between two points. Oriented means we are ordering them such that A(first) and B(second) give A->B in the CCW orientation of the facet using it.
	// The key is the hash value for the two points A,B and represents the ridge.
	// The val is the index in facetList of the unique active facet containing this oriented ridge.
	struct RidgeMapNode {
		uint64_t key; // we use the hash as the key since the hash function is a bijection
		uint32_t val;
		RidgeMapNode* next;
	};

	// NOTE: Using macro helpers so I can avoid lifting struct definitions out of this function.
	//       All of these macros are undefined at the end of this function

	// Initialize a new facet on the facetList. Does NOT check out of bounds.
	#define new_facet(fList, fListIndex, indexA, indexB, indexC, pInterior) do {\
		float interiorToA[3];\
		Facet* currentFacet = &fList[(fListIndex)];\
		set3u(currentFacet->points, (indexA), (indexB), (indexC));\
		set_triangle_normal(currentFacet->normal, pointList[(indexA)].pos, pointList[(indexB)].pos, pointList[(indexC)].pos);\
		/* if normal . (A - centroid) < 0, then our normal points inward so reorient the triangle */\
		sub3f(interiorToA, pointList[(indexA)].pos, (pInterior).pos);\
		if (dot3f(currentFacet->normal, interiorToA) < 0.0f) {\
			negate3f_inplace(currentFacet->normal);\
			currentFacet->points[1] = (indexC);\
			currentFacet->points[2] = (indexB);\
		}\
		currentFacet->offset = dot3f(currentFacet->normal, pointList[(indexA)].pos);\
		currentFacet->isNew = true;\
		currentFacet->isVisible = false;\
		currentFacet->isActive = false;\
		currentFacet->extPointListDA = { NULL, 0, 0 };\
		facetListSize++;\
	} while (0)

	// insert into the ridge map with calloc
	#define insert_ridge_map(map, map_size, new_key, new_val) do {\
		RidgeMapNode** ptr = &(map[(new_key) % (map_size)]);\
		while (*ptr != NULL && (*ptr)->key != (new_key)) {\
			ptr = &((*ptr)->next);\
		}\
		if (*ptr == NULL) {\
			*ptr = (RidgeMapNode*)calloc(1, sizeof(RidgeMapNode));\
		}\
		(*ptr)->key = (new_key);\
		(*ptr)->val = (new_val);\
	} while (0)

	// append to the IndexList dynamic array with realloc
	#define da_append(list, item) do {\
		if (list.length >= list.capacity) {\
			list.capacity = list.capacity == 0 ? 256 : list.capacity * 2;\
			list.items = (decltype(list.items))realloc(list.items, list.capacity * sizeof(*list.items));\
		}\
		list.items[list.length++] = item;\
	} while (0)

	// convexHull guaranteed to have size <= current size so we prealloc here and resize at the end
	Point* pointList = (Point*)malloc(sizeof(Point) * mesh->num_vertices);
	size_t pointListSize = 0;
	
	// ===== DEDUPLICATION PASS =====
	// benchmark demonstrated significant speedup using map over O(n^2).
	// For guy.obj (24461 v): 17ms vs 700ms; for hp_portrait.obj (819352 v): 500ms vs force killed after 1 minute  
	{
		struct VertexHashSetNode {
			Vertex* vertex;
			VertexHashSetNode* next;
		};

		// use a hash set to dedup vertices
		size_t set_size = mesh->num_vertices;
		VertexHashSetNode** set = (VertexHashSetNode**)calloc(set_size, sizeof(VertexHashSetNode*));
		if (set == nullptr) { fprintf(stderr, "Failed to malloc hash set in makeConvexHull\n"); free(pointList); return nullptr; }
		for (int i = 0; i < mesh->num_vertices; i++) {
			Vertex* v = &(mesh->vertices[i]);
			unsigned int hash = float3Hash(v->pos, __CONVEX_HULL_DEDUP_INV_EPS__);

			// check if vertex is in set
			VertexHashSetNode** ptr = &set[hash % set_size];
			bool found = false;
			while (*ptr != NULL) {
				if (equals3f((*ptr)->vertex->pos, v->pos, __CONVEX_HULL_DEDUP_EPS__)) {
					found = true;
					break;
				}
				ptr = &((*ptr)->next);
			}

			// if vertex is not found in the set then add it
			if (!found) {
				*ptr = (VertexHashSetNode*)calloc(1, sizeof(VertexHashSetNode));
				(*ptr)->vertex = v;
				set3fv(pointList[pointListSize].pos, v->pos);
				pointList[pointListSize].assigned = false;
				pointListSize++;
			}
		}
		for (int i = 0; i < set_size; i++) {
			VertexHashSetNode* ptr = set[i];
			VertexHashSetNode* temp;
			while (ptr != NULL) {
				temp = ptr->next;
				free(ptr);
				ptr = temp;
			}
		}
		free(set);
		pointList = (Point*)realloc(pointList, pointListSize * sizeof(Point));
	}

	// printf("(CONVEX HULL): vertices deduplicated: %d --> %d \n", mesh->num_vertices, pointListSize);
	// ===== END DEDUP PASS =====

	// ===== BEGIN QUICKHULL =====
	// @TODO: benchmark quickhull vs the rest of makeConvexHull
	// list of facets used in convex hull, size may exceed cap and trigger realloc
	size_t facetListSize = 0;
	size_t facetListCap = maxi(4, mesh->num_faces);
	Facet* facetList = (Facet*)malloc(sizeof(Facet) * facetListCap);
	if (!facetList) { fprintf(stderr, "Failed to malloc facetList in makeConvexHull\n"); free(pointList); return NULL; }

	// map point index A, B to the unique facet with the directed edge A->B
	// will not realloc
	size_t ridgeMapSize = mesh->num_faces;
	RidgeMapNode** ridgeMap = (RidgeMapNode**)calloc(ridgeMapSize, sizeof(RidgeMapNode*));
	if (!ridgeMap) { fprintf(stderr, "Failed to malloc ridgeMap in makeConvexHull\n"); free(pointList); free(facetList); return NULL; }

	// initial simplex:
	// p0, p1 are min/max of one dimension
	// p2 is argmax distance from p1p2 segment
	// p3 is argmax distance from p1p2p3 plane
	unsigned int p0, p1, p2, p3;

	// step 1: take min/max along X
	{
		unsigned int minX = 0;
		unsigned int maxX = 0;
		for (int i = 1; i < pointListSize; i++) {
			if (pointList[i].pos[0] < pointList[minX].pos[0]) minX = i;
			if (pointList[i].pos[0] > pointList[maxX].pos[0]) maxX = i;
		}
		p0 = minX;
		p1 = maxX;
	}

	// step 2: find point farthest in perpendicular distance from segment (p0p1)
	// distance(p, p0p1) = ||(p - p0) x (p1 - p0)|| / || p1 - p0 ||
	//   but we can just check || (p - p0) x (p1 - p0) ||^2
	p2 = 0;
	{
		float maxDist = 0.0f;
		float distance;
		float newSegment[3]; // p - p0
		float segment[3]; // p1 - p0
		float cross_temp[3];
		sub3f(segment, pointList[p1].pos, pointList[p0].pos);
		for (int i = 1; i < pointListSize; i++) {
			if (i == p0 || i == p1) continue; // skip these points
			sub3f(newSegment, pointList[i].pos, pointList[p0].pos);
			cross3f(cross_temp, newSegment, segment); // (p - p0) x (p1 - p0)
			distance = dot3f(cross_temp, cross_temp);
			if (distance > maxDist) {
				p2 = i;
				maxDist = distance;
			}
		}
	}
	
	// step 3: find point farthest in perpendicular distance from normal
	// normal(p0p1p2) = normalize((p2 - p0) x (p1 - p0))
	// distance(p, p0p1p2) = abs( (p2 - p0) . (normal) ) 
	{
		float maxDist = 0.0f;
		float distance;
		float segP0P2[3];
		float segP0P1[3];
		float normal[3];
		float segP0Pi[3];
		sub3f(segP0P2, pointList[p2].pos, pointList[p0].pos);
		sub3f(segP0P1, pointList[p1].pos, pointList[p0].pos);
		cross3f(normal, segP0P2, segP0P1);
		normalize3f_inplace(normal); // this is a normal vector to the p1p2p3 triangle
		p3 = 0;
		for (int i = 0; i < pointListSize; i++) {
			if (i == p0 || i == p1 || i == p2) continue;
			sub3f(segP0Pi, pointList[i].pos, pointList[p0].pos);
			distance = fabsf(dot3f(segP0Pi, normal));
			if (distance > maxDist) {
				p3 = i;
				maxDist = distance;
			}
		}
	}

	// Compute centroid to orient direction of normals away from center
	Point centroid;
	set3fv(centroid.pos, pointList[p0].pos);
	add3f(centroid.pos, centroid.pos, pointList[p1].pos);
	add3f(centroid.pos, centroid.pos, pointList[p2].pos);
	add3f(centroid.pos, centroid.pos, pointList[p3].pos);
	mult3f(centroid.pos, centroid.pos, 0.25f);

	// set initial faces for tetrahedron
	// Note: Use macro function because we are defining the structs inside this function.
	{
		new_facet(facetList, facetListSize, p0, p1, p2, centroid);
		new_facet(facetList, facetListSize, p0, p1, p3, centroid);
		new_facet(facetList, facetListSize, p0, p2, p3, centroid);
		new_facet(facetList, facetListSize, p1, p2, p3, centroid);

		// insert ridges into ridge map
		for (int i = 0; i < 4; i++) {
			uint64_t hash[3];
			hash[0] = hash_pair(facetList[i].points[0], facetList[i].points[1]);
			hash[1] = hash_pair(facetList[i].points[1], facetList[i].points[2]);
			hash[2] = hash_pair(facetList[i].points[2], facetList[i].points[0]);
			insert_ridge_map(ridgeMap, ridgeMapSize, hash[0], i);
			insert_ridge_map(ridgeMap, ridgeMapSize, hash[1], i);
			insert_ridge_map(ridgeMap, ridgeMapSize, hash[2], i);

			facetList[i].isNew = false;
			facetList[i].isActive = true;
		}
	}

	// Assign every remaining point to the facet (of the original 4) which it is farthest outside of, if it is outside
	{
		float distance;
		float maxDist;
		Facet* assignedFacet;
		for (size_t i = 0; i < pointListSize; i++) {
			if (i == p0 || i == p1 || i == p2 || i == p3) continue;
			maxDist = 0.0f;
			assignedFacet = nullptr;
			for (size_t j = 0; j < facetListSize; j++) {
				distance = dot3f(pointList[i].pos, facetList[j].normal) - facetList[j].offset;
				if (distance > maxDist) {
					maxDist = distance;
					assignedFacet = &facetList[j];
				}
			}
			// assign the point
			if (assignedFacet != nullptr) {
				da_append(assignedFacet->extPointListDA, i);
			}
		}
	}

	// Main loop:
	//  1. Take a facet that is both active and has exterior points
	//  2. Find the point p which is farthest outside that facet (max perp. distance)
	//  3. Find all facets visible to p and set as inactive
	//  4. Find all horizon edges (with p1, p2) and form a new facet (p, p1, p2) with corrected orientation (increases facetListSize)
	//  5. Reassign all exterior points belonging to the inactive facets among the newly created facets
	//  6. After each iteration, a processed facet will be inactive or have no outside points
	for (size_t i = 0; i < facetListSize; i++) {
		if (!facetList[i].isActive || facetList[i].extPointListDA.length == 0) continue;

		// get farthest point
		unsigned int p = facetList[i].extPointListDA.items[0];
		float maxDist = 0.0f;
		float dist;
		for (size_t j = 0; j < facetList[i].extPointListDA.length; j++) {
			dist = dot3f(pointList[facetList[i].extPointListDA.items[j]].pos, facetList[i].normal) - facetList[i].offset;
			if (dist > maxDist) {
				maxDist = dist;
				p = facetList[i].extPointListDA.items[j];
			}
		}

		// visibility check
		for (size_t j = 0; j < facetListSize; j++) {
			if (!facetList[j].isActive) continue;
			if (dot3f(pointList[p].pos, facetList[j].normal) - facetList[j].offset > 0.0f) {
				facetList[j].isVisible = true;
			}
		}

		// horizon edge check and create new facets
		// For each visible facet:
		//   For each edge of facet:
		//     If adjacent facet across ridge is not visible, then this is a horizon edge
		//     NOTE: orientation of ridge reverses; facet1 with (A->B) ==> facet2 with (B->A)
		// For all horizon edges (points A, B): Form new facet (A, B, P) with corrected orientation
		IndexList newFacetIndices = {NULL, 0, 0};
		for (size_t j = 0; j < facetListSize; j++) {
			if (!facetList[j].isActive || !facetList[j].isVisible || facetList[j].isNew) continue;
			for (int k = 0; k < 3; k++) {
				unsigned int A = facetList[j].points[k];
				unsigned int B = facetList[j].points[(k + 1) % 3];
				
				// check B->A for the adjacent visibility
				uint64_t hash = hash_pair(B, A);
				RidgeMapNode** ptr = &(ridgeMap[hash % ridgeMapSize]);
				while (*ptr != NULL && (*ptr)->key != hash) {
					ptr = &((*ptr)->next);
				}
				if (*ptr == NULL) { printf("(CONVEX HULL): PANIC; RIDGE NOT FOUND: facetList[%zu].points[%u//%u]\n", j, B, A); continue; }
				if (!facetList[(*ptr)->val].isVisible) {
					da_append(newFacetIndices, facetListSize);
					if (facetListSize >= facetListCap) {
						facetListCap *= 2;
						facetList = (Facet*)realloc(facetList, facetListCap * sizeof(Facet));
					}
					new_facet(facetList, facetListSize, A, B, p, centroid);
				}
			}
		}

		// reassign exterior points to new facets
		// @TODO: benchmark this loop
		for (size_t j = 0; j < facetListSize; j++) {
			if (facetList[j].isActive && facetList[j].isVisible) {
				// redistribute points
				float distance;
				float maxDist;
				Facet* assignedFacet;
				for (size_t k = 0; k < facetList[j].extPointListDA.length; k++) {
					size_t test_point_index = facetList[j].extPointListDA.items[k];
					if (test_point_index == p) continue; // dont reassign active point
					// reassign to new facets
					maxDist = 0.0f;
					assignedFacet = nullptr;
					for (size_t w = 0; w < newFacetIndices.length; w++) {
						size_t test_facet_index = newFacetIndices.items[w];
						distance = dot3f(pointList[test_point_index].pos, facetList[test_facet_index].normal) - facetList[test_facet_index].offset;
						if (distance > maxDist) {
							maxDist = distance;
							assignedFacet = &facetList[test_facet_index];
						}
					}
					if (assignedFacet != nullptr) {
						da_append(assignedFacet->extPointListDA, test_point_index);
					}
				}
				// destroy the visible facet
				free(facetList[j].extPointListDA.items);
				facetList[j].extPointListDA = {NULL, 0, 0};
				facetList[j].isActive = false;
				facetList[j].isVisible = false;
			}
		}

		// update ridge map
		for (size_t j = 0; j < facetListSize; j++) {
			if (!facetList[j].isNew) continue;
			uint64_t hash[3];
			hash[0] = hash_pair(facetList[j].points[0], facetList[j].points[1]);
			hash[1] = hash_pair(facetList[j].points[1], facetList[j].points[2]);
			hash[2] = hash_pair(facetList[j].points[2], facetList[j].points[0]);
			insert_ridge_map(ridgeMap, ridgeMapSize, hash[0], j);
			insert_ridge_map(ridgeMap, ridgeMapSize, hash[1], j);
			insert_ridge_map(ridgeMap, ridgeMapSize, hash[2], j);
			facetList[j].isNew = false;
			facetList[j].isActive = true;
		}
		free(newFacetIndices.items);
		newFacetIndices = {NULL, 0, 0};
	}
	// ===== END QUICKHULL =====

	// ===== BUILD MESH FROM CONVEX HULL =====
	// now the convex hull is created, we stuff this in a mesh
	Mesh* convexHull = (Mesh*)malloc(sizeof(Mesh));
	initMesh(convexHull);
	convexHull->hullId = -1;

	size_t numActiveFacets = 0;
	for (int i = 0; i < facetListSize; i++) {
		if (facetList[i].isActive) numActiveFacets++;
	}

	// set mesh vertices
	convexHull->vertices = (Vertex*)malloc(sizeof(Vertex)*numActiveFacets*3); // overallocate then realloc later
	convexHull->faces = (Face*)malloc(sizeof(Face)*numActiveFacets);
	convexHull->num_faces = numActiveFacets;
	size_t faceIndex = 0;
	size_t vertexIndex = 0;
	for (int i = 0; i < facetListSize; i++) {
		if (!facetList[i].isActive) continue;
		Point* p0 = &pointList[facetList[i].points[0]];
		Point* p1 = &pointList[facetList[i].points[1]];
		Point* p2 = &pointList[facetList[i].points[2]];
		if (!p0->assigned) {
			// add p0
			set3fv(convexHull->vertices[vertexIndex].pos, p0->pos);
			p0->vertexIndex = vertexIndex;
			p0->assigned = true;
			vertexIndex++;
		}
		if (!p1->assigned) {
			// add p1
			set3fv(convexHull->vertices[vertexIndex].pos, p1->pos);
			p1->vertexIndex = vertexIndex;
			p1->assigned = true;
			vertexIndex++;
		}
		if (!p2->assigned) {
			// add p2
			set3fv(convexHull->vertices[vertexIndex].pos, p2->pos);
			p2->vertexIndex = vertexIndex;
			p2->assigned = true;
			vertexIndex++;
		}
		// add face p0,p1,p2
		set3u(convexHull->faces[faceIndex].vertexId, p0->vertexIndex, p1->vertexIndex, p2->vertexIndex);
		faceIndex++;
	}

	// realloc
	convexHull->vertices = (Vertex*)realloc(convexHull->vertices, sizeof(Vertex)*vertexIndex);
	convexHull->num_vertices = vertexIndex;
	// ===== END MESH BUILDING =====

	// ===== BEGIN CLEANUP =====
	#undef da_append
	#undef insert_ridge_map
	#undef new_facet

	for (int i = 0; i < facetListSize; i++) {
		if (facetList[i].extPointListDA.items != NULL) {
			free(facetList[i].extPointListDA.items);
			facetList[i].extPointListDA = {NULL, 0, 0};
		}
	}

	for (int i = 0; i < ridgeMapSize; i++) {
		RidgeMapNode* ptr = ridgeMap[i];
		RidgeMapNode* temp;
		while (ptr != NULL) {
			temp = ptr->next;
			free(ptr);
			ptr = temp;
		}
	}
	free(ridgeMap);
	free(pointList);
	free(facetList);
	// ===== END CLEANUP =====

	return convexHull;
}

// return index of mesh vertex farthest along T*dir
unsigned int support(const Mesh* mesh, const float dir[3], const float T[9]) {
	float local_dir[3];
	matvec3(local_dir, T, dir);
	float distance;
	float maxDist = dot3f(mesh->vertices[0].pos, local_dir);
	unsigned int res = 0;
	for (size_t i = 1; i < mesh->num_vertices; i++) {
		distance = dot3f(mesh->vertices[i].pos, local_dir);
		if (distance > maxDist) { 
			maxDist = distance;
			res = i;
		}
	}
	return res;
}

// input: two mesh instances
// output: true if mesh instances collide; false otherwise
bool GJK_intersect(MeshInstance* objectA, MeshInstance* objectB) {
	if (!global_resource_pool.meshes[objectA->globalMeshId]->has_convex_hull) return false;
	if (!global_resource_pool.meshes[objectB->globalMeshId]->has_convex_hull) return false;
	Mesh* hullA = global_resource_pool.meshes[global_resource_pool.meshes[objectA->globalMeshId]->hullId];
	Mesh* hullB = global_resource_pool.meshes[global_resource_pool.meshes[objectB->globalMeshId]->hullId];

	float modelA[16];
	float transformA[9];

	float modelB[16];
	float transformB[9];

	float translate_temp[16];
	float rotate_temp[16];
	float scale_temp[16];
	set_translate_mat(translate_temp, objectA->pos);
	set_rotation_mat(rotate_temp, objectA->rotation);
	set_scale_mat(scale_temp, objectA->scale);
	mat4_mul(modelA, rotate_temp, scale_temp);
	mat4_mul(modelA, translate_temp, modelA);
	set_model_tranpose_mat3(transformA, rotate_temp, objectA->scale);
	
	set_translate_mat(translate_temp, objectB->pos);
	set_rotation_mat(rotate_temp, objectB->rotation);
	set_scale_mat(scale_temp, objectB->scale);
	mat4_mul(modelB, rotate_temp, scale_temp);
	mat4_mul(modelB, translate_temp, modelB);
	set_model_tranpose_mat3(transformB, rotate_temp, objectB->scale);

	struct Simplex { 
		float points[4][3]; 
		unsigned int length;
	};

	Simplex simplex    = {0};
	float dir[3]       = {1.0f, 0.0f, 0.0f};
	float negdir[3]    = {0};
	float supA[3]      = {0};
	float supB[3]      = {0};
	float point[3]     = {0};
	float AO[3]        = {0};
	float AB[3]        = {0};
	float AC[3]        = {0};
	float AD[3]        = {0};
	float ABcrossAO[3] = {0};
	float AOcrossAC[3] = {0};
	float normalABC[3] = {0};
	float normalABD[3] = {0};
	float normalACD[3] = {0};

	// transform dir to local coords before passing to support function
	// transform result back to world coords

	// initialize first point, add to simplex, get direction from pA -> 0
	// pA = support(hA, dir, hA->normal) - support(hB, -dir. hB->normal)
	negate3f(negdir, dir);
	set3fv(supA, hullA->vertices[support(hullA, dir, transformA)].pos);
	set3fv(supB, hullB->vertices[support(hullB, negdir, transformB)].pos);
	// transform to world coords before subtracting
	matvec4_3fv_inplace(modelA, supA);
	matvec4_3fv_inplace(modelB, supB);
	sub3f(point, supA, supB);

	// simplex = {pA}
	set3fv(simplex.points[0], point);
	simplex.length = 1;

	// dir = -pA
	negate3f(dir, simplex.points[0]);

	const int __MAX_ITER__ = 30;
	int iter = 0;
	while (iter < __MAX_ITER__) {
		negate3f(negdir, dir);
	    // pt = support(hA, dir, hA->normal) - support(hB, -dir, hB->normal)
		set3fv(supA, hullA->vertices[support(hullA, dir, transformA)].pos);
		set3fv(supB, hullB->vertices[support(hullB, negdir, transformB)].pos);
		matvec4_3fv_inplace(modelA, supA);
		matvec4_3fv_inplace(modelB, supB);
		sub3f(point, supA, supB);

		if (dot3f(point, dir) < 0.0f) {
			return false; // no intersection
		}

		// simplex = simplex union {pt}
		set3fv(simplex.points[simplex.length], point);
		unsigned int Aindex = simplex.length;
		simplex.length++;

	    // if (update_simplex(&simplex, &direction))
	    //     return true
		switch(simplex.length) {
			case 1:
				// 1 point; set new direction only; this logically should never be entered
				negate3f(dir, simplex.points[Aindex]);
				break;
			case 2: // @TODO: if we fail this check, would we have returned in the `no intersection` check anyways?
				// 2 points; if B->A points in the direction of the origin
				// Yes: simplex = {A};   dir = -A
				// No:  simplex = {A,B}; dir = (AB x AO) x AB
				negate3f(dir, simplex.points[Aindex]); // dir = -A
				sub3f(AB, simplex.points[0], simplex.points[Aindex]);
				if (dot3f(AB, dir) < 0) {
					set3fv(simplex.points[0], simplex.points[Aindex]); // copy A into first element
					simplex.length--;
				} else {
					cross3f(ABcrossAO, AB, dir);
					cross3f(dir, ABcrossAO, AB);
				}
				break;
			case 3:
				// 3 points; test if origin is outside AC or AB:
				// Yes: reject opposite point; set dir = (A(kept) x AO) x A(kept)
				// No:  keep simplex and set dir = normal (oriented towards O)
				negate3f(AO, simplex.points[Aindex]);
				sub3f(AB, simplex.points[0], simplex.points[Aindex]);
				sub3f(AC, simplex.points[1], simplex.points[Aindex]);
				cross3f(normalABC, AB, AC);
				cross3f(ABcrossAO, AB, AO);
				cross3f(AOcrossAC, AO, AC);

				if (dot3f(normalABC, AO) < 0) {
					negate3f_inplace(normalABC);
				}

				if (dot3f(normalABC, ABcrossAO) < 0) {
					// reject C
					set3fv(simplex.points[1], simplex.points[Aindex]);
					simplex.length--;
					cross3f(dir, ABcrossAO, AB);
				} else if (dot3f(normalABC, AOcrossAC) < 0) {
					// reject B
					set3fv(simplex.points[0], simplex.points[Aindex]);
					simplex.length--;
					cross3f(dir, AC, AOcrossAC);
				} else {
					set3fv(dir, normalABC);
				}
				break;
			case 4:
				// 4 points; test if origin is outside any facet:
				// Yes: reject opposite point; set dir = normal of remaining facet pointing to origin
				// No:  return true; origin is inside simplex thus we have a collision
				negate3f(AO, simplex.points[Aindex]);
				sub3f(AB, simplex.points[0], simplex.points[Aindex]);
				sub3f(AC, simplex.points[1], simplex.points[Aindex]);
				sub3f(AD, simplex.points[2], simplex.points[Aindex]);
				cross3f(normalABC, AB, AC); if (dot3f(normalABC, AD) > 0.0f) { negate3f_inplace(normalABC); }
				cross3f(normalABD, AB, AD); if (dot3f(normalABD, AC) > 0.0f) { negate3f_inplace(normalABD); }
				cross3f(normalACD, AC, AD); if (dot3f(normalACD, AB) > 0.0f) { negate3f_inplace(normalACD); }
				if (dot3f(normalABC, AO) > 0) {
					// reject D
					set3fv(simplex.points[2], simplex.points[Aindex]);
					simplex.length--;
					set3fv(dir, normalABC);
				} else if (dot3f(normalABD, AO) > 0) {
					// reject C
					set3fv(simplex.points[1], simplex.points[Aindex]);
					simplex.length--;
					set3fv(dir, normalABD);
				} else if (dot3f(normalACD, AO) > 0) {
					// reject B
					set3fv(simplex.points[0], simplex.points[Aindex]);
					simplex.length--;
					set3fv(dir, normalACD);
				} else {
					// intersection found
					return true;
				}
				break;
			default:
				printf("Error: default in switch(simplex.length) in GJK_intersect on iteration %d with simplex.length=%u\n", iter, simplex.length);
		}
		iter++;
	}
	return false; // max iter reached with no intersection found
}
// ===== END GEOMETRY FUNCTIONS =====

// ===== SHADER HANDLING =====
// Compile the shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (!shader) { fprintf(stderr, "Failed to create shader object\n"); return 0; }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "Shader Compilation Failed\n%s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// compile and link vertex and fragment shaders into a full shader program
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // compile shaders
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    // link shaders to program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // validate
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "Shader Program Linking Failed\n%s\n", infoLog);
        shaderProgram = 0;
    }

    // clean up shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}
// ===== END SHADER HANDLING =====

// ===== LOAD FUNCTIONS =====
// @NOTE: assuming vertex_normal[i] goes to vertex[i] for all i
// @TODO: speed this up. taking 2 seconds to read 125mb text file. speedup may require better non-text file format.
int malloc_mesh_fields_from_obj_file(const char* filename, Mesh* mesh) {
	if (mesh->vertices != nullptr) { printf("mesh->vertices != nullptr in malloc_mesh_fields_from_obj_file\n"); return 0; }
	// size of mesh fields
	size_t num_vertices = 0;
	size_t num_uv_coords = 0;
	size_t num_vnormals = 0;
	size_t num_faces = 0;
	// index for writing to mesh fields
	size_t v_index = 0;
	size_t vt_index = 0;
	size_t vn_index = 0;
	size_t f_index = 0;
	// logical flags
	bool vnormals_enabled = false;
	bool textures_enabled = false;
	bool sscanf_include_normals = false;
	bool sscanf_include_textures = false;
	// buffers for sscanf
	float pos[3];
	unsigned int v[4], t[4], n[4];

	// counts items in file
	char buf[512];
	FILE* file = fopen(filename, "r");
	if (file == NULL) { 
		printf("FILE %s is NULL in malloc_mesh_fields_from_obj_file\n", filename);
		return 0;
	}
	while (fgets(buf, sizeof(buf), file) != NULL) {
		if (buf[0] == 'v' && buf[1] == ' ') ++num_vertices;
		if (buf[0] == 'v' && buf[1] == 't') {
			++num_uv_coords;
			sscanf_include_textures = true;

		}
		if (buf[0] == 'v' && buf[1] == 'n') {
			++num_vnormals;
			sscanf_include_normals = true;
		}
		if (buf[0] == 'f') {
			// check if this is a quad
			int count = 0;
			for (char* p = buf; *(p + 1) != '\0'; p++) { 
				if (*p == ' ' && *(p+1) >= '0' && *(p+1) <= '9')
					count++;
			}
			if (count == 4) {
				num_faces++;
			}
			num_faces++;
		}
	}
	rewind(file);
	if (num_uv_coords == num_vertices) textures_enabled = true;
	if (num_vnormals == num_vertices) vnormals_enabled = true;
	mesh->vertices = (Vertex*)malloc(sizeof(Vertex)*num_vertices);
	mesh->faces = (Face*)malloc(sizeof(Face)*num_faces);
	
	// load items onto memory
	while (fgets(buf, sizeof(buf), file) != NULL) {
		if (buf[0] == 'v' && buf[1] == ' ') {
			if (sscanf(buf, "v %f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
				set3fv(mesh->vertices[v_index].pos, pos);
				++v_index;
			}
		}
		if (textures_enabled && buf[0] == 'v' && buf[1] == 't') {
			if (sscanf(buf, "vt %f %f", &pos[0], &pos[1]) == 2) {
				mesh->vertices[vt_index].texture[0] = pos[0];
				mesh->vertices[vt_index].texture[1] = pos[1];
				++vt_index;
			}
		}
		if (vnormals_enabled && buf[0] == 'v' && buf[1] == 'n') {
			if (sscanf(buf, "vn %f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
				set3fv(mesh->vertices[vn_index].normal, pos);
				++vn_index;
			}
		}
		if (buf[0] == 'f') {
			int res;
			bool is_quad = false;
			bool is_good = false;
			// change sscanf based on presence of vnormals
			if (sscanf_include_textures && sscanf_include_normals) {
				res = sscanf(buf, "f %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u", &v[0], &t[0], &n[0], &v[1], &t[1], &n[1], &v[2], &t[2], &n[2], &v[3], &t[3], &n[3]);
				if (res == 9 || res == 12) {
					is_good = true;
				}
			} else if (sscanf_include_normals) {
				res = sscanf(buf, "f %u//%u %u//%u %u//%u %u//%u", &v[0], &n[0], &v[1], &n[1], &v[2], &n[2], &v[3], &n[3]);
				if (res == 6 || res == 8)
					is_good = true;
			} else {
				res = sscanf(buf, "f %u %u %u %u", &v[0], &v[1], &v[2], &v[3]);
				if (res == 3 || res == 4)
					is_good = true;
			}

			// if this is a quad, set flag
			if (res == 4 || res == 8 || res == 12)
				is_quad = true;
			
			// if parse was good
			if (is_good) {
				mesh->faces[f_index].vertexId[0] = v[0] - 1;
				mesh->faces[f_index].vertexId[1] = v[1] - 1;
				mesh->faces[f_index].vertexId[2] = v[2] - 1;
				++f_index;
				// second triangle of the quad
				if (is_quad) {
					mesh->faces[f_index].vertexId[0] = v[2] - 1;
					mesh->faces[f_index].vertexId[1] = v[3] - 1;
					mesh->faces[f_index].vertexId[2] = v[0] - 1;
					++f_index;
				}
			} else {
				printf("is_good=false in malloc_mesh_fields_from_obj_file\n");
				printf("sscanf_include_normals=%d | is_quad=%d | res=%d\n", sscanf_include_normals, is_quad, res);
			}
		}
	}
	fclose(file);
	if (v_index != num_vertices || f_index != num_faces || (vnormals_enabled && vn_index != num_vnormals)) { 
		fprintf(stderr, "Failed to read .obj file:\n");
		if (v_index != num_vertices) printf("  -->");
		printf("  vertex mismatch: %d %d\n", v_index, num_vertices);
		if (f_index != num_faces) printf("  -->");
		printf("  faces mismatch: %d %d\n", f_index, num_faces);
		if (textures_enabled) {
			if (vt_index != num_uv_coords) printf("  -->");
			printf("  texture mismatch: %d %d\n", vt_index, num_uv_coords);
		} else {
			printf("  textures disabled\n");
		}
		if (vnormals_enabled) {
			if (vn_index != num_vnormals) printf("  -->");
			printf("  normal mismatch: %d %d\n", vn_index, num_vnormals);
		} else {
			printf("  normals disabled\n");
		}
		free(mesh->vertices);
		free(mesh->faces);
		return 0;
	}

	mesh->num_vertices = num_vertices;
	mesh->num_faces = num_faces;
	mesh->has_normals = vnormals_enabled;
	return 1;
}

// assume textureFiles is an array of 6 filenames
// @TODO: add custom .tga loader to remove stbi dependency.
unsigned int loadCubemap(const char** textureFiles) {
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width[6];
	int height[6];
	int nr[6];
	unsigned char* cubemapFaceData[6];

	#pragma omp parallel for
	for (int i = 0; i < 6; i++) {
		cubemapFaceData[i] = stbi_load(textureFiles[i], &width[i], &height[i], &nr[i], 0);
	}

	// we can't parallelize the upload to gpu; it requires opengl context on master thread
	for (int i = 0; i < 6; i++) {
		if (cubemapFaceData[i]) {
			GLenum format = (nr[i] == 4) ? GL_RGBA : GL_RGB;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width[i], height[i], 0, format, GL_UNSIGNED_BYTE, cubemapFaceData[i]);
		} else {
			printf("failed to load cubemap image: %s\n", textureFiles[i]);
		}
		stbi_image_free(cubemapFaceData[i]);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

// reads text file onto a new string
char* mallocTextFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) { fprintf(stderr, "Failed to open text file: %s\n", filename); return NULL; }

    // Seek to end to get file size
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    // Allocate buffer (+1 for null terminator)
    char* text = (char*)malloc(sizeof(char) * (length + 1));
    if (!text) { fclose(file); fprintf(stderr, "Memory allocation failed in mallocTextFromFile\n"); return NULL; }

    // Read file into buffer
    size_t read_size = fread(text, 1, length, file);
    text[read_size] = '\0';

    fclose(file);
    return text;
}

// input filenames for vertex shader and fragment shader sources
// output shader reference
GLuint loadShader(const char* vertexShaderSourceFilename, const char* fragmentShaderSourceFilename) {
	const char* vertexShaderSource = mallocTextFromFile(vertexShaderSourceFilename);
	const char* fragmentShaderSource = mallocTextFromFile(fragmentShaderSourceFilename);
    GLuint shaderProgram = 0;

    if (vertexShaderSource && fragmentShaderSource) {
        shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    } else {
        fprintf(stderr, "Shader source is null in loadShader\n");
    }

	free((void*)vertexShaderSource);
	free((void*)fragmentShaderSource);

	return shaderProgram;
}
// ===== END LOAD FUNCTIONS =====

// ==== REGISTER FUNCTIONS =====
// register mesh instance
int addMeshInstanceToGlobalScene(MeshInstance* meshInstance) { 
	if (global_scene.meshInstanceCount != __MAX_MODELS__) {
		global_scene.meshInstances[global_scene.meshInstanceCount] = meshInstance;
		global_scene.meshInstanceCount++;
	}
	return global_scene.meshInstanceCount;
}

// register mesh; returns Id of registered mesh
int addMeshToGlobalPool(Mesh* mesh) {
	if (global_resource_pool.meshCount != __MAX_MESHES__)
		global_resource_pool.meshes[global_resource_pool.meshCount++] = mesh;
	return global_resource_pool.meshCount - 1;
}

// register texture
int addTextureToGlobalPool(Texture* texture) {
	if (global_resource_pool.textureCount != __MAX_TEXTURES__)
		global_resource_pool.textures[global_resource_pool.textureCount++] = texture;
	return global_resource_pool.textureCount;
}

// register shader
int addShaderToGlobalPool(Shader* shader) {
	if (global_resource_pool.shaderCount != __MAX_SHADERS__)
		global_resource_pool.shaders[global_resource_pool.shaderCount++] = shader;
	return global_resource_pool.shaderCount;
}

// Upload mesh to the GPU
void uploadMeshBuffers(const Mesh *mesh) {
	// Upload vertex data
	glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mesh->num_vertices, mesh->vertices, GL_STATIC_DRAW);

	// Upload index data
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 3 * mesh->num_faces, mesh->faces, GL_STATIC_DRAW);
}
// ===== END REGISTER FUNCTIONS =====

// swap between fps mode and normal mode
// GLFW_CURSOR_DISABLED - disable cursor and center mouse like fps game
// GLFW_CURSOR_NORMAL - normal cursor
void swapCursorInputMode(GLFWwindow* window) {
	MouseInfo *mouse = &(global_scene.mouse);
	if (!mouse->canModeSwitch) return;

	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetCursorPosCallback(window, NULL);
		mouse->canModeSwitch = false;
		mouse->firstMouse = true;
	} else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouse_callback);
		mouse->canModeSwitch = false;
	}
}

// rotate camera by euler angles
void rotateCamera(GLFWwindow* window, float yaw, float pitch) {
	Camera* camera = &(global_scene.camera);

	camera->yaw += yaw;
	camera->pitch += pitch;

	if (camera->pitch > 89.0f)
		camera->pitch = 89.0f;
	else if (camera->pitch < -89.0f)
		camera->pitch = -89.0f;

	camera->front[0] = cosf(radiansf(camera->yaw)) * cosf(radiansf(camera->pitch));
	camera->front[1] = sinf(radiansf(camera->pitch));
	camera->front[2] = sinf(radiansf(camera->yaw)) * cosf(radiansf(camera->pitch));
	normalize3f_inplace(camera->front);
}

// ===== MAIN LOOP FUNCTIONS =====
// trigger events based on inputs
// @TODO: probably set flags here and process input events from mapping flags -> actions ? 
void processInput(GLFWwindow* window, float deltaTime) {
	Camera *camera = &(global_scene.camera);
	MouseInfo *mouse = &(global_scene.mouse);
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		float factor = camera->speed * deltaTime;
		camera->pos[0] += camera->front[0] * factor;
		camera->pos[1] += camera->front[1] * factor;
		camera->pos[2] += camera->front[2] * factor;
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		float right[3];
		cross3f(right, camera->up, camera->front);
		normalize3f_inplace(right);
		float factor = camera->speed * deltaTime;
		camera->pos[0] += right[0] * factor;
		camera->pos[1] += right[1] * factor;
		camera->pos[2] += right[2] * factor;
	}

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera->pos[1] += camera->speed * deltaTime;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		float factor = camera->speed * deltaTime;
		camera->pos[0] -= camera->front[0] * factor;
		camera->pos[1] -= camera->front[1] * factor;
		camera->pos[2] -= camera->front[2] * factor;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		float right[3];
		cross3f(right, camera->up, camera->front);
		normalize3f_inplace(right);
		float factor = camera->speed * deltaTime;
		camera->pos[0] -= right[0] * factor;
		camera->pos[1] -= right[1] * factor;
		camera->pos[2] -= right[2] * factor;
	}
		
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		camera->pos[1] -= camera->speed * deltaTime;
	}

	if (glfwGetKey(window, GLFW_KEY_G) == GLFW_RELEASE) {
		mouse->canModeSwitch = true;
	}

	if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
		swapCursorInputMode(window);
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		camera->speed = camera->TOP_MOVE_SPEED;
	} else {
		camera->speed = camera->NORMAL_MOVE_SPEED;
	}

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
		rotateCamera(window, 0.0f, camera->TILT_SPEED * deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
		rotateCamera(window, 0.0f, -camera->TILT_SPEED * deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		rotateCamera(window, -camera->PAN_SPEED * deltaTime, 0.0f);
	}

	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		rotateCamera(window, camera->PAN_SPEED * deltaTime, 0.0f);
	}
}

// calculate deltaTime, FPS, etc
void updateTime(Metrics* metrics) {
	metrics->lastTime = metrics->currTime;
	metrics->currTime = glfwGetTime();
	metrics->deltaTime = metrics->currTime - metrics->lastTime;
	metrics->heartBeat += metrics->deltaTime;

	// fps counter implementation
	metrics->frameCount = (metrics->frameCount + 1) % metrics->FRAMES_TO_COUNT;
	if (!metrics->frameCount) {
		metrics->fpsWindowTimeStart = metrics->fpsWindowTimeEnd;
		metrics->fpsWindowTimeEnd = metrics->currTime;
		metrics->fps = metrics->FRAMES_TO_COUNT / (metrics->fpsWindowTimeEnd - metrics->fpsWindowTimeStart);
	}
}

// physics for now
void updateScene(GLFWwindow* window, float deltaTime) {
	// handles collision detection
	executeCollisionCheck();

	// update position based on velocity vector
	MeshInstance* active_instance;
	for (int i = 0; i < global_scene.meshInstanceCount; i++) { 
		active_instance = global_scene.meshInstances[i];
		if (!active_instance->physics) continue;
		active_instance->pos[0] += active_instance->velocity[0]*deltaTime;
		active_instance->pos[1] += active_instance->velocity[1]*deltaTime;
		active_instance->pos[2] += active_instance->velocity[2]*deltaTime;
	}

	// update velocity vector
	// for (int i = 0; i < global_scene.meshInstanceCount; i++) {
	// 	active_instance = global_scene.meshInstances[i];
	// 	if (active_instance->physics & 1) {
	// 		active_instance->velocity[1] -= G_ACCEL*deltaTime;
	// 	}
	// 	if (active_instance->physics & 2) {
	// 		// handle collision logic here
			
	// 	}
	// }

	// for collision detection demo
	float theta = deltaTime * 5.0f;
	float demoRotate[4] = {cosf(theta), 0.0f, sinf(theta), 0.0f};
	active_instance = global_scene.meshInstances[0];
	active_instance->pos[0] += deltaTime;
	quat_mult_inplace(active_instance->rotation, demoRotate);
}

void renderScene(GLFWwindow* window) {
	// Clear the screen buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// @NOTE: hardcoding the shader locations here for now
	// this means we are doing that string lookup every frame when we shouldn't need to
	unsigned int basicShader = global_resource_pool.shaders[0]->shaderID;
	unsigned int skyboxShader = global_resource_pool.shaders[1]->shaderID;

	// basic shader uniforms
	unsigned int modelColorLoc = glGetUniformLocation(basicShader, "modelColor");
	unsigned int lightPosLoc = glGetUniformLocation(basicShader, "lightPos");
	unsigned int lightColorLoc = glGetUniformLocation(basicShader, "lightColor");
	unsigned int viewPos = glGetUniformLocation(basicShader, "viewPos");
	unsigned int modelLoc = glGetUniformLocation(basicShader, "model");
	unsigned int normLoc = glGetUniformLocation(basicShader, "normal");
	unsigned int hasNormalsLoc = glGetUniformLocation(basicShader, "hasNormals");
	unsigned int hasTextureLoc = glGetUniformLocation(basicShader, "hasTexture");
	unsigned int projviewLoc = glGetUniformLocation(basicShader, "projview");

	// skybox shader uniforms
	unsigned int skyboxLoc = glGetUniformLocation(skyboxShader, "skybox");
	unsigned int skyboxProjViewLoc = glGetUniformLocation(skyboxShader, "projview");

	// precompute matrix: projview = proj*view
	// @TODO: move the hardcoded values out somewhere
	const float fovy = radiansf(60.0f);
	const float near = 0.1f;
	const float far = 500.0f;
	glfwGetFramebufferSize(window, &global_scene.windowWidth, &global_scene.windowHeight);
	set_perspective_mat(global_scene.proj, fovy, (float)global_scene.windowWidth / (float)global_scene.windowHeight, near, far);
	set_lookat_mat(global_scene.view, global_scene.camera.pos, global_scene.camera.front, global_scene.camera.up);
	mat4_mul(global_scene.projview, global_scene.proj, global_scene.view); 

	// upload uniforms to shader
	glUseProgram(basicShader);
	glUniformMatrix4fv(projviewLoc, 1, GL_FALSE, global_scene.projview);
	glUniform3fv(lightPosLoc, 1, global_scene.lightSource.pos);
	glUniform3fv(lightColorLoc, 1, global_scene.lightSource.color);
	glUniform3fv(viewPos, 1, global_scene.camera.pos);

	// Loop through the scene, build and upload the model matrix, then draw
	for (int i = 0; i < global_scene.meshInstanceCount; i++) {
		MeshInstance* meshInstance = global_scene.meshInstances[i];
		if (meshInstance == nullptr || meshInstance->globalMeshId < 0) continue;
		Mesh* mesh = global_resource_pool.meshes[meshInstance->globalMeshId];
		if (mesh == nullptr) continue;
		// Bind the VAO (restores all attribute and buffer settings)
		glBindVertexArray(mesh->VAO);

		// Build model and normal matrix
		//   Model = Translate * Rotate * Scale
		//   Normal = mat3(Model^(-T))
		// new implementation with no glm dependency
		// NOTE: We could order this cleverly to do everything in place with no temps, but I don't think the added complexity is worth it
		float translate_temp[16];
		float rotate_temp[16];
		float scale_temp[16];
		set_translate_mat(translate_temp, meshInstance->pos);
		set_rotation_mat(rotate_temp, meshInstance->rotation);
		set_scale_mat(scale_temp, meshInstance->scale);
		set_normal_mat3(global_scene.normal, rotate_temp, meshInstance->scale);
		mat4_mul(global_scene.model, rotate_temp, scale_temp);
		mat4_mul(global_scene.model, translate_temp, global_scene.model);
		

		// upload uniforms to the shader
		glUseProgram(basicShader);
		glUniform3fv(modelColorLoc, 1, meshInstance->color);
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, global_scene.model);
		glUniformMatrix3fv(normLoc, 1, GL_FALSE, global_scene.normal);
		glUniform1i(hasNormalsLoc, mesh->has_normals);
		glUniform1i(hasTextureLoc, 0);

		// Issue a draw call
		glDrawElements(GL_TRIANGLES, mesh->num_faces * 3, GL_UNSIGNED_INT, 0);
	}

	// if we want to show the hulls, then do a second pass for them
	bool showHulls = true;
	if (showHulls) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe for hulls
		for (int i = 0; i < global_scene.meshInstanceCount; i++) {
			// error checking
			MeshInstance* meshInstance = global_scene.meshInstances[i];
			if (meshInstance == nullptr || meshInstance->globalMeshId < 0) continue;
			Mesh* mesh = global_resource_pool.meshes[meshInstance->globalMeshId];
			if (!mesh->has_convex_hull) { printf("missing convex hull for mesh %d\n", meshInstance->globalMeshId); continue; } // mesh does not have a hull
			if (!mesh->hullId == -1) { printf("meshInstance[%d] is directly referencing a hull!\n", i); continue; } // somehow a meshInstance is directly using a hull as it's mesh
			
			// get hull
			Mesh* hull = global_resource_pool.meshes[mesh->hullId];
			if (hull->hullId != -1) { printf("mesh->hullId (%d) is not a hull!\n", mesh->hullId); continue; } // the hull mesh is not a hull

			// Bind the VAO (restores all attribute and buffer settings)
			glBindVertexArray(hull->VAO);

			// Build model and normal matrix
			//   Model = Translate * Rotate * Scale
			//   Normal = mat3(Model^(-T))
			// new implementation with no glm dependency
			// NOTE: We could order this cleverly to do everything in place with no temps, but I don't think the added complexity is worth it
			float translate_temp[16];
			float rotate_temp[16];
			float scale_temp[16];
			set_translate_mat(translate_temp, meshInstance->pos);
			set_rotation_mat(rotate_temp, meshInstance->rotation);
			set_scale_mat(scale_temp, meshInstance->scale);
			set_normal_mat3(global_scene.normal, rotate_temp, meshInstance->scale);
			mat4_mul(global_scene.model, rotate_temp, scale_temp);
			mat4_mul(global_scene.model, translate_temp, global_scene.model);
			
			// upload uniforms to the shader
			glUseProgram(basicShader);
			glUniform3fv(modelColorLoc, 1, meshInstance->hullColor);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, global_scene.model);
			glUniformMatrix3fv(normLoc, 1, GL_FALSE, global_scene.normal);
			glUniform1i(hasNormalsLoc, 0);
			glUniform1i(hasTextureLoc, 0);

			// Issue a draw call
			glDrawElements(GL_TRIANGLES, hull->num_faces * 3, GL_UNSIGNED_INT, 0);
		}
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}


	// draw skybox last
	glDepthFunc(GL_LEQUAL);

	// skybox needs to follow us around
	remove_translation_mat4(global_scene.view);

	// precompute projview
	mat4_mul(global_scene.projview, global_scene.proj, global_scene.view);

	glUseProgram(skyboxShader);
	glUniform1f(skyboxLoc, 0);
	glUniformMatrix4fv(skyboxProjViewLoc, 1, GL_FALSE, global_scene.projview);
	glBindVertexArray(global_scene.skybox.VAO);
	glBindTexture(GL_TEXTURE_CUBE_MAP, global_scene.skybox.cubemapID);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// reset depth func
	glDepthFunc(GL_LESS);

	// Unbind the active VAO
	glBindVertexArray(0);
}
// ===== END MAIN LOOP FUNCTIONS =====

// quick mesh instance default init
void setDefaultMeshInstance(MeshInstance* meshInstance, const int resourceId) {
	meshInstance->globalMeshId = resourceId;
	meshInstance->globalTextureId = -1;

	set3f(meshInstance->pos, 0.0f, 0.0f, 0.0f);
	set3f(meshInstance->color, 1.0f, 1.0f, 1.0f);
	set3f(meshInstance->scale, 1.0f, 1.0f, 1.0f);
	set4f(meshInstance->rotation, 1.0f, 0.0f, 0.0f, 0.0f);

	set3f(meshInstance->velocity, 0.0f, 0.0f, 0.0f);
	
	set3f(meshInstance->hullColor, 1.0f, 0.5f, 0.0f);

	//meshInstance->physics = 3;
}

// load initial scene (malloc mesh instances)
void setDefaultScene() {
	// row 1 of each mesh
	const float spacing = 10;
	for (int i = 0; i < global_resource_pool.meshCount; i++) {
		MeshInstance* meshInstance = (MeshInstance*)malloc(sizeof(MeshInstance));
		setDefaultMeshInstance(meshInstance, i);
		set3f(meshInstance->pos, i*spacing, 0.0f, 0.0f);
		meshInstance->physics = 2;
		addMeshInstanceToGlobalScene(meshInstance);
	}

	// teapots falling to floor
	// floor
	// MeshInstance* floorInstance = (MeshInstance*)malloc(sizeof(MeshInstance));
	// floorInstance->globalMeshId = 1;
	// set3f(floorInstance->pos, 0.0f, -20.0f, 0.0f);
	// set3f(floorInstance->color, 0.1f, 0.1f, 0.1f);
	// set3f(floorInstance->scale, 200.0f, 0.1f, 200.0f);
	// set4f(floorInstance->rotation, 1.0f, 0.0f, 0.0f, 0.0f);
	// floorInstance->physics = 2;
	// addMeshInstanceToGlobalScene(floorInstance);

	// // physics entities
	// int num_teapots = 1 << 5;
	// const float spacing = 10;
	// MeshInstance* instancePool = (MeshInstance*)malloc(sizeof(MeshInstance) * num_teapots);
	// for (int i = 0; i < num_teapots; i++) {
	// 	setDefaultMeshInstance(&instancePool[i], 0);
	// 	set3f(instancePool[i].pos, spacing*randf(), spacing*randf() + 50.0f, spacing*randf());
	// 	addMeshInstanceToGlobalScene(&instancePool[i]);
	// 	instancePool[i].physics = 3;
	// }

	// light source
	set3f(global_scene.lightSource.pos, 5.0f, 50.0f, 15.0f);

	// skybox
	// @TODO: nothing to do here yet; need to decouple init vs load

	// camera
	set3f(global_scene.camera.pos, 1.25f, 3.5f, 5.0f);
	set3f(global_scene.camera.front, -0.25f, -0.5, -1.0f);
	normalize3f_inplace(global_scene.camera.front);
	global_scene.camera.pitch = degreesf(asinf(global_scene.camera.front[1]));
	global_scene.camera.yaw = degreesf(atan2f(global_scene.camera.front[2], global_scene.camera.front[0]));
}

// ===== INIT FUNCTIONS =====
// NOTE: resource allocation is NOT initialization

// initialize some OpenGL state
void initOpenGL() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);  


	glfwSwapInterval(0); // 0: disable vsync | 1: enable vsync
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
}

// default values for camera
void initCamera(Camera& camera) {
	set3f(camera.pos,   0.0f, 0.0f,  75.0f);
	set3f(camera.front, 0.0f, 0.0f, -1.0f);
	set3f(camera.up,    0.0f, 1.0f,  0.0f);

	camera.pitch = 0.0f;
	camera.yaw = -90.0f;
	camera.speed = 0.0f;

	camera.speed = 5.0f;
	camera.TOP_MOVE_SPEED = 50.0f;
	camera.NORMAL_MOVE_SPEED = 5.0f;

	camera.PAN_SPEED = 90.0f;
	camera.TILT_SPEED = 45.0f;
}

// default values for light source
void initLightSource(LightSource& lightSource) {
	set3f(lightSource.pos, 0.0f, 0.0f, 0.0f);
	set3f(lightSource.color, 1.0f, 1.0f, 1.0f);
}

// default values for MouseInfo (part of global scene)
void initMouseInfo(MouseInfo& mouse) { 
	mouse.lastX = 400;
	mouse.lastY = 300;
	mouse.sensitivity = 0.1f;
	mouse.firstMouse = true;
	mouse.canModeSwitch = true;
}

// default values for metrics
void initMetrics(Metrics* metrics) {
	metrics->currTime = glfwGetTime();
	metrics->heartBeat = 0.0f;
	metrics->frameCount = 0;
	metrics->FRAMES_TO_COUNT = 60;
}

// builds (no malloc) default mesh including VAO,VBO,EBO objects
int initMesh(Mesh* mesh) {
	// init values
	mesh->vertices = nullptr;
	mesh->faces = nullptr; 
	mesh->num_vertices = 0;
	mesh->num_faces = 0;
	mesh->has_normals = false;

	mesh->has_convex_hull = false;
	mesh->hullId = 0;

	mesh->name = nullptr;

	// Build VAO, VBO, EBOs for each mesh in the scene
	// Vertex Attribute Object (VAO): tracks buffers and attribute locations within buffers
	// Vertex Buffer Object (VBO): physical buffer for vertex data
	// Element Buffer Object (EBO): physical buffer of ordered vertex indices for rendering order
	
	// Generate objects
	glGenVertexArrays(1, &(mesh->VAO));
	glGenBuffers(1, &(mesh->VBO));
	glGenBuffers(1, &(mesh->EBO));

	// Bind objects
	glBindVertexArray(mesh->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);

	// Configure vertex attributes
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); // Position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float))); // Vector normals
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float))); // Texture coordinates
	glEnableVertexAttribArray(2);

	// Unbind this VAO
	glBindVertexArray(0);

	return 0;
}

// load, compile, malloc, register shaders to the global resource pool
// for now this hardcodes the shaders with ID
void initShaders() {
	//////////////////////////////////////////////
	// what are SHADERS? two types we can control:
	// vertex shader: defines transformation on individual vertices (usually position)
	// fragment: individual pixel information (position, color, etc) linearly interpolated from vertices
	// fragment shader: defines transformation on individual fragments (usually color)
	//////////////////////////////////////////////
	
	unsigned int basicShader = loadShader("src/shaders/basicShader.vs", "src/shaders/basicShader.fs");
	unsigned int skyboxShader = loadShader("src/shaders/skyboxShader.vs", "src/shaders/skyboxShader.fs");
	if (basicShader == 0) { printf("basicShader==0 in initShaders\n"); }
	if (basicShader == 1) { printf("skyboxShader==0 in initShaders\n"); }

	// @TODO: learn how to pool these resources correctly
	Shader* shader0 = (Shader*)calloc(1, sizeof(Shader));
	shader0->shaderID = basicShader;
	addShaderToGlobalPool(shader0);

	Shader* shader1 = (Shader*)calloc(1, sizeof(Shader));
	shader1->shaderID = skyboxShader;
	addShaderToGlobalPool(shader1);

	// @TODO: get locations to shader uniforms
	//       if we enhance shader struct to dynamically use these,
	//       then we can initialize that memory here
	// glUseProgram(basicShader);
	// unsigned int modelColorLoc = glGetUniformLocation(basicShader, "modelColor");
	// ...
}

// Load skybox images from file, initialize GL cubemap object, define skybox vertices, initialize skybox VAO/VBO, upload vertices to GPU
// @TODO: Clean up; skybox loads from file can go to global pool; etc
// NOTE: jpg/png loading is too slow. ~100ms per file. TGA increased file size which kept load times about the same.
//       for now we just load the images in parallel for ~170ms total
void initSkybox() {
	#define skybox_tga
	#ifdef skybox_tga
	const char* skyboxFiles[] = {
		"resources/skybox/skybox01/right.tga",
		"resources/skybox/skybox01/left.tga",
		"resources/skybox/skybox01/top.tga",
		"resources/skybox/skybox01/bottom.tga",
		"resources/skybox/skybox01/front.tga",
		"resources/skybox/skybox01/back.tga"
	};
	#endif
	#ifdef skybox_png
	const char* skyboxFiles[] = {
		"resources/skybox/skybox01/right.png",
		"resources/skybox/skybox01/left.png",
		"resources/skybox/skybox01/top.png",
		"resources/skybox/skybox01/bottom.png",
		"resources/skybox/skybox01/front.png",
		"resources/skybox/skybox01/back.png"
	};
	#endif

	global_scene.skybox.cubemapID = loadCubemap(skyboxFiles);

	// @TODO: clean up this vertex initialization
	const float skybox_vertices[108] = {
		// positions          
	    -1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
	  	 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	// @TODO: this memcpy is activating the skybox to the scene
	// 		  vs the init should just init data from file
	//        I want to eventually select the active skybox in the loadScene() function
	memcpy(global_scene.skybox.vertices, skybox_vertices, sizeof(skybox_vertices));

	// Generate objects
	glGenVertexArrays(1, &(global_scene.skybox.VAO));
	glGenBuffers(1, &(global_scene.skybox.VBO));

	// Bind objects
	glBindVertexArray(global_scene.skybox.VAO);
	glBindBuffer(GL_ARRAY_BUFFER, global_scene.skybox.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(global_scene.skybox.vertices), &(global_scene.skybox.vertices), GL_STATIC_DRAW);

	// Configure vertex attributes
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0); // Position
	glEnableVertexAttribArray(0);

	// Unbind this VAO
	glBindVertexArray(0);
}

// @TODO: cleanup
// for each item in the hardcoded filename list:
//     1. malloc+init a new mesh
//     2. malloc mesh fields and load data from file
//     3. register mesh to resource pool list
//     4. upload mesh data to GPU buffers
// return total count of meshes in resource pool
int initGlobalResourcePoolMallocMeshAndMeshFields() {
	global_resource_pool.meshCount = 0;
	global_resource_pool.textureCount = 0;
	const char *list_of_meshes[] = {
		"resources/mesh/teapot.obj"
		,"resources/mesh/box.obj"
		,"resources/mesh/teapot2.obj"
		,"resources/mesh/guy.obj"
		//,"resources/large_files/HP_Portrait.obj"
		//,"resources/large_files/kayle.obj"
		//,"resources/extra_mesh/elf.obj"
	};
	const int vnormal_style = 1; // { 0 = flat | 1 = smooth }
	// this caused a bug because I'm trying to operate on elements of this array as if they were malloc'd individually
	// in the future if I use a pool for this resource, just realloc directly on the global resource pool
	// Mesh* meshList = (Mesh*)malloc(sizeof(Mesh) * num_meshes);  
	for (int i = 0; i < sizeof(list_of_meshes)/sizeof(list_of_meshes[0]); i++) {
		Mesh* mesh = (Mesh*)malloc(sizeof(Mesh));
		initMesh(mesh);
		mesh->name = (char*)malloc(strlen(list_of_meshes[i]) + 1);
		strcpy(mesh->name, list_of_meshes[i]);
		printf("loading mesh from file: %s...", list_of_meshes[i]);
		tic();
		if (!malloc_mesh_fields_from_obj_file(list_of_meshes[i], mesh)) { 
			printf(" | ERROR: malloc_mesh_fields_from_obj_file returned 0 in initGlobalResourcePoolMallocMeshAndMeshFields for mesh %s\n", mesh->name);
			free(mesh);
			continue; 
		}
		printf("(%.3f ms)\n", toc());
		// if we couldn't load normals from file, then compute them now
		if (!mesh->has_normals) {
			printf("  >> Normals not found. Computing");
			tic();
			if (vnormal_style == 0) {
				printf(" flat normals...");
				realloc_mesh_with_face_vertices(mesh);
				compute_vnormal_flat(mesh);
			} else if (vnormal_style == 1) {
				printf(" smooth normals...");
				const int tol = 5;
				int dupe_count = deduplicate_mesh_vertices(mesh, tol);
				printf("(dropped dupes: %d)...", dupe_count);
				compute_vnormal_smooth(mesh);
			} else {
				printf(" | WARNING: vnormal_style not set. Unable to load normals.\n");
			}
			printf("(%.3f ms)\n", toc());
		}

		// @NOTE: We are not writing normals back to the file.
		//   We probably shouldn't mutate resources unless specifically saved from the program, except maybe as one-time processing.
		//   Maybe we write a new function to serialize the whole mesh (with vnormal) back.
		printf("  >> v: %d | f: %d\n", mesh->num_vertices, mesh->num_faces);
		addMeshToGlobalPool(mesh);
		uploadMeshBuffers(mesh);
	}
	return global_resource_pool.meshCount;
}

void initGlobalScene() {
	initCamera(global_scene.camera);
	initLightSource(global_scene.lightSource);
	initMouseInfo(global_scene.mouse);
	tic(); initSkybox(); printf("SKYBOX LOAD: %.3f ms\n", toc());
	global_scene.meshInstanceCount = 0;
}
// ===== END INIT FUNCTIONS =====

// ===== ALGORITHM HANDLERS =====
// wrapper function for building convex hulls and adding to scene
void executeConvexHulls() {
	printf("executeConvexHulls:\n");
	Mesh* hull;
	const int meshCountBeforeHulls = global_resource_pool.meshCount;
	unsigned int hullId;
	for (int i = 0; i < meshCountBeforeHulls; i++) {
		tic();
		hull = makeConvexHull(global_resource_pool.meshes[i]);
		printf("  >> convex hull in %.3f ms for %s\n", toc(), global_resource_pool.meshes[i]->name);
		if (hull != nullptr) {
			hullId = addMeshToGlobalPool(hull);
			global_resource_pool.meshes[i]->hullId = hullId;
			global_resource_pool.meshes[i]->has_convex_hull = true;
			uploadMeshBuffers(hull);
		} else {
			printf("  ERROR: makeConvexHull(meshes[%d]) returned nullptr\n", i);
		}
	}
}

// Orchestrate GJK intersection algorithm
void executeCollisionCheck() {
	if (global_scene.meshInstanceCount < 2) return;
	for (size_t i = 0; i < global_scene.meshInstanceCount; i++) {
		set3f(global_scene.meshInstances[i]->hullColor, 1.0f, 0.5f, 0.0f);
	}
	for (size_t i = 0; i < global_scene.meshInstanceCount; i++) {
		for (size_t j = i+1; j < global_scene.meshInstanceCount; j++) {
			// @TODO: radius check to rule out pairs before trying GJK intersect
			if (GJK_intersect(global_scene.meshInstances[i], global_scene.meshInstances[j])) {
				set3f(global_scene.meshInstances[i]->hullColor, 1.0f, 0.0f, 0.0f);
				set3f(global_scene.meshInstances[j]->hullColor, 1.0f, 0.0f, 0.0f);
			}
		}
	}
}
// ===== END ALGORITHM HANDLERS =====

// ===== PRINT FUNCTIONS =====
// print the global resource pool for debugging
void printGlobalResourcePool() {
	printf("global_resource_pool looks like:\n");
	printf("  >> num_meshes = %d\n", global_resource_pool.meshCount);
	for (int i = 0; i < global_resource_pool.meshCount; i++) {
		printf("  meshes[%d] f: %zu v: %zu", i, global_resource_pool.meshes[i]->num_faces, global_resource_pool.meshes[i]->num_vertices);
		if (global_resource_pool.meshes[i]->has_convex_hull) {
			printf(" | HAS HULLID: %d", global_resource_pool.meshes[i]->hullId);
		} else if (global_resource_pool.meshes[i]->hullId == -1) {
			printf(" | IS HULL");
		}
		printf("\n");
	}
}
// ===== END PRINT FUNCTIONS =====

int main(int argc, char** argv) {
	srand(getSeed());

	// ========================= OPENGL SETUP =========================
	// initialize glfw, glad, OpenGL
	if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return -1; } // GLFW
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // OpenGL 3.x
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // OpenGL 3.3
	GLFWwindow* window = glfwCreateWindow(1280, 720, "GLFW OpenGL", NULL, NULL); // window
	if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return -1; }
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { fprintf(stderr, "Failed to initialize GLAD\n"); return -1; } // GLAD
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); // callback
	
	// custom init OpenGL state
	initOpenGL();
	// ========================= END OPENGL SETUP =========================

	// ========================= SETUP SCENE =========================
	initGlobalScene();
	// @TODO: load_resources_onto_pools();
	initGlobalResourcePoolMallocMeshAndMeshFields();
	//init_global_resources();
	initShaders();
	setDefaultScene();
	// ========================= END SCENE SETUP =========================


	// ================ Playground to test 3D algorithms before render loop ===================
	executeConvexHulls();

	printf("Begin render loop\n");
	// ================ end playground ================


	Metrics metrics;
	initMetrics(&metrics);

	char title[256]; // window title
    const char* glVersion = (const char*)glGetString(GL_VERSION); // driver info
    const char* glRenderer = (const char*)glGetString(GL_RENDERER); // gpu info

	while (!glfwWindowShouldClose(window)) {
		// Update the time
		updateTime(&metrics);

		// Handle input
		processInput(window, metrics.deltaTime);

		// @TODO: separate systems? i.e. updatePhysics(), updateAnim(), ...
		updateScene(window, metrics.deltaTime);

		// Render the global scene to the back buffer
		renderScene(window);

		// Swap buffers
		glfwSwapBuffers(window);

		// Get new inputs
		glfwPollEvents();
 
		// run this every ~1 second
		if (metrics.heartBeat > 1.0f) {
			snprintf(title, sizeof(title), "[FPS: %.2f] GLFW OpenGL - %s - %s", metrics.fps, glVersion, glRenderer);
			glfwSetWindowTitle(window, title);
			metrics.heartBeat = 0.0f;
		}
	} // end main render loop

	// memory cleanup
	free_scene(&global_scene);
	free_resource_pool(&global_resource_pool);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
 