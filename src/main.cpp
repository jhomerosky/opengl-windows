#define _USE_MATH_DEFINES
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <cfloat>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Vertex {
	float pos[3];
	float color[3];
	float normal[3] = {0, 0, 0}; // if vnormals are not used, then no contribution to lighting
};

struct Face {
	unsigned int vertexId[3];
};

struct Mesh {
	Vertex* vertices = nullptr;
	Face* faces = nullptr;

	size_t num_vertices = 0;
	size_t num_faces = 0;

	~Mesh() { free(vertices); free(faces); }
};

struct Camera {
	glm::vec3 pos = {0.0f, 0.0f, 0.0f};
	glm::vec3 front = {0.0f, 0.0f, 0.0f};
	glm::vec3 up = {0.0f, 0.0f, 0.0f};

	float pitch = 0;
	float yaw = -90.0f;

	float speed = 0;
};

struct MouseInfo {
	float lastX = 400;
	float lastY = 300;
	float sensitivity = 0.1f;
	bool firstMouse = true;

	float lastModeSwitchTime = 0.0f;
	float modeSwitchCooldown = 0.1f;
};

struct LightSource {
	glm::vec3 pos = {5.0f, 5.0f, 5.0f};
	glm::vec3 color = {1.0f, 1.0f, 1.0f};
};

// struct Scene {
// 	Mesh* meshes = nullptr;
// 	~Scene() { free(meshes); }
// };

// ================ GLOBAL VARS ==================
Camera camera;
MouseInfo mouse;
LightSource lightSource;

// ================ END GLOBAL VARS ==================

// called before the loop 
void initOpenGL() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);  


	glfwSwapInterval(0); // disable vsync
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
}

// ==== MATH LIBRARY ====
float randf() { return (float)2 * (rand() - RAND_MAX/2) / RAND_MAX; }
int isNumber(const char *str) { if (str == NULL || *str == '\0') return 0; char* end; strtol(str, &end, 10); return *end == '\0'; }
// ==== END ====

// function to execute when the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (mouse.firstMouse) {
		mouse.lastX = xpos;
		mouse.lastY = ypos;
		mouse.firstMouse = false;
	}

	float xoffset = xpos - mouse.lastX;
	float yoffset = mouse.lastY - ypos;
	mouse.lastX = xpos;
	mouse.lastY = ypos;

	xoffset *= mouse.sensitivity;
	yoffset *= mouse.sensitivity;

	camera.yaw += xoffset;
	camera.pitch += yoffset;

	if (camera.pitch > 89.0f)
		camera.pitch = 89.0f;
	else if (camera.pitch < -89.0f)
		camera.pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
	direction.y = sin(glm::radians(camera.pitch));
	direction.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
	camera.front = glm::normalize(direction);
}

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
	float scale = factor * 2.0f / std::max(std::max(max_x - min_x, max_y - min_y), max_z - min_z);

	// Recenter and scale vertices
	for (size_t i = 0; i < mesh.num_vertices; ++i) {
		mesh.vertices[i].pos[0] = (mesh.vertices[i].pos[0] - center_x) * scale;
		mesh.vertices[i].pos[1] = (mesh.vertices[i].pos[1] - center_y) * scale;
		mesh.vertices[i].pos[2] = (mesh.vertices[i].pos[2] - center_z) * scale;
	}
}

// @Assuming: vertex_normal[i] = vertex[i] for all i
int malloc_mesh_from_obj_file(const char* filename, Mesh* mesh) {
	if (mesh->vertices != nullptr) { return 0; }
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
		//printf("%s", buf);
		if (buf[0] == '\0' || buf[0] == '#') continue;
		if (buf[0] == 'v' && buf[1] == ' ') {
			if (sscanf(buf, "v %f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
				mesh->vertices[v_index].pos[0] = pos[0];
				mesh->vertices[v_index].pos[1] = pos[1];
				mesh->vertices[v_index].pos[2] = pos[2];
				mesh->vertices[v_index].color[0] = 1.0;
				mesh->vertices[v_index].color[1] = 0.0;
				mesh->vertices[v_index].color[2] = 1.0;
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

	// @Question: should this check be here?
	if (v_index != num_vertices || f_index != num_faces || (vnormals_enabled && vn_index != num_vnormals)) { 
		fprintf(stderr, "Failed to read .obj file\n");
		return -1;
	}

	mesh->num_vertices = num_vertices;
	mesh->num_faces = num_faces;
	return 1;
}

int malloc_mesh_from_random(Mesh* mesh) {
	float color_palette[5][3] = {
		{1.0f, 0.0f, 0.0f}, // red
		{0.0f, 1.0f, 0.0f}, // green
		{0.0f, 0.0f, 1.0f}, // blue
		{1.0f, 1.0f, 0.0f}, // yellow
		{1.0f, 0.0f, 1.0f}  // magenta
	};

	mesh->vertices = (Vertex*)malloc(sizeof(Vertex) * 5);
	mesh->num_vertices = 5;
	for (int i = 0; i < 5; i++) {
		mesh->vertices[i].pos[0] = randf();
		mesh->vertices[i].pos[1] = randf();
		mesh->vertices[i].pos[2] = 0;
		for (int j = 0; j < 3; j++) {
			mesh->vertices[i].color[j] = color_palette[i][j];
		}
	}

	mesh->faces = (Face*)malloc(sizeof(Face) * 2);
	mesh->faces[0] = {1, 2, 3};
	mesh->faces[1] = {3, 4, 5};
	mesh->num_faces = 2;

	return 1;
}

void print_mesh_to_stdout(const Mesh& mesh) {
	printf("Printing mesh:\n");
	if (mesh.vertices == nullptr) { printf("The mesh is uninitialized\n"); }

	for (int i = 0; i < mesh.num_vertices; i++) {
		printf("V[%d] = [ \n", i);
		printf("  pos { ");
		for (int j = 0; j < 3; j++) { printf("%.4f ", mesh.vertices[i].pos[j]); }
		printf("}\n  color { ");
		for (int j = 0; j < 3; j++) { printf("%.4f ", mesh.vertices[i].color[j]); }
		printf("}\n  normal { ");
		for (int j = 0; j < 3; j++) { printf("%.4f ", mesh.vertices[i].normal[j]); }
		printf("} ]\n");
	}

	for (int i = 0; i < mesh.num_faces; i++) {
		printf("F[%d] = [ %d | %d | %d ]\n", i, mesh.faces[i].vertexId[0], mesh.faces[i].vertexId[1], mesh.faces[i].vertexId[2]);
	}

	printf("End mesh printing.\n");
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "Shader Compilation Failed\n%s\n", infoLog);
        return 0;
    }
    return shader;
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "Shader Program Linking Failed\n%s\n", infoLog);
        return 0;
    }

    // Clean up shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

int initMesh(Mesh& mesh) {
	// build mesh
	const char* filename = "resources/teapot.obj";
	if (!malloc_mesh_from_obj_file(filename, &mesh)) { fprintf(stderr, "Failed to malloc the mesh\n"); return -1; } 

	if (mesh.vertices == nullptr) {
		if (!malloc_mesh_from_random(&mesh)) { fprintf(stderr, "Failed to malloc the mesh\n"); return -1; }
	}
	
	//normalize_mesh_to_unit_box(mesh);
	//print_mesh_to_stdout(mesh);

	return 0;
}

int initCamera() {
	camera.pos   = glm::vec3(0.0f, 0.0f,  3.0f);
	camera.front = glm::vec3(0.0f, 0.0f, -1.0f);
	camera.up    = glm::vec3(0.0f, 1.0f,  0.0f);
	camera.speed = 5.0f;
	return 0;
}

void swapCursorInputMode(GLFWwindow* window) {
	float now = glfwGetTime();
	if (now - mouse.lastModeSwitchTime < mouse.modeSwitchCooldown) return;

	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		glfwSetCursorPosCallback(window, NULL);
		mouse.lastModeSwitchTime = now;
		mouse.firstMouse = true;
	} else {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouse_callback);
		mouse.lastModeSwitchTime = now;
	}
}

void processInput(GLFWwindow* window, float deltaTime) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.pos += camera.front * camera.speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.pos += glm::cross(camera.up, camera.front) * camera.speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_APOSTROPHE) == GLFW_PRESS)
		camera.pos += glm::vec3(0.0f, 1.0f, 0.0f) * camera.speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.pos -= camera.front * camera.speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.pos -= glm::cross(camera.up, camera.front) * camera.speed * deltaTime;
		
	if (glfwGetKey(window, GLFW_KEY_SLASH) == GLFW_PRESS)
		camera.pos -= glm::vec3(0.0f, 1.0f, 0.0f) * camera.speed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
		swapCursorInputMode(window);

}

int main(int argc, char** argv) {	
	srand(time(NULL));

	// init glfw
    if (!glfwInit()) { fprintf(stderr, "Failed to initialize GLFW\n"); return -1; }
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);  // Request OpenGL 3.x
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);  // Request OpenGL 3.3

	// create glfw window
    GLFWwindow* window = glfwCreateWindow(800, 600, "GLFW OpenGL", NULL, NULL);
    if (!window) { fprintf(stderr, "Failed to create GLFW window\n"); glfwTerminate(); return -1; }

	// make window's opengl context current
    glfwMakeContextCurrent(window);

	// initialize glad after context is current
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { fprintf(stderr, "Failed to initialize GLAD\n"); return -1; }

	// called when window size changes
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// capture cursor
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// init
	initOpenGL();

	Mesh mesh;
	initMesh(mesh);
	initCamera();

	// init vars for fps counter
	double lastTime;
	double currTime = glfwGetTime();
	double deltaTime;
	double fpsWindowTimeStart;
	double fpsWindowTimeEnd = glfwGetTime();
	unsigned int frameCount = 0;
	int FRAMES_TO_COUNT = 60;
	float fps;
	char title[512];
	
    const char* glVersion = (const char*)glGetString(GL_VERSION);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);

	// Define VAO, VBO, EBO
	GLuint VAO, VBO, EBO;

	// Generate and bind VAO (Vertex Array Object)
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Generate and bind VBO (Vertex Buffer Object)
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	// Generate and bind EBO (Element Buffer Object)
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

	// Upload vertex data
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mesh.num_vertices, mesh.vertices, GL_STATIC_DRAW);

	// Upload index data
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * 3 * mesh.num_faces, mesh.faces, GL_STATIC_DRAW);

	// Configure vertex attributes
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); // Position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float))); // Color
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float))); // Vector normals
	glEnableVertexAttribArray(2);

	const char* vertexShaderSource = R"(
		#version 330 core
		layout(location = 0) in vec3 position;
		layout(location = 1) in vec3 color;
		layout(location = 2) in vec3 vnormal;

		uniform mat4 model;
		uniform mat4 view;
		uniform mat4 proj;
		uniform mat3 normal;

		out vec3 fragPosition;
		out vec3 fragColor;
		out vec3 fragNormal;
		void main() {
			fragPosition = vec3(model * vec4(position, 1.0));
			gl_Position = proj * view * vec4(fragPosition, 1.0);
			
			fragColor = color;
			fragNormal = normal * vnormal;
		}
	)";

	const char* fragmentShaderSource = R"(
		#version 330 core
		in vec3 fragPosition;
		in vec3 fragColor;
		in vec3 fragNormal;

		uniform vec3 lightPos;
		uniform vec3 lightColor;
		uniform vec3 viewPos;
		
		out vec4 color;
		void main() {
			// ambient lighting
			float ambientStrength = 0.1;
			vec3 ambient = ambientStrength * lightColor;

			// diffuse lighting
			vec3 norm = normalize(fragNormal);
			vec3 lightDirection = normalize(lightPos - fragPosition);
			float diff = max(dot(lightDirection, norm), 0.0f);
			vec3 diffuse = diff * lightColor;

			// specular lighting
			float specularStrength = 0.5;
			vec3 viewDirection = normalize(viewPos - fragPosition);
			vec3 reflectDirection = reflect(-lightDirection, norm);
			float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);
    		vec3 specular = specularStrength * spec * lightColor;

			// set the final color
			color = vec4((ambient + diffuse + specular) * fragColor, 1.0);
		}
	)";

	GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

	// model, view, proj matrices
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat3 normal;

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	while (!glfwWindowShouldClose(window)) {
		lastTime = currTime;
		currTime = glfwGetTime();
		deltaTime = currTime - lastTime;
		// Handle input
		processInput(window, deltaTime);

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Bind the VAO (restores all attribute and buffer settings)
		glBindVertexArray(VAO);

		// Update model matrix
		float scale = 0.10;
		// model = glm::scale(glm::mat4(1.0f), scale * glm::vec3(1.0f, 1.0f, 1.0f));
		// model = glm::translate(model, glm::vec3(0.0f, -10.0f, 0.0f));
		model = glm::mat4(1.0f);
		model = glm::rotate(model, (float)(glfwGetTime() * M_PI * 0.25f), glm::vec3(0.0f, 1.0f, 0.0f));

		// normal matrix for normal vectors is defined this way
		normal = glm::mat3(glm::transpose(glm::inverse(model)));

		// Update view matrix
		view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);

		// Update projection matrix
		glfwGetFramebufferSize(window, &width, &height);
		float aspect = (float)width / (float)height;
		glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

		// send m,v,p matrices to the shader
		unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

		unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		unsigned int projLoc = glGetUniformLocation(shaderProgram, "proj");
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(proj));

		unsigned int normLoc = glGetUniformLocation(shaderProgram, "normal");
		glUniformMatrix3fv(normLoc, 1, GL_FALSE, glm::value_ptr(normal));

		// send light source to shader
		unsigned int lightPosLoc = glGetUniformLocation(shaderProgram, "lightPos");
		glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightSource.pos));
		unsigned int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
		glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightSource.color));

		// send camera position to shader
		unsigned int viewPos = glGetUniformLocation(shaderProgram, "viewPos");
		glUniform3fv(viewPos, 1, glm::value_ptr(camera.pos));

		// Issue a draw call
		glDrawElements(GL_TRIANGLES, mesh.num_faces * 3, GL_UNSIGNED_INT, 0);
		glUseProgram(shaderProgram);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
		
		// fps counter implementation
		frameCount = (frameCount + 1) % FRAMES_TO_COUNT;
		if (!frameCount) {
			fpsWindowTimeStart = fpsWindowTimeEnd;
			fpsWindowTimeEnd = currTime;
			fps = FRAMES_TO_COUNT / (fpsWindowTimeEnd - fpsWindowTimeStart);
			snprintf(title, sizeof(title), "GLFW OpenGL - [FPS: %.2f] - %s - %s", fps, glVersion, glRenderer);
			glfwSetWindowTitle(window, title);
		}
		

	} // end main render loop
	
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

