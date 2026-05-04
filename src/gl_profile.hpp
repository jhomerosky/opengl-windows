#ifndef __my_gl_profile__
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// @TODO: cleanup
// use glfwGetTime() to store a global time
// tic() updates global time
// toc() returns time since global time

double GLOBAL_lastTime;
void tic() { GLOBAL_lastTime = glfwGetTime(); }
double toc() { return (glfwGetTime() - GLOBAL_lastTime) * 1000.0; }

struct gl_timer {
    double start_time;
    double last_duration;
};

static inline gl_timer get_gl_timer() {
    gl_timer timer;
    timer.start_time = glfwGetTime();
    timer.last_duration = 0.0;
    return timer;
}

static inline void reset_gl_time(gl_timer *timer) {
    timer->start_time = glfwGetTime();
    timer->last_duration = 0.0;
}

static inline double gl_time(gl_timer *timer) {
    timer->last_duration = (glfwGetTime() - timer->start_time) * 1000.0;
    return timer->last_duration;
}

static inline void print_duration(gl_timer *timer) {
    printf("[%.3f]", timer->last_duration);
}

static inline void gl_timer_println(gl_timer *timer, const char *msg) {
    timer->last_duration = (glfwGetTime() - timer->start_time) * 1000.0;
    printf("[%s: %.3f ms]\n", msg, timer->last_duration);
}

#define __my_gl_profile__
#endif