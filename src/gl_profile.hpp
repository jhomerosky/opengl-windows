#include <glad/glad.h>
#include <GLFW/glfw3.h>

// use glfwGetTime() to store a global time
// tic() updates global time
// toc() returns time since global time

double GLOBAL_lastTime;
void tic() { GLOBAL_lastTime = glfwGetTime(); }
double toc() { return (glfwGetTime() - GLOBAL_lastTime) * 1000.0; }