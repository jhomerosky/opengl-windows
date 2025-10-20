#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "structs.hpp"
#include "math_utils.hpp"
#include "shaders.hpp"
#include "gl_profile.hpp"
#include "file_utils.hpp"

#define G_ACCEL 9.8f

// ===== GLOBAL VARS =====
ResourcePool global_resource_pool;
Scene global_scene;

// ===== END GLOBAL VARS =====

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
	normalize_in_place3f(camera->front);
}
// ===== END CALLBACK FUNCTIONS =====



// hashing strategy: hash = x * prime1 ^ y * prime2 ^ z * prime3
// (x,y,z) = (int)((x,y,z) * 1e5)
static inline unsigned long long float3Hash(const float pos[3], const int decimalFilter) {

	// 1.1234567 * 1e5 = 112345.67 --> 112345
	// 1.1234587 * 1e5 = 112345.87 --> 112345
	// 1.1234587 - 1.1234567 = 0.0000020 < 0.00001 = 1e-5
	const long long primes[3] = {19349669, 83492791, 73856093};
	//const int primes[3] = {113, 569, 877};
	const long long vecints[3] = {
		(long long)(pos[0] * decimalFilter),
		(long long)(pos[1] * decimalFilter),
		(long long)(pos[2] * decimalFilter)
	};
	return (primes[0] * vecints[0]) ^ (primes[1] * vecints[1]) ^ (primes[2] * vecints[2]);
}

// @NOTE: assumes vertices are deduplicated
// compute each face's normal vector, add vnormal to each component vector, normalize all vectors
int compute_vnormal_smooth(Mesh* mesh) {
	printf("  computing vnormal smooth\n");
	// @TODO: can we parallelize computing normals and do addition later?
	for (int i = 0; i < mesh->num_faces; i++) {
		float e1[3];
		float e2[3];
		float res[3];
		// vertices
		const Vertex v0 = mesh->vertices[mesh->faces[i].vertexId[0]];
		const Vertex v1 = mesh->vertices[mesh->faces[i].vertexId[1]];
		const Vertex v2 = mesh->vertices[mesh->faces[i].vertexId[2]];
		// e1
		e1[0] = v1.pos[0] - v0.pos[0];
		e1[1] = v1.pos[1] - v0.pos[1];
		e1[2] = v1.pos[2] - v0.pos[2];
		// e2
		e2[0] = v2.pos[0] - v1.pos[0];
		e2[1] = v2.pos[1] - v1.pos[1];
		e2[2] = v2.pos[2] - v1.pos[2];

		// cross product will not be near 0 because we are using edges of a triangle
		cross3f(res, e1, e2);

		// add this face's vnormal to each vertex in the face
		mesh->vertices[mesh->faces[i].vertexId[0]].normal[0] += res[0];
		mesh->vertices[mesh->faces[i].vertexId[0]].normal[1] += res[1];
		mesh->vertices[mesh->faces[i].vertexId[0]].normal[2] += res[2];

		mesh->vertices[mesh->faces[i].vertexId[1]].normal[0] += res[0];
		mesh->vertices[mesh->faces[i].vertexId[1]].normal[1] += res[1];
		mesh->vertices[mesh->faces[i].vertexId[1]].normal[2] += res[2];

		mesh->vertices[mesh->faces[i].vertexId[2]].normal[0] += res[0];
		mesh->vertices[mesh->faces[i].vertexId[2]].normal[1] += res[1];
		mesh->vertices[mesh->faces[i].vertexId[2]].normal[2] += res[2];
	}
	// normalize every vertex's vnormal;
	// @NOTE: Benchmark shows omp threads speeds this up but simd does not
	#pragma omp parallel for
	for (int i = 0; i < mesh->num_vertices; i++) {
		normalize_in_place3f(mesh->vertices[i].normal);
	}
	mesh->has_normals = true;
	return 0;
}

