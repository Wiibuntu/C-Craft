#ifndef RENDERER_H
#define RENDERER_H

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Renderer {
public:
    Renderer(int width, int height, const char* title);
    ~Renderer();
    
    bool shouldClose();
    void clear();
    void swapBuffers();

private:
    GLFWwindow* window; // Add window member
};

#endif
