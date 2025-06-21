#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <omp.h>

#include "structs.cpp"
#include "math_utils.cpp"
#include "shaders.cpp"
#include "glProfile.cpp"


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
	direction[0] = cos(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	direction[1] = sin(radiansf(camera->pitch));
	direction[2] = sin(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	normalize_in_place(direction);
	camera->front = glm::vec3(direction[0], direction[1], direction[2]);
}
// ===== END CALLBACK FUNCTIONS =====

// ===== PRINT FUNCTIONS =====
void print_mesh_to_stdout(const Mesh& mesh) {
	printf("Printing mesh:\n");
	if (mesh.vertices == nullptr) { printf("The mesh is uninitialized\n"); }

	
	// for (int i = 0; i < mesh.num_vertices; i++) {
	// 	printf("V[%d] = [ \n", i);
	// 	printf("  pos { ");
	// 	for (int j = 0; j < 3; j++) { printf("%.4f ", mesh.vertices[i].pos[j]); }
	// 	printf("}\n  color { ");
	// 	for (int j = 0; j < 3; j++) { printf("%.4f ", mesh.vertices[i].color[j]); }
	// 	printf("}\n  normal { ");
	// 	for (int j = 0; j < 3; j++) { printf("%.4f ", mesh.vertices[i].normal[j]); }
	// 	printf("} ]\n");
	// }

	for (int i = 0; i < mesh.num_faces; i++) {
		printf("F[%d] = [ %d | %d | %d ]\n", i, mesh.faces[i].vertexId[0], mesh.faces[i].vertexId[1], mesh.faces[i].vertexId[2]);
	}

	printf("num_vertices=%d\n", mesh.num_vertices);
	printf("num_faces=%d\n", mesh.num_faces);

	if (mesh.has_normals)
		printf("Normal vectors found.\n");
	else
		printf("Normal vectors NOT found.\n");

	printf("End mesh printing.\n");
}

void print_global_scene_to_stdout() {
	printf("scene printing not developed yet\n");
}
// ===== END PRINT FUNCTIONS =====

void normalize_mesh_to_unit_box(Mesh& mesh) {
	float factor = .96;
	float min_x = FLT_MAX, max_x = -FLT_MAX;
	float min_y = FLT_MAX, max_y = -FLT_MAX;
	float min_z = FLT_MAX, max_z = -FLT_MAX;

	float* p;
	for (size_t i = 0; i < mesh.num_vertices; ++i) {
		p = mesh.vertices[i].pos;
		if (p[0] < min_x) min_x = p[0];
		if (p[0] > max_x) max_x = p[0];
		if (p[1] < min_y) min_y = p[1];
		if (p[1] > max_y) max_y = p[1];
		if (p[2] < min_z) min_z = p[2];
		if (p[2] > max_z) max_z = p[2];
	}

	// Compute center and scale
	float center_x = (min_x + max_x) / 2.0f;
	float center_y = (min_y + max_y) / 2.0f;
	float center_z = (min_z + max_z) / 2.0f;
	float scale = factor * 2.0f / maxf(maxf(max_x - min_x, max_y - min_y), max_z - min_z);

	// Recenter and scale vertices
	for (size_t i = 0; i < mesh.num_vertices; ++i) {
		mesh.vertices[i].pos[0] = (mesh.vertices[i].pos[0] - center_x) * scale;
		mesh.vertices[i].pos[1] = (mesh.vertices[i].pos[1] - center_y) * scale;
		mesh.vertices[i].pos[2] = (mesh.vertices[i].pos[2] - center_z) * scale;
	}
}

// @Assuming: CCW face orientation, faces are triangles
// @TODO: parallelize
// @Mutates the mesh vertex list
int compute_and_store_vector_normals(Mesh* mesh) {
	// for each face:
	// e1 = v2 - v1, e2 = v3 - v2
	// fnormal = normalize(cross(e1, e2))
	// assign this normal to all vertices in this face
	// build new vertex list from unique pairs of v, vnormal

	Vertex *new_vertex_list = (Vertex*)malloc(sizeof(Vertex) * 3 * mesh->num_faces);
	if (new_vertex_list == nullptr) { fprintf(stderr, "Failed to malloc the new vertex list\n"); return -1; }

	#pragma omp parallel for
	for (int i = 0; i < mesh->num_faces; i++) {
		//printf("omp num threads: %d\n", omp_get_num_threads());
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
		normalize_in_place(res);

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

// @Assuming: vertex_normal[i] = vertex[i] for all i
// @TODO: speed this up. taking 2 seconds to read 125mb file.
int malloc_mesh_fields_from_obj_file(const char* filename, Mesh* mesh) {
	if (mesh->vertices != nullptr) { printf("mesh->vertices != nullptr in malloc_mesh_fields_from_obj_file\n"); return 0; }
	size_t num_vertices = 0;
	size_t num_faces = 0;
	size_t num_vnormals = 0;
	bool vnormals_enabled = false;

	char buf[2048];
	FILE* file = fopen(filename, "r");
	while (fgets(buf, 2048, file) != NULL) {
		if (buf[0] == '\0' || buf[0] == '#') continue;
		if (buf[0] == 'v' && buf[1] == ' ') ++num_vertices;
		if (buf[0] == 'v' && buf[1] == 'n') ++num_vnormals;
		if (buf[0] == 'f') ++num_faces;
	}
	rewind(file);
	if (num_vnormals == num_vertices) vnormals_enabled = true;

	mesh->vertices = (Vertex*)malloc(sizeof(Vertex)*num_vertices);
	mesh->faces = (Face*)malloc(sizeof(Face)*num_faces);

	size_t v_index = 0;
	size_t vn_index = 0;
	size_t f_index = 0;
	float pos[3];
	unsigned int v[3], n[3];
	while (fgets(buf, 512, file) != NULL) {
		if (buf[0] == '\0' || buf[0] == '#') continue;
		if (buf[0] == 'v' && buf[1] == ' ') {
			if (sscanf(buf, "v %f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
				mesh->vertices[v_index].pos[0] = pos[0];
				mesh->vertices[v_index].pos[1] = pos[1];
				mesh->vertices[v_index].pos[2] = pos[2];
				++v_index;
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
			if (sscanf(buf, "f %u//%u %u//%u %u//%u", &v[0], &n[0], &v[1], &n[1], &v[2], &n[2]) == 6) {
				mesh->faces[f_index].vertexId[0] = v[0] - 1;
				mesh->faces[f_index].vertexId[1] = v[1] - 1;
				mesh->faces[f_index].vertexId[2] = v[2] - 1;
				++f_index;
			} else if (sscanf(buf, "f %u %u %u", &v[0], &v[1], &v[2]) == 3) {
				mesh->faces[f_index].vertexId[0] = v[0] - 1;
				mesh->faces[f_index].vertexId[1] = v[1] - 1;
				mesh->faces[f_index].vertexId[2] = v[2] - 1;
				++f_index;
			}
		}
	}
	fclose(file);

	if (v_index != num_vertices || f_index != num_faces || (vnormals_enabled && vn_index != num_vnormals)) { 
		fprintf(stderr, "Failed to read .obj file\n");
		return 0;
	}

	mesh->num_vertices = num_vertices;
	mesh->num_faces = num_faces;
	mesh->has_normals = vnormals_enabled;
	return 1;
}


int addMeshInstanceToGlobalScene(MeshInstance* meshInstance) { 
	if (global_scene.meshInstanceCount != __MAX_MESHES__)
		global_scene.meshInstances[global_scene.meshInstanceCount++] = meshInstance;
	return global_scene.meshInstanceCount;
}

int addMeshToResourcePool(Mesh* mesh) {
	if (global_resource_pool.meshCount != __MAX_MODELS__)
		global_resource_pool.meshes[global_resource_pool.meshCount++] = mesh;
	return global_resource_pool.meshCount;
}

// Upload the data to the GPU
void uploadMeshBuffers(const Mesh *mesh) {
	// Upload vertex data
	glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mesh->num_vertices, mesh->vertices, GL_STATIC_DRAW);

	// Upload index data
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 3 * mesh->num_faces, mesh->faces, GL_STATIC_DRAW);
}

void swapCursorInputMode(GLFWwindow* window) {
	MouseInfo *mouse = &(global_scene.mouse);
	float now = glfwGetTime();
	if (now - mouse->lastModeSwitchTime < mouse->modeSwitchCooldown) return;

	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetCursorPosCallback(window, NULL);
		mouse->lastModeSwitchTime = now;
		mouse->firstMouse = true;
	} else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouse_callback);
		mouse->lastModeSwitchTime = now;
	}
}