// deduplicate a mesh's vertex list on position with tolerance of 1e-(tol)
// @TODO: some vertices are already used by many different faces and this will still check those anyways
//        which results in a very large number of unnecessary checks. can we eliminate that behavior?
// @NOTE: deduplication helps with smooth shading, but in the end especially with texture uv coords we will still need
//        to use face-vertices on gpu. Time spent improving this is probably not productive.
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
	if (new_vertex_list == nullptr) { fprintf(stderr, "Failed to malloc the new vertex list in deduplicate_mesh_vertices\n"); return -1; }
	unsigned int vertex_index = 0;

	for (int i = 0; i < mesh->num_faces; i++) {
		Face* temp = &(mesh->faces[i]);
		for (int j = 0; j < 3; j++) {
			// hash each vertex
			Vertex* v = &(mesh->vertices[temp->vertexId[j]]);
			unsigned int hash = float3Hash(v->pos, decimalFilter) % map_size;
			HashNode** ptr = &map[hash];
			
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
				new_vertex_list[(*ptr)->data].pos[0] = v->pos[0];
				new_vertex_list[(*ptr)->data].pos[1] = v->pos[1];
				new_vertex_list[(*ptr)->data].pos[2] = v->pos[2];
			}
			temp->vertexId[j] = (*ptr)->data;
		}
	}

	int dupe_count = mesh->num_vertices - vertex_index;
	printf("  duplicates dropped = %d\n", dupe_count);

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

	return 0;
}

