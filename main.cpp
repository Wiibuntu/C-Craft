#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include "math.h"
#include "shader.h"
#include "cube.h"
#include "camera.h"

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Vertex shader source code
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 MVP;  // Combined Model-View-Projection matrix
void main() {
    gl_Position = MVP * vec4(aPos, 1.0);
}
)";

// Fragment shader source code
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main() {
    // A simple greenish color for blocks
    FragColor = vec4(0.6, 0.8, 0.3, 1.0);
}
)";

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Could not initialize SDL: " 
                  << SDL_GetError() << std::endl;
        return -1;
    }

    // Request an OpenGL 3.3 Core context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create window with OpenGL context
    SDL_Window* window = SDL_CreateWindow("Voxel Engine",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Failed to create window: " 
                  << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " 
                  << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "GLEW initialization error: " 
                  << glewGetErrorString(glewError) << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // Enable depth testing so nearer objects hide farther ones
    glEnable(GL_DEPTH_TEST);

    // Build and compile our shader program
    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Generate our "world" geometry: a flat 16x16 grid of cubes at y = 0
    std::vector<float> worldVertices;
    int worldSize = 16;
    for (int x = -worldSize/2; x < worldSize/2; ++x) {
        for (int z = -worldSize/2; z < worldSize/2; ++z) {
            addCube(worldVertices, static_cast<float>(x), 0.0f, static_cast<float>(z));
        }
    }
    
    // Create Vertex Array Object (VAO) and Vertex Buffer Object (VBO)
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, worldVertices.size() * sizeof(float),
                 worldVertices.data(), GL_STATIC_DRAW);
    
    // Specify the layout of our vertex data (position only)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);

    // Set up the camera
    Camera camera;
    camera.position = {0.0f, 5.0f, 20.0f};
    camera.yaw = -3.14f/2;  // Initially facing toward -Z
    camera.pitch = 0.0f;
    
    // Capture the mouse for relative movement (mouse look)
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();

    // Main loop
    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // Process events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            // Update camera rotation with mouse motion
            if (event.type == SDL_MOUSEMOTION) {
                float sensitivity = 0.002f;
                camera.yaw   += event.motion.xrel * sensitivity;
                camera.pitch -= event.motion.yrel * sensitivity;
                if (camera.pitch > 1.57f)
                    camera.pitch = 1.57f;
                if (camera.pitch < -1.57f)
                    camera.pitch = -1.57f;
            }
        }
        
        // Keyboard state for smooth movement
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        float speed = 10.0f * deltaTime;
        Vec3 forward = {
            cos(camera.yaw) * cos(camera.pitch),
            sin(camera.pitch),
            sin(camera.yaw) * cos(camera.pitch)
        };
        forward = normalize(forward);
        Vec3 right = normalize(cross(forward, {0,1,0}));
        
        if (keystate[SDL_SCANCODE_W])
            camera.position = add(camera.position, multiply(forward, speed));
        if (keystate[SDL_SCANCODE_S])
            camera.position = subtract(camera.position, multiply(forward, speed));
        if (keystate[SDL_SCANCODE_A])
            camera.position = subtract(camera.position, multiply(right, speed));
        if (keystate[SDL_SCANCODE_D])
            camera.position = add(camera.position, multiply(right, speed));
        if (keystate[SDL_SCANCODE_SPACE])
            camera.position.y += speed;
        if (keystate[SDL_SCANCODE_LCTRL])
            camera.position.y -= speed;
        
        // Clear the screen and depth buffer
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);  // Sky blue background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
        // Create transformation matrices:
        // Projection: perspective projection.
        // View: camera transformation.
        // Model: identity (since cubes are already in world space).
        Mat4 projection = perspectiveMatrix(45.0f * 3.1415926f/180.0f,
                                            static_cast<float>(SCREEN_WIDTH)/SCREEN_HEIGHT,
                                            0.1f, 100.0f);
        Vec3 cameraTarget = add(camera.position, forward);
        Mat4 view = lookAtMatrix(camera.position, cameraTarget, {0,1,0});
        Mat4 model = identityMatrix();
        Mat4 pv = multiplyMatrix(projection, view);
        Mat4 mvp = multiplyMatrix(pv, model);
        
        // Pass the MVP matrix to the shader
        GLint mvpLoc = glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
        
        // Draw the world
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(worldVertices.size() / 3));
        glBindVertexArray(0);
        
        SDL_GL_SwapWindow(window);
    }
    
    // Cleanup
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