void panCamera(GLFWwindow* window, float angle) {
	Camera* camera = &(global_scene.camera);

	camera->yaw += angle;

	float direction[3];
	direction[0] = cos(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	direction[1] = sin(radiansf(camera->pitch));
	direction[2] = sin(radiansf(camera->yaw)) * cos(radiansf(camera->pitch));
	normalize_in_place(direction);
	camera->front = glm::vec3(direction[0], direction[1], direction[2]);
}

void processInput(GLFWwindow* window, float deltaTime) {
	Camera *camera = &(global_scene.camera);
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera->pos += camera->front * camera->speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera->pos += glm::cross(camera->up, camera->front) * camera->speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		camera->pos += glm::vec3(0.0f, 1.0f, 0.0f) * camera->speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera->pos -= camera->front * camera->speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera->pos -= glm::cross(camera->up, camera->front) * camera->speed * deltaTime;
		
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
		camera->pos -= glm::vec3(0.0f, 1.0f, 0.0f) * camera->speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
		swapCursorInputMode(window);

	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		camera->speed = camera->TOP_MOVE_SPEED;
	} else {
		camera->speed = camera->NORMAL_MOVE_SPEED;
	}

	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
		panCamera(window, -camera->PAN_SPEED * deltaTime);
	}

	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
		panCamera(window, camera->PAN_SPEED * deltaTime);
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