// Mutates the size of the mesh to 3*(num_faces) vertices.
// Face-vertices: a vertex contains { pos, vn, uv } so consider each vertex with the face it belongs to
int realloc_mesh_with_face_vertices(Mesh* mesh) {
	Vertex *new_vertex_list = (Vertex*)malloc(sizeof(Vertex) * 3 * mesh->num_faces);
	if (new_vertex_list == nullptr) { fprintf(stderr, "Failed to malloc the new vertex list in realloc_mesh_with_face_vertices\n"); return -1; }

	// NOTE: benchmark demonstrated >2x speedup multithreading speedup
	#pragma omp parallel for
	for (int i = 0; i < mesh->num_faces; i++) {
		// add to new list; note no overlap between threads
		new_vertex_list[i*3] = mesh->vertices[mesh->faces[i].vertexId[0]];
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
	printf("  computing vnormal flat\n");
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
		// e1
		e1[0] = v1->pos[0] - v0->pos[0];
		e1[1] = v1->pos[1] - v0->pos[1];
		e1[2] = v1->pos[2] - v0->pos[2];
		// e2
		e2[0] = v2->pos[0] - v1->pos[0];
		e2[1] = v2->pos[1] - v1->pos[1];
		e2[2] = v2->pos[2] - v1->pos[2];

		// cross product will not be near 0 because we are using edges of a triangle
		cross3f(res, e1, e2);
		normalize_in_place3f(res);

		// write normal to vertex
		set3f(v0->normal, res[0], res[1], res[2]);
		set3f(v1->normal, res[0], res[1], res[2]);
		set3f(v2->normal, res[0], res[1], res[2]);
	}
	mesh->has_normals = true;

	return 0;
}

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
	while (fgets(buf, 2048, file) != NULL) {
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
			if (count == 4) 
				num_faces++;
			num_faces++;
		}
	}
	rewind(file);
	if (num_uv_coords == num_vertices) textures_enabled = true;
	if (num_vnormals == num_vertices) vnormals_enabled = true;
	mesh->vertices = (Vertex*)malloc(sizeof(Vertex)*num_vertices);
	mesh->faces = (Face*)malloc(sizeof(Face)*num_faces);
	
	// load items onto memory
	while (fgets(buf, 512, file) != NULL) {
		if (buf[0] == 'v' && buf[1] == ' ') {
			if (sscanf(buf, "v %f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
				set3f(mesh->vertices[v_index].pos, pos[0], pos[1], pos[2]);
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
				set3f(mesh->vertices[vn_index].normal, pos[0], pos[1], pos[2]);
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

		
		return 0;
	}

	mesh->num_vertices = num_vertices;
	mesh->num_faces = num_faces;
	mesh->has_normals = vnormals_enabled;
	return 1;
}

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
	normalize_in_place3f(camera->front);
}

// trigger events based on inputs
void processInput(GLFWwindow* window, float deltaTime) {
	Camera *camera = &(global_scene.camera);
	MouseInfo *mouse = &(global_scene.mouse);
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		float factor = camera->speed * deltaTime;
		for (int i = 0; i < 3; i++)camera->pos[i] += camera->front[i] * factor;
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		float right[3];
		cross3f(right, camera->up, camera->front);
		normalize_in_place3f(right);
		float factor = camera->speed * deltaTime;
		for (int i = 0; i < 3; i++) camera->pos[i] += right[i] * factor;
	}

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera->pos[1] += camera->speed * deltaTime;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		float factor = camera->speed * deltaTime;
		for (int i = 0; i < 3; i++) camera->pos[i] -= camera->front[i] * factor;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		float right[3];
		cross3f(right, camera->up, camera->front);
		normalize_in_place3f(right);
		float factor = camera->speed * deltaTime;
		for (int i = 0; i < 3; i++) camera->pos[i] -= right[i] * factor;
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
	for (int i = 0; i < global_scene.meshInstanceCount; i++) {
		active_instance = global_scene.meshInstances[i];
		if (active_instance->physics & 1) {
			active_instance->velocity[1] -= G_ACCEL*deltaTime;
		}
		if (active_instance->physics & 2) {
			// handle collision logic here
			
		}
	}
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
		set_normal_mat(global_scene.normal, rotate_temp, meshInstance->scale);
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
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe for hulls
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
			set_normal_mat(global_scene.normal, rotate_temp, meshInstance->scale);
			mat4_mul(global_scene.model, rotate_temp, scale_temp);
			mat4_mul(global_scene.model, translate_temp, global_scene.model);
			
			// upload uniforms to the shader
			glUseProgram(basicShader);
			glUniform3fv(modelColorLoc, 1, meshInstance->hullColor);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, global_scene.model);
			glUniformMatrix3fv(normLoc, 1, GL_FALSE, global_scene.normal);
			glUniform1i(hasNormalsLoc, 0);
			glUniform1i(hasTextureLoc, 0);

			// @TODO: something is breaking when drawing. faces change on execution.
			//    probably mismanaging memory, maybe related to the faces
			// Issue a draw call
			glDrawElements(GL_TRIANGLES, hull->num_faces * 3, GL_UNSIGNED_INT, 0);
		}
		//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}


	// draw skybox last
	glDepthFunc(GL_LEQUAL);

	// perform equivalent of glm::mat4(glm::mat3(view))
	global_scene.view[3] = 0.0f;
	global_scene.view[7] = 0.0f;
	global_scene.view[11] = 0.0f;
	global_scene.view[12] = 0.0f;
	global_scene.view[13] = 0.0f;
	global_scene.view[14] = 0.0f;
	global_scene.view[15] = 1.0f;

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

// assume textureFiles is an array of 6 filenames
unsigned int loadCubemap(const char** textureFiles) {
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width[6];
	int height[6];
	int nr[6];
	unsigned char* skyboxData[6];

	#pragma omp parallel for
	for (int i = 0; i < 6; i++) {
		skyboxData[i] = stbi_load(textureFiles[i], &width[i], &height[i], &nr[i], 0);
	}

	// we can't parallelize the upload to gpu; it requires opengl context on master thread
	for (int i = 0; i < 6; i++) {
		if (skyboxData[i]) {
			GLenum format = (nr[i] == 4) ? GL_RGBA : GL_RGB;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width[i], height[i], 0, format, GL_UNSIGNED_BYTE, skyboxData[i]);
		} else {
			printf("failed to load cubemap image: %s\n", textureFiles[i]);
		}
		stbi_image_free(skyboxData[i]);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

// ===== INIT FUNCTIONS ===== 
// called before the loop 
void initOpenGL() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);  


	glfwSwapInterval(1); // 0: disable vsync | 1: enable vsync
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
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

