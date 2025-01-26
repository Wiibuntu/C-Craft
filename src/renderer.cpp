#include "renderer.h"
#include <iostream>
#include <glad/glad.h> // Ensure glad is included before glfw

Renderer::Renderer(int width, int height, const char* title) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Set GLFW window hints for OpenGL version and core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create the GLFW window
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Make the window's OpenGL context current
    glfwMakeContextCurrent(window);

    // Load OpenGL functions using glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Set viewport
    glViewport(0, 0, width, height);

    // Enable V-Sync
    glfwSwapInterval(1);

    // Print success message
    std::cout << "Renderer initialized successfully." << std::endl;
}

Renderer::~Renderer() {
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "Renderer destroyed and resources released." << std::endl;
}

bool Renderer::shouldClose() {
    return glfwWindowShouldClose(window);
}

void Renderer::clear() {
    // Set a default clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Dark gray
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::swapBuffers() {
    glfwSwapBuffers(window);
}