void initCamera(Camera& camera) {
	camera.pos   = glm::vec3(0.0f, 0.0f,  3.0f);
	camera.front = glm::vec3(0.0f, 0.0f, -1.0f);
	camera.up    = glm::vec3(0.0f, 1.0f,  0.0f);

	camera.pitch = 0.0f;
	camera.yaw = -90.0f;
	camera.speed = 0.0f;

	camera.speed = 5.0f;
	camera.TOP_MOVE_SPEED = 50.0f;
	camera.NORMAL_MOVE_SPEED = 5.0f;

	camera.PAN_SPEED = 90.0f;
}

void initLightSource(LightSource& lightSource) {
	lightSource.pos = glm::vec3(5.0f, 5.0f, 15.0f);
	lightSource.color = glm::vec3(1.0f, 1.0f, 1.0f);
}

void initMouseInfo(MouseInfo& mouse) { 
	mouse.lastX = 400;
	mouse.lastY = 300;
	mouse.sensitivity = 0.1f;
	mouse.firstMouse = true;

	mouse.lastModeSwitchTime = 0.0f;
	mouse.modeSwitchCooldown = 0.1f;
}

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
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float))); // Color
	//glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float))); // Vector normals
	glEnableVertexAttribArray(1);

	// Unbind this VAO
	glBindVertexArray(0);

	return 0;
}

// hardcodes which mesh to load
void initMeshInstance(MeshInstance* meshInstance) {
	meshInstance->mesh = global_resource_pool.meshes[0]; // teapot.obj
	meshInstance->model = glm::mat4(1.0f);
	meshInstance->color = glm::vec3(1.0f, 1.0f, 1.0f);
}