// set default values; no malloc in here
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
	
	const int numShaders = 2;
	unsigned int basicShader = loadShader("src/shaders/basicShader.vs", "src/shaders/basicShader.fs");
	unsigned int skyboxShader = loadShader("src/shaders/skyboxShader.vs", "src/shaders/skyboxShader.fs");

	Shader* shader = (Shader*)malloc(sizeof(Shader)*numShaders);
	shader[0].shaderID = basicShader;
	shader[1].shaderID = skyboxShader;

	for (int i = 0; i < numShaders; i++) {
		addShaderToGlobalPool(&shader[i]);
	}

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

// quick mesh instance default init
void setDefaultMeshInstance(MeshInstance* meshInstance, const int resourceId) {
	meshInstance->globalMeshId = resourceId;
	meshInstance->globalTextureId = -1;

	set3f(meshInstance->pos, 0.0f, 0.0f, 0.0f);
	set3f(meshInstance->color, 1.0f, 1.0f, 1.0f);
	set3f(meshInstance->scale, 1.0f, 1.0f, 1.0f);
	set4f(meshInstance->rotation, 1.0f, 0.0f, 0.0f, 0.0f);
	
	set3f(meshInstance->hullColor, 1.0f, 0.5f, 0.0f);

	//meshInstance->physics = 3;
}

// load initial scene (malloc mesh instances)
void loadScene() {
	// row 1 of each mesh
	const float spacing = 5;
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
	normalize_in_place3f(global_scene.camera.front);
	global_scene.camera.pitch = degreesf(asinf(global_scene.camera.front[1]));
	global_scene.camera.yaw = degreesf(atan2f(global_scene.camera.front[2], global_scene.camera.front[0]));
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
	int num_meshes = 1; // @NOTE: THIS DETERMINES HOW MANY FILES IN LIST TO LOAD
	const char *list_of_meshes[] = {
		"resources/mesh/teapot.obj", // ending here
		"resources/mesh/box.obj", 
		"resources/mesh/teapot2.obj",
		"resources/mesh/guy.obj", 
		"resources/mesh/elf.obj", 
		"resources/large_files/HP_Portrait.obj", 
		"resources/large_files/kayle.obj"  
	};
	const int vnormal_style = 0; // { 0 = flat | 1 = smooth }
	Mesh* meshList = (Mesh*)malloc(sizeof(Mesh) * num_meshes);
	for (int i = 0; i < num_meshes; i++) {
		initMesh(&meshList[i]);
		printf("loading mesh from file: %s\n", list_of_meshes[i]);
		tic();
		malloc_mesh_fields_from_obj_file(list_of_meshes[i], &meshList[i]);
		printf("  TIME LOAD %s: %.6f ms\n", list_of_meshes[i], toc());
		// if we couldn't load normals from file, then compute them now
		if (!meshList[i].has_normals) {
			printf("  Normals not found. Computing normals and rebuilding mesh.\n");
			tic();
			if (vnormal_style == 0) {
				realloc_mesh_with_face_vertices(&meshList[i]);
				compute_vnormal_flat(&meshList[i]);
			} else if (vnormal_style == 1) {
				const int tol = 5;
				deduplicate_mesh_vertices(&meshList[i], tol);
				compute_vnormal_smooth(&meshList[i]);
			} else {
				printf("  Warning: vnormal_style not set. Unable to load normals.\n");
			}
			printf("  TIME COMPUTE NORMALS %s: %.6f ms\n", list_of_meshes[i], toc());
		}
		// @TODO: write normals back to file?
		// @NOTE:
		// We probably shouldn't auto mutate resources unless specifically saved from the program, except maybe as one-time processing.
		// Maybe we write a new function to serialize the whole mesh (with vnormal) back.
		printf("  v: %d | f: %d\n", meshList[i].num_vertices, meshList[i].num_faces);
		addMeshToGlobalPool(&meshList[i]);
		uploadMeshBuffers(&meshList[i]);
	}
	return global_resource_pool.meshCount;
}

void initGlobalScene() {
	initCamera(global_scene.camera);
	initLightSource(global_scene.lightSource);
	initMouseInfo(global_scene.mouse);
	tic(); initSkybox(); printf("SKYBOX LOAD: %.6f ms\n", toc());
	global_scene.meshInstanceCount = 0;
}
// ===== END INIT FUNCTIONS =====


// input: mesh
// output: mesh representing the convex hull
// quickhull algorithm
Mesh* makeConvexHull(Mesh* mesh) {
	// smaller vertex for algorithm
	struct Point {
		float pos[3];
	};
	
	// the literature calls these facets but they are just faces
	struct Facet {
		unsigned int points[3]; // index in a list of points
		float normal[3];
		float offset; // plane = { x : n dot x = d }; d is the offset
		int active; // instead of deleting, just mark as inactive
	};

	// convexHull guaranteed to have size <= current size so we prealloc here and resize at the end
	Point* pointListInput = (Point*)malloc(sizeof(Point) * mesh->num_vertices);
	size_t pointListIndex = 0;
	
	// ===== DEDUPLICATION PASS =====
	const int BUCKET_SIZE = 10;
	struct HashNode {
		Vertex* bucket[BUCKET_SIZE];
		size_t bucket_next;
	};

	// set tolerance levels for float[3] equivalence
	const int tol = 5;
	int decimalFilter = 1;
	for (int i = 0; i < tol; i++) decimalFilter *= 10;
	const float eps = 1.0/(float)decimalFilter;

	// use a hash set to dedup vertices
	size_t map_size = mesh->num_vertices/2;
	HashNode* set = (HashNode*)calloc(map_size, sizeof(HashNode));
	for (int i = 0; i < mesh->num_vertices; i++) {
		Vertex* v = &(mesh->vertices[i]);
		unsigned int hash = float3Hash(v->pos, decimalFilter) % map_size;

		// check if vertex is in set
		bool found = false;
		for (int j = 0; j < set[hash].bucket_next; j++) {
			if (equals3f(set[hash].bucket[j]->pos, v->pos, eps)) {
				found = true;
				break;
			}
		}

		// if vertex is not found in the set then add it
		if (!found) {
			HashNode* ptr = &set[hash];
			if (ptr->bucket_next == BUCKET_SIZE) {
				printf("PANIC; RAN OUT OF BUCKET\n");
				continue;
			}
			ptr->bucket[ptr->bucket_next] = v;
			ptr->bucket_next++;
			pointListInput[pointListIndex].pos[0] = v->pos[0];
			pointListInput[pointListIndex].pos[1] = v->pos[1];
			pointListInput[pointListIndex].pos[2] = v->pos[2];
			pointListIndex++;
		}
	}
	free(set);
	pointListInput = (Point*)realloc(pointListInput, pointListIndex * sizeof(Point));	
	printf("vertices deduplicated: %d (before) | %d (after)\n", mesh->num_vertices, pointListIndex);
	// ===== END DEDUP PASS =====

	// list of points used in convex hull, overallocating for now
	size_t pointListSize = 0;
	size_t pointListCap = pointListIndex;
	Point* pointListOutput = (Point*)malloc(sizeof(Point) * pointListCap);

	// list of facets used in convex hull, size may exceed cap and trigger realloc
	size_t facetListSize = 0;
	size_t facetListCap = mesh->num_faces;
	Facet* facetListOutput = (Facet*)malloc(sizeof(Facet) * facetListCap);

	// initial phase, pick 4 extreme points
	Point* minX = &(pointListInput[0]);
	Point* minY = &(pointListInput[0]);
	Point* minZ = &(pointListInput[0]);
	Point* maxX = &(pointListInput[0]);
	Point* maxY = &(pointListInput[0]);
	Point* maxZ = &(pointListInput[0]);
	for (int i = 1; i < pointListIndex; i++) {
		if (pointListInput[i].pos[0] < minX->pos[0])
			minX = &(pointListInput[i]);
		if (pointListInput[i].pos[1] < minY->pos[1])
			minY = &(pointListInput[i]);
		if (pointListInput[i].pos[2] < minZ->pos[2])
			minZ = &(pointListInput[i]);
		if (pointListInput[i].pos[0] > maxX->pos[0])
			maxX = &(pointListInput[i]);
		if (pointListInput[i].pos[1] > maxY->pos[1])
			maxY = &(pointListInput[i]);
		if (pointListInput[i].pos[2] > maxZ->pos[2])
			maxZ = &(pointListInput[i]);
	}
	printf("minX = %.5f %.5f %.5f\n", minX->pos[0], minX->pos[1], minX->pos[2]);
	printf("maxX = %.5f %.5f %.5f\n", maxX->pos[0], maxX->pos[1], maxX->pos[2]);
	printf("minY = %.5f %.5f %.5f\n", minY->pos[0], minY->pos[1], minY->pos[2]);
	printf("maxY = %.5f %.5f %.5f\n", maxY->pos[0], maxY->pos[1], maxY->pos[2]);
	printf("minZ = %.5f %.5f %.5f\n", minZ->pos[0], minZ->pos[1], minZ->pos[2]);
	printf("maxZ = %.5f %.5f %.5f\n", maxZ->pos[0], maxZ->pos[1], maxZ->pos[2]);

	// initial simplex:
	// p1, p2 are min/max of one dim of above
	// p3 is argmax distance from p1p2 segment
	// p4 is argmax distance from plane

	// step 1: take min/max along X
	// for now just taking 4 at random
	pointListOutput[pointListSize++] = *minX;
	pointListOutput[pointListSize++] = *maxX;

	// step 2: find point farthest in perpendicular distance from segment
	Point* p = &(pointListInput[0]);
	float maxDist = 0;
	float newSegment[3]; // p - minX
	float segment[3]; // maxX - minX
	float temp[3];
	float distance;
	for (int i = 1; i < pointListIndex; i++) {
		sub3f(newSegment, pointListInput[i].pos, minX->pos);
		sub3f(segment, maxX->pos, minX->pos);
		cross3f(temp, newSegment, segment);
		distance = sqrtf(dot3f(temp, temp)) / sqrtf(dot3f(segment, segment));
		if (distance > maxDist) {
			p = &pointListInput[i];
			maxDist = distance;
		}
	}
	printf("P3 = %.5f %.5f %.5f with distance=%.5f\n", p->pos[0], p->pos[1], p->pos[2], distance);
	pointListOutput[pointListSize++] = *p;

	// step 3: find point farthest in perpendicular distance from normal
	sub3f(newSegment, p->pos, minX->pos);
	sub3f(segment, maxX->pos, minX->pos);
	cross3f(temp, newSegment, segment);
	normalize_in_place3f(temp); // this is a normal vector to the p1p2p3 triangle
	p = &(pointListInput[0]);
	maxDist = 0;
	distance = 0;
	for (int i = 0; i < pointListIndex; i++) {
		sub3f(newSegment, pointListInput[i].pos, minX->pos);
		distance = fabsf(dot3f(newSegment, temp));
		if (distance > maxDist) {
			p = &pointListInput[i];
			maxDist = distance;
		}
	}
	printf("P4 = %.5f %.5f %.5f with distance=%.5f\n", p->pos[0], p->pos[1], p->pos[2], distance);
	pointListOutput[pointListSize++] = *p;

	// set initial faces for tetrahedron
	set3u(facetListOutput[facetListSize].points, 0, 1, 2);
	facetListOutput[facetListSize].active = true;
	facetListSize++;

	set3u(facetListOutput[facetListSize].points, 0, 1, 3);
	facetListOutput[facetListSize].active = true;
	facetListSize++;

	set3u(facetListOutput[facetListSize].points, 0, 2, 3);
	facetListOutput[facetListSize].active = true;
	facetListSize++;

	set3u(facetListOutput[facetListSize].points, 1, 2, 3);
	facetListOutput[facetListSize].active = true;
	facetListSize++;

	// now the convex hull is created, we stuff this in a mesh
	Mesh* convexHull = (Mesh*)malloc(sizeof(Mesh));
	initMesh(convexHull);
	convexHull->hullId = -1;

	// set mesh vertices
	convexHull->vertices = (Vertex*)malloc(sizeof(Vertex)*pointListSize);
	convexHull->num_vertices = pointListSize;
	for (int i = 0; i < pointListSize; i++) {
		convexHull->vertices[i].pos[0] = pointListOutput[i].pos[0];
		convexHull->vertices[i].pos[1] = pointListOutput[i].pos[1];
		convexHull->vertices[i].pos[2] = pointListOutput[i].pos[2];
	}

	// set mesh faces
	// even though we have face normals here, we may not be able to easily set them back to each vertex
	// also convex hulls will be displayed differently anyways; dont need shading
	convexHull->faces = (Face*)malloc(sizeof(Face)*facetListSize);
	convexHull->num_faces = facetListSize;
	convexHull->has_normals = false;
	size_t faceIndex = 0;
	for (int i = 0; i < facetListSize; i++) {
		if (facetListOutput[i].active) {
			convexHull->faces[faceIndex].vertexId[0] = facetListOutput[i].points[0];
			convexHull->faces[faceIndex].vertexId[1] = facetListOutput[i].points[1];
			convexHull->faces[faceIndex].vertexId[2] = facetListOutput[i].points[2];
			faceIndex++;
		}
		// @TODO: check if we grew too large
	}
	convexHull->faces = (Face*)realloc(convexHull->faces, sizeof(Face)*faceIndex);

	free(pointListInput);
	free(pointListOutput);
	free(facetListOutput);
	return convexHull;
}

// wrapper function for building convex hulls and adding to scene
void executeConvexHulls() {
	Mesh* hull;
	const int meshCountBeforeHulls = global_resource_pool.meshCount;
	unsigned int hullId;
	for (int i = 0; i < meshCountBeforeHulls; i++) {
		printf(">> building convex hull:\n");
		tic();
		hull = makeConvexHull(global_resource_pool.meshes[i]);
		printf(">> convex hull in %.6f ms\n", toc());
		if (hull != nullptr) {
			hullId = addMeshToGlobalPool(hull);
			global_resource_pool.meshes[i]->hullId = hullId;
			global_resource_pool.meshes[i]->has_convex_hull = true;
			uploadMeshBuffers(hull);
		} else {
			printf("makeConvexHull(meshes[%d]) returned nullptr\n", i);
		}
	}
	printf("global_resource_pool looks like:\n");
	printf(">> size = %d\n", global_resource_pool.meshCount);
	for (int i = 0; i < global_resource_pool.meshCount; i++) {
		printf(">> meshes[%d] has_convex_hull: %d\n", i, global_resource_pool.meshes[i]->has_convex_hull);
		printf(">> meshes[%d] hullId: %d\n", i, global_resource_pool.meshes[i]->hullId);
	}
	printf("exiting executeConvexHulls()\n");
}


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
	loadScene();
	// ========================= END SCENE SETUP =========================


	// ================ Playground to test 3D algorithms before render loop ===================
	executeConvexHulls();
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
			snprintf(title, sizeof(title), "GLFW OpenGL - [FPS: %.2f] - %s - %s", metrics.fps, glVersion, glRenderer);
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
 