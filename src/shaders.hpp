#include <glad/glad.h>
#include <stdlib.h>
#include <stdio.h>

// ===== header =====
const char* loadShaderSource(const char* filename);
GLuint compileShader(GLenum type, const char* source);
// ===== end header =====

const char* loadShaderSource(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", filename);
        return NULL;
    }

    // Seek to end to get file size
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    // Allocate buffer (+1 for null terminator)
    char* buffer = (char*)malloc(sizeof(char) * (length + 1));
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    // Read file into buffer
    size_t read_size = fread(buffer, 1, length, file);
    buffer[read_size] = '\0'; // Null-terminate

    fclose(file);
    return buffer;
}

// Compile the shader
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
        return 0;
    }

    // clean up shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}