// this should probably be refactored
// this hardcodes a list of filenames
// for each item in the list:
//     1. malloc+init a new mesh
//     2. malloc mesh fields and load data from file
//     3. register mesh to resource pool list
//     4. upload mesh data to GPU buffers
// return total count of meshes in resource pool
int initGlobalResourcePoolMallocMeshAndMeshFields() {
	int num_meshes = 4;
	const char *list_of_meshes[] = {
		"resources/teapot.obj",
		"resources/teapot2.obj",
		"resources/guy.obj",
		"resources/large_files/HP_Portrait.obj"
	};
	for (int i = 0; i < num_meshes; i++) {
		Mesh* mesh = (Mesh*)malloc(sizeof(Mesh));
		initMesh(mesh);
		printf("loading mesh from file: %s\n", list_of_meshes[i]);
		tic();
		malloc_mesh_fields_from_obj_file(list_of_meshes[i], mesh);
		printf("  TIME LOAD %s: %.6f ms\n", list_of_meshes[i], toc());
		// if we couldn't load normnals from file, then compute them now
		// @TODO: write normals back to file?
		tic();
		if (!mesh->has_normals) {
			printf("Normals not found. Computing normals and rebuilding mesh.\n");
			compute_and_store_vector_normals(mesh);
		}
		printf("  TIME COMPUTE NORMALS %s: %.6f ms\n", list_of_meshes[i], toc());
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

	// init glfw
    if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return -1; }
	
	// Reqest OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	// create glfw window
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW OpenGL", NULL, NULL);
    if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return -1; }

	// make window's opengl context current
    glfwMakeContextCurrent(window);

	// initialize glad after context is current
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { fprintf(stderr, "Failed to initialize GLAD\n"); return -1; }

	// called when window size changes
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// initialize
	initOpenGL();
	initGlobalScene();

	int num_meshes = initGlobalResourcePoolMallocMeshAndMeshFields();

	int num_models = __MAX_MODELS__;
	MeshInstance* modelpool[__MAX_MODELS__];
	for (int i = 0; i < num_models; i++) {
		modelpool[i] = (MeshInstance*)malloc(sizeof(MeshInstance));
		initMeshInstance(modelpool[i]);
		addMeshInstanceToGlobalScene(modelpool[i]);
		modelpool[i]->color = glm::vec3(
			((float)rand() / (float)RAND_MAX) * 0.66f + 0.33f,
			((float)rand() / (float)RAND_MAX) * 0.66f + 0.33f,
			((float)rand() / (float)RAND_MAX) * 0.66f + 0.33f
		);
	}

	// @TEMPORARY: sun object
	MeshInstance sun;
	initMeshInstance(&sun);
	sun.model = glm::translate(glm::mat4(1.0f), global_scene.lightSource.pos);
	sun.color = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::mat4 sunNormal = glm::mat3(glm::transpose(glm::inverse(sun.model)));
	
	// init vars for fps counter
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
	
	// get some info
    const char* glVersion = (const char*)glGetString(GL_VERSION);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);

	// vertex shaders defines the vertex positions on the screen
	// vertex information is interpolated to individual pixel information "fragments"
	// fragment shaders define transformations on interpolated fragments
	// these char* need to be freed
	const char* vertexShaderSource = loadShaderSource("src/shaders/basicShader.vs");
	const char* fragmentShaderSource = loadShaderSource("src/shaders/basicShader.fs");
	GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

	// get locations to shader uniforms
	unsigned int modelColorLoc = glGetUniformLocation(shaderProgram, "modelColor");
	unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
	unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
	unsigned int viewPos = glGetUniformLocation(shaderProgram, "viewPos");
	unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
	unsigned int normLoc = glGetUniformLocation(shaderProgram, "normal");
	unsigned int hasNormalsLoc = glGetUniformLocation(shaderProgram, "hasNormals");
	unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
	unsigned int projLoc = glGetUniformLocation(shaderProgram, "proj");

	// model, view, proj matrices
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat3 normal; // for normal vertices

	int windowWidth, windowHeight;
	while (!glfwWindowShouldClose(window)) {
		// Track realtime info
		lastTime = currTime;
		currTime = glfwGetTime();
		deltaTime = currTime - lastTime;
		heartBeat += deltaTime;

		// Handle input
		processInput(window, deltaTime);

		// Build and upload the view matrix to the shader
		view = glm::lookAt(global_scene.camera.pos, global_scene.camera.pos + global_scene.camera.front, global_scene.camera.up);
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		// Build and upload the projection matrix to the shader
		glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
		proj = glm::perspective(glm::radians(45.0f), (float)windowWidth / (float)windowHeight, 0.1f, 500.0f);
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

		// send light source to shader
		glUniform3fv(lightPosLoc, 1, glm::value_ptr(global_scene.lightSource.pos));
		glUniform3fv(lightColorLoc, 1, glm::value_ptr(global_scene.lightSource.color));

		// send camera position to shader
		glUniform3fv(viewPos, 1, glm::value_ptr(global_scene.camera.pos));

		// Clear the screen buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Render sun
		glBindVertexArray(sun.mesh->VAO);
		glUniform3fv(modelColorLoc, 1, glm::value_ptr(sun.color));
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sun.model));
		glUniformMatrix3fv(normLoc, 1, GL_FALSE, glm::value_ptr(sunNormal));
		glUniform1i(hasNormalsLoc, false);
		glUseProgram(shaderProgram);
		glDrawElements(GL_TRIANGLES, sun.mesh->num_faces * 3, GL_UNSIGNED_INT, 0);
		// End render sun

		// Loop through the scene, build and upload the model matrix, then draw
		for (int i = 0; i < global_scene.meshInstanceCount; i++) {
			MeshInstance* meshInstance = global_scene.meshInstances[i];
			if (meshInstance == nullptr) continue;
			Mesh* mesh = meshInstance->mesh;
			if (mesh == nullptr) continue;
			// Bind the VAO (restores all attribute and buffer settings)
			glBindVertexArray(mesh->VAO);

			// Update model matrix
			float speed = glm::abs(glm::sin(0.2*i));
			meshInstance->model = glm::mat4(1.0f);
			meshInstance->model = glm::translate(meshInstance->model, glm::vec3((i%45)*5.0f, glm::sin(currTime + i), 5.0*(float)(i/45))); //i % 5)*5.0f));
			meshInstance->model = glm::rotate(meshInstance->model, (float)(currTime * M_PI * speed), glm::vec3(0.0f, 1.0f, 0.0f));

			// upload color vector to the shader
			glUniform3fv(modelColorLoc, 1, glm::value_ptr(meshInstance->color));

			// normal matrix for normal vectors is defined this way
			normal = glm::mat3(glm::transpose(glm::inverse(meshInstance->model)));

			// upload model matrix to the shader
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(meshInstance->model));

			// upload normal matrix to the shader
			glUniformMatrix3fv(normLoc, 1, GL_FALSE, glm::value_ptr(normal));

			// upload has_normals flag to sahder
			glUniform1i(hasNormalsLoc, mesh->has_normals);

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
 
		// to execute every ~second
		if (heartBeat > 1.0f) {
			snprintf(title, sizeof(title), "GLFW OpenGL - [FPS: %.2f] - %s - %s", fps, glVersion, glRenderer);
			glfwSetWindowTitle(window, title);
			heartBeat = 0.0f;
		}
	} // end main render loop

	// cleanup here
	for (int i = 0; i < num_models; i++) {
		free(global_scene.meshInstances[i]);
	}
	free((void*)vertexShaderSource);
	free((void*)fragmentShaderSource);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
 