#ifndef __my_shaders__

#include <glad/glad.h>
#include <stdlib.h>
#include <stdio.h>

#include "file_utils.hpp"

// ===== header =====
GLuint compileShader(GLenum type, const char* source);
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);
GLuint loadShader(char* vertexShaderSource, char* fragmentShaderSource);
// ===== end header =====

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

#define __my_shaders__
#endif