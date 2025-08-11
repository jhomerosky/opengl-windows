#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <omp.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "structs.hpp"
#include "math_utils.hpp"
#include "shaders.hpp"
#include "gl_profile.hpp"
#include "file_utils.hpp"


// ===== GLOBAL VARS =====
Scene global_scene;
ResourcePool global_resource_pool;

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
	camera->front[0] = cos(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	camera->front[1] = sin(radiansf(camera->pitch));
	camera->front[2] = sin(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	normalize_in_place3f(camera->front);
}
// ===== END CALLBACK FUNCTIONS =====

// @TODO: complete
int compute_vector_normals_onto_mesh_smooth(Mesh* mesh) {
	for (int i = 0; i < mesh->num_faces; i++) {
		//
	}
	return 0;
}

// @Assuming: CCW face orientation, faces are triangles
// @Mutates the size of the mesh
int compute_vector_normals_onto_mesh_flat(Mesh* mesh) {
	// for each face:
	// e1 = v2 - v1, e2 = v3 - v2
	// fnormal = normalize(cross(e1, e2))
	// assign this normal to all vertices in this face
	// build new vertex list from unique pairs of v, vnormal

	Vertex *new_vertex_list = (Vertex*)malloc(sizeof(Vertex) * 3 * mesh->num_faces);
	if (new_vertex_list == nullptr) { fprintf(stderr, "Failed to malloc the new vertex list\n"); return -1; }

	#pragma omp parallel for
	for (int i = 0; i < mesh->num_faces; i++) {
		// private memory
		float e1[3];
		float e2[3];
		float res[3];
		// vertices
		Vertex v0 = mesh->vertices[mesh->faces[i].vertexId[0]];
		Vertex v1 = mesh->vertices[mesh->faces[i].vertexId[1]];
		Vertex v2 = mesh->vertices[mesh->faces[i].vertexId[2]];
		// e1
		e1[0] = v1.pos[0] - v0.pos[0];
		e1[1] = v1.pos[1] - v0.pos[1];
		e1[2] = v1.pos[2] - v0.pos[2];
		// e2
		e2[0] = v2.pos[0] - v1.pos[0];
		e2[1] = v2.pos[1] - v1.pos[1];
		e2[2] = v2.pos[2] - v1.pos[2];

		// cross product will not be near 0 because we are using edges of a triangle
		cross3f_to_vec3(e1, e2, res);
		normalize_in_place3f(res);

		// new set new vertices
		new_vertex_list[i*3] = v0;
		new_vertex_list[i*3 + 1] = v1;
		new_vertex_list[i*3 + 2] = v2;

		// for each vn computed (= num faces), store each (of 3) vertices with vn in new list of size 3*num_faces
		new_vertex_list[i*3].normal[0]     = res[0];
		new_vertex_list[i*3 + 1].normal[0] = res[0];
		new_vertex_list[i*3 + 2].normal[0] = res[0];

		new_vertex_list[i*3].normal[1]     = res[1];
		new_vertex_list[i*3 + 1].normal[1] = res[1];
		new_vertex_list[i*3 + 2].normal[1] = res[1];

		new_vertex_list[i*3].normal[2]     = res[2];
		new_vertex_list[i*3 + 1].normal[2] = res[2];
		new_vertex_list[i*3 + 2].normal[2] = res[2];

		// point faces to new vertex index
		mesh->faces[i].vertexId[0] = i*3;
		mesh->faces[i].vertexId[1] = i*3 + 1;
		mesh->faces[i].vertexId[2] = i*3 + 2;
	}
	free(mesh->vertices);
	mesh->vertices = new_vertex_list;
	mesh->num_vertices = 3 * mesh->num_faces;
	mesh->has_normals = true;

	return 0;
}

// @Assuming: vertex_normal[i] goes to vertex[i] for all i
// @TODO: speed this up. taking 2 seconds to read 125mb file.
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
				mesh->vertices[v_index].pos[0] = pos[0];
				mesh->vertices[v_index].pos[1] = pos[1];
				mesh->vertices[v_index].pos[2] = pos[2];
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
				mesh->vertices[vn_index].normal[0] = pos[0];
				mesh->vertices[vn_index].normal[1] = pos[1];
				mesh->vertices[vn_index].normal[2] = pos[2];
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

// register mesh
int addMeshToResourcePool(Mesh* mesh) {
	if (global_resource_pool.meshCount != __MAX_MESHES__)
		global_resource_pool.meshes[global_resource_pool.meshCount++] = mesh;
	return global_resource_pool.meshCount;
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

	camera->front[0] = cos(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	camera->front[1] = sin(radiansf(camera->pitch));
	camera->front[2] = sin(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
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
		cross3f_to_vec3(camera->up, camera->front, right);
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
		cross3f_to_vec3(camera->up, camera->front, right);
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

// ===== INIT FUNCTIONS ===== 
// called before the loop 
void initOpenGL() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);  


	glfwSwapInterval(0); // disable vsync
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
}

// default values for camera
void initCamera(Camera& camera) {
	camera.pos[0] = 0.0f;
	camera.pos[1] = 0.0f;
	camera.pos[2] = 3.0f;

	camera.front[0] =  0.0f;
	camera.front[1] =  0.0f;
	camera.front[2] = -1.0f;

	camera.up[0] = 0.0f;
	camera.up[1] = 1.0f;
	camera.up[2] = 0.0f;

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
	lightSource.pos[0] = 0.0f;
	lightSource.pos[1] = 0.0f;
	lightSource.pos[2] = 0.0f;

	lightSource.color[0] = 1.0f;
	lightSource.color[1] = 1.0f;
	lightSource.color[2] = 1.0f;
}

// default values for MouseInfo (part of global scene)
void initMouseInfo(MouseInfo& mouse) { 
	mouse.lastX = 400;
	mouse.lastY = 300;
	mouse.sensitivity = 0.1f;
	mouse.firstMouse = true;
	mouse.canModeSwitch = true;
}

// builds (no malloc) default mesh including VAO,VBO,EBO objects
int initMesh(Mesh* mesh) {
	// init values
	mesh->vertices = nullptr;
	mesh->faces = nullptr; 
	mesh->num_vertices = 0;
	mesh->num_faces = 0;
	mesh->has_normals = false;

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

// initializes an initial meshInstance with a hardcoded mesh value
void setDefaultMeshInstance(MeshInstance* meshInstance) {
	meshInstance->mesh = global_resource_pool.meshes[0]; // teapot.obj
	meshInstance->texture = nullptr;
	for (int i = 0; i < 3; i++) {
		meshInstance->pos[i] = 0.0f;
		meshInstance->color[i] = 1.0f;
		meshInstance->scale[i] = 1.0f;
	}
	meshInstance->rotation[0] = 1.0f;
	meshInstance->rotation[1] = 0.0f;
	meshInstance->rotation[2] = 0.0f;
	meshInstance->rotation[3] = 0.0f;
}

// load initial scene
void loadScene() {
	MeshInstance* model = (MeshInstance*)malloc(sizeof(MeshInstance));
	setDefaultMeshInstance(model);
	addMeshInstanceToGlobalScene(model);

	// light source
	global_scene.lightSource.pos[0] = 5.0f;
	global_scene.lightSource.pos[1] = 50.f;
	global_scene.lightSource.pos[2] = 15.0f;
}

// this should probably be refactored
// for each item in the hardcoded filename list:
//     1. malloc+init a new mesh
//     2. malloc mesh fields and load data from file
//     3. register mesh to resource pool list
//     4. upload mesh data to GPU buffers
// return total count of meshes in resource pool
int initGlobalResourcePoolMallocMeshAndMeshFields() {
	global_resource_pool.meshCount = 0;
	global_resource_pool.textureCount = 0;
	int num_meshes = 2; // @NOTE: THIS DETERMINES HOW MANY FILES IN LIST TO LOAD
	const char *list_of_meshes[] = {
		"resources/mesh/teapot.obj", 
		"resources/mesh/box.obj", // ending here
		"resources/mesh/teapot2.obj",
		"resources/mesh/guy.obj", 
		"resources/mesh/elf.obj",
		"resources/large_files/HP_Portrait.obj",
		"resources/large_files/kayle.obj"
	};
	for (int i = 0; i < num_meshes; i++) {
		Mesh* mesh = (Mesh*)malloc(sizeof(Mesh));
		initMesh(mesh);
		printf("loading mesh from file: %s\n", list_of_meshes[i]);
		tic();
		malloc_mesh_fields_from_obj_file(list_of_meshes[i], mesh);
		printf("  TIME LOAD %s: %.6f ms\n", list_of_meshes[i], toc());
		// if we couldn't load normals from file, then compute them now
		// @TODO: write normals back to file?
		if (!mesh->has_normals) {
			printf("  Normals not found. Computing normals and rebuilding mesh.\n");
			tic();
			compute_vector_normals_onto_mesh_flat(mesh);
			printf("  TIME COMPUTE NORMALS %s: %.6f ms\n", list_of_meshes[i], toc());
		}
		printf("  v: %d | f: %d\n", mesh->num_vertices, mesh->num_faces);
		addMeshToResourcePool(mesh);
		uploadMeshBuffers(mesh);
	}
	return global_resource_pool.meshCount;
}

void initGlobalScene() {
	initCamera(global_scene.camera);
	initLightSource(global_scene.lightSource);
	initMouseInfo(global_scene.mouse);
	global_scene.meshInstanceCount = 0;
}
// ===== END INIT FUNCTIONS =====

int main(int argc, char** argv) {
	srand(getSeed());

	// ===== OPENGL SETUP =====
	// initialize glfw, glad, OpenGL
	if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return -1; } // glfw
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // OpenGL 3.x
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // OpenGL 3.3
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW OpenGL", NULL, NULL); // window
    if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return -1; }
	glfwMakeContextCurrent(window); // point opengl to this window
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { fprintf(stderr, "Failed to initialize GLAD\n"); return -1; }
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); // call this when window is resized
	initOpenGL(); // custom init OpenGL state
	// ==== END OPENGL SETUP =====

	// ===== SETUP SCENE =====
	initGlobalScene();
	// @TODO: load_resources_onto_pools();
	initGlobalResourcePoolMallocMeshAndMeshFields();
	loadScene();

	// init vars for fps counter
	// @TODO: condense this in some way? wrap in a global singleton?
	float lastTime;
	float currTime = glfwGetTime();
	float deltaTime;
	float fpsWindowTimeStart;
	float fpsWindowTimeEnd = currTime;
	float heartBeat = 0.0f;
	unsigned int frameCount = 0;
	int FRAMES_TO_COUNT = 60;
	float fps;
	char title[256];
    const char* glVersion = (const char*)glGetString(GL_VERSION); // driver info
    const char* glRenderer = (const char*)glGetString(GL_RENDERER); // gpu info

	//////////////////////////////////////////////
	// what are SHADERS? two types we can control:
	// vertex shader: defines transformation on individual vertices (usually position)
	// fragment: individual pixel information (position, color, etc) linearly interpolated from vertices
	// fragment shader: defines transformation on individual fragments (usually color)
	//////////////////////////////////////////////
	const char* vertexShaderSource = loadShaderSource("src/shaders/basicShader.vs");
	const char* fragmentShaderSource = loadShaderSource("src/shaders/basicShader.fs");
	GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
	free((void*)vertexShaderSource);
	free((void*)fragmentShaderSource);

	// get locations to shader uniforms
	unsigned int modelColorLoc = glGetUniformLocation(shaderProgram, "modelColor");
	unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
	unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
	unsigned int viewPos = glGetUniformLocation(shaderProgram, "viewPos");
	unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
	unsigned int normLoc = glGetUniformLocation(shaderProgram, "normal");
	unsigned int hasNormalsLoc = glGetUniformLocation(shaderProgram, "hasNormals");
	unsigned int hasTextureLoc = glGetUniformLocation(shaderProgram, "hasTexture");
	unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
	unsigned int projLoc = glGetUniformLocation(shaderProgram, "proj");

	// model, view, proj matrices
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat3 normal; // normal matrix instead of model matrix for normal vectors to keep the transformation linear

	int windowWidth, windowHeight;
	while (!glfwWindowShouldClose(window)) {
		// Track realtime info
		lastTime = currTime;
		currTime = glfwGetTime();
		deltaTime = currTime - lastTime;
		heartBeat += deltaTime;

		// Handle input
		processInput(window, deltaTime);

		// Clear the screen buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Build the projection matrix (depends on fov, aspect ratio, draw distance)
		glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
		proj = glm::perspective(radiansf(45.0f), (float)windowWidth / (float)windowHeight, 0.1f, 500.0f);

		// Build the view matrix (depends on camera update)
		glUseProgram(shaderProgram);
		glm::vec3 camera_pos = glm::vec3(global_scene.camera.pos[0], global_scene.camera.pos[1], global_scene.camera.pos[2]);
		view = glm::lookAt(
			camera_pos, 
			camera_pos + glm::vec3(global_scene.camera.front[0], global_scene.camera.front[1], global_scene.camera.front[2]), 
			glm::vec3(global_scene.camera.up[0], global_scene.camera.up[1], global_scene.camera.up[2])
		);
		
		// upload uniforms to shader
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniform3fv(lightPosLoc, 1, global_scene.lightSource.pos);
		glUniform3fv(lightColorLoc, 1, global_scene.lightSource.color);
		glUniform3fv(viewPos, 1, global_scene.camera.pos);

		// Loop through the scene, build and upload the model matrix, then draw
		for (int i = 0; i < global_scene.meshInstanceCount; i++) {
			MeshInstance* meshInstance = global_scene.meshInstances[i];
			if (meshInstance == nullptr || meshInstance->mesh == nullptr) continue;
			Mesh* mesh = meshInstance->mesh;
			// Bind the VAO (restores all attribute and buffer settings)
			glBindVertexArray(mesh->VAO);

			// Build model and normal matrix
			model = glm::mat4(1.0f);
			glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(meshInstance->pos[0], meshInstance->pos[1], meshInstance->pos[2]));
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(meshInstance->scale[0], meshInstance->scale[1], meshInstance->scale[2]));
			glm::mat4 rotate = glm::mat4_cast(glm::quat(meshInstance->rotation[0], meshInstance->rotation[1], meshInstance->rotation[2], meshInstance->rotation[3]));
			model = translate * rotate * scale;
			normal = glm::mat3(glm::transpose(glm::inverse(model)));

			// upload uniforms to the shader
			glUniform3fv(modelColorLoc, 1, meshInstance->color);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			glUniformMatrix3fv(normLoc, 1, GL_FALSE, glm::value_ptr(normal));
			glUniform1i(hasNormalsLoc, mesh->has_normals);
			glUniform1i(hasTextureLoc, 0);

			// Issue a draw call
			glUseProgram(shaderProgram);
			glDrawElements(GL_TRIANGLES, mesh->num_faces * 3, GL_UNSIGNED_INT, 0);
		}
		// Unbind the active VAO
		glBindVertexArray(0);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
		
		// fps counter implementation
		frameCount = (frameCount + 1) % FRAMES_TO_COUNT;
		if (!frameCount) {
			fpsWindowTimeStart = fpsWindowTimeEnd;
			fpsWindowTimeEnd = currTime;
			fps = FRAMES_TO_COUNT / (fpsWindowTimeEnd - fpsWindowTimeStart);
		}
 
		// heartbeat implementation (this block runs every ~1 sec)
		if (heartBeat > 1.0f) {
			snprintf(title, sizeof(title), "GLFW OpenGL - [FPS: %.2f] - %s - %s", fps, glVersion, glRenderer);
			glfwSetWindowTitle(window, title);
			heartBeat = 0.0f;
		}
	} // end main render loop


    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
 