#include <glad/glad.h>
#include <GLFW/glfw3.h>

double GLOBAL_lastTime;
void tic() { GLOBAL_lastTime = glfwGetTime(); }
double toc() { return glfwGetTime() - GLOBAL_lastTime; }