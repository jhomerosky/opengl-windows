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


struct Vertex {
	float pos[3];
	float color[3];
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

// struct Scene {
// 	Mesh* meshes = nullptr;
// 	~Scene() { free(meshes); }
// };

// called before the loop 
void initOpenGL() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glfwSwapInterval(0); // disable vsync
}

// ==== MATH LIBRARY ====
float randf() { return (float)2 * (rand() - RAND_MAX/2) / RAND_MAX; }
int isNumber(const char *str) { if (str == NULL || *str == '\0') return 0; char* end; strtol(str, &end, 10); return *end == '\0'; }
// ==== END ====

// function to execute when the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}

void normalize_mesh_to_unit_box(Mesh& mesh) {
	float min_x = FLT_MAX, max_x = -FLT_MAX;
	float min_y = FLT_MAX, max_y = -FLT_MAX;
	float min_z = FLT_MAX, max_z = -FLT_MAX;

	for (size_t i = 0; i < mesh.num_vertices; ++i) {
		float* p = mesh.vertices[i].pos;
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
	float scale = 2.0f / std::max(std::max(max_x - min_x, max_y - min_y), max_z - min_z);

	// Recenter and scale vertices
	for (size_t i = 0; i < mesh.num_vertices; ++i) {
		mesh.vertices[i].pos[0] = (mesh.vertices[i].pos[0] - center_x) * scale;
		mesh.vertices[i].pos[1] = (mesh.vertices[i].pos[1] - center_y) * scale;
		mesh.vertices[i].pos[2] = (mesh.vertices[i].pos[2] - center_z) * scale - 1.0f;
	}
}

int malloc_mesh_from_obj_file(const char* filename, Mesh* mesh) {
	if (mesh->vertices != nullptr) { return 0; }
	size_t num_vertices = 0;
	size_t num_faces = 0;

	char buf[2048];
	FILE* file = fopen(filename, "r");
	while (fgets(buf, 2048, file) != NULL) {
		//printf("%s", buf);
		if (buf[0] == '\0' || buf[0] == '#') continue;
		if (buf[0] == 'v' && buf[1] == ' ') ++num_vertices;
		if (buf[0] == 'f') ++num_faces;
	}
	rewind(file);

	mesh->vertices = (Vertex*)malloc(sizeof(Vertex)*num_vertices);
	mesh->faces = (Face*)malloc(sizeof(Face)*num_faces);

	size_t v_index = 0;
	size_t f_index = 0;
	float pos[3];
	unsigned int v[3], n[3];
	while (fgets(buf, 2048, file) != NULL) {
		//printf("%s", buf);
		if (buf[0] == '\0' || buf[0] == '#') continue;
		if (buf[0] == 'v' && buf[1] == ' ') {
			if (sscanf(buf, "v %f %f %f", &pos[0], &pos[1], &pos[2]) == 3) {
				mesh->vertices[v_index].pos[0] = pos[0];
				mesh->vertices[v_index].pos[1] = pos[1];
				mesh->vertices[v_index].pos[2] = pos[2];
				mesh->vertices[v_index].color[0] = 1.0;
				mesh->vertices[v_index].color[1] = 1.0;
				mesh->vertices[v_index].color[2] = 1.0;
				++v_index;
			}
		}
		if (buf[0] == 'f') {
			if (sscanf(buf, "f %u//%u %u//%u %u//%u", &v[0], &n[0], &v[1], &n[1], &v[2], &n[2]) == 6) {
				mesh->faces[f_index].vertexId[0] = v[0];
				mesh->faces[f_index].vertexId[1] = v[1];
				mesh->faces[f_index].vertexId[2] = v[2];
				++f_index;
			}
		}
	}

	// @Question: should this check be here?
	if (v_index != num_vertices || f_index != num_faces) { fprintf(stderr, "Failed to read .obj file\n"); return -1; }

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
	
	normalize_mesh_to_unit_box(mesh);
	//print_mesh_to_stdout(mesh);

	return 0;
	
}

int main(int argc, char** argv) {	
	srand(time(NULL));
	// handle input args
	int _FPS_COUNTER_ = 0;
	for (int i = 1; i < argc; i++) {
		// enable fps counter
		if (strcmp(argv[i], "-fps") == 0) _FPS_COUNTER_ = 1;
	}

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
	
	// init
	initOpenGL();

	Mesh mesh;
	initMesh(mesh);

	// init vars for fps counter
	double lastTime = glfwGetTime();
	double currTime;
	unsigned int frameCount = 0;
	int FRAMES_TO_COUNT = 50;

	// Define VAO, VBO, EBO
	GLuint VAO, VBO, EBO;

	// Generate and bind VAO
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Generate and bind VBO
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	// Generate and bind EBO
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

	const char* vertexShaderSource = R"(
		#version 330 core
		layout(location = 0) in vec3 position;
		layout(location = 1) in vec3 color;
		out vec3 vertexColor;
		void main() {
			gl_Position = vec4(position, 1.0);
			vertexColor = color;
		}
	)";

	const char* fragmentShaderSource = R"(
		#version 330 core
		in vec3 vertexColor;
		out vec4 color;
		void main() {
			color = vec4(vertexColor, 1.0);  // Set the final color (with alpha of 1)
		}
	)";

	GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

	while (!glfwWindowShouldClose(window)) {

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Bind the VAO (restores all attribute and buffer settings)
		glBindVertexArray(VAO);

		// Issue a draw call
		glDrawElements(GL_TRIANGLES, mesh.num_faces * 3, GL_UNSIGNED_INT, 0);
		glUseProgram(shaderProgram);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();
		
		// fps counter implementation
		frameCount = (frameCount + 1) % FRAMES_TO_COUNT;
		if (frameCount == 0 && _FPS_COUNTER_) {
		    lastTime = currTime;
            currTime = glfwGetTime();
			printf("fps=%.1f\n", FRAMES_TO_COUNT / (currTime - lastTime));
		}
	} // end main render loop
	
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

