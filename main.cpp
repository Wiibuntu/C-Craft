#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include "math.h"
#include "shader.h"
#include "cube.h"
#include "camera.h"
#include "texture.h"

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Updated vertex shader: now passes texture coordinates to the fragment shader.
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 MVP;
void main()
{
    gl_Position = MVP * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

// Updated fragment shader: samples from a texture.
const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;
void main()
{
    FragColor = texture(ourTexture, TexCoord);
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
    
    glEnable(GL_DEPTH_TEST);

    // Build and compile shader program
    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Load texture (make sure "texture.png" is in your project directory)
    GLuint textureID = loadTexture("texture.png");
    if (textureID == 0) {
        std::cerr << "Texture failed to load!" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // Generate world geometry: create a larger flat terrain.
    // We'll create a 64 x 64 grid of cubes.
    std::vector<float> worldVertices;
    int worldSize = 64;
    for (int x = -worldSize/2; x < worldSize/2; ++x) {
        for (int z = -worldSize/2; z < worldSize/2; ++z) {
            // Each cube is 1 unit high; place them at y = 0.
            addCube(worldVertices, static_cast<float>(x), 0.0f, static_cast<float>(z));
        }
    }
    
    // Create VAO and VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, worldVertices.size() * sizeof(float),
                 worldVertices.data(), GL_STATIC_DRAW);
    
    // Each vertex now has 5 floats (3 for position, 2 for texture coordinates)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);

    // Set up the camera (player)
    Camera camera;
    // Start the player slightly above ground (e.g. eye height ~1.8 units)
    camera.position = {0.0f, 1.8f, 20.0f};
    camera.yaw = -3.14f/2;  // Facing toward -Z
    camera.pitch = 0.0f;
    
    // For player physics
    float verticalVelocity = 0.0f;
    const float gravity = -9.81f;    // Gravity acceleration
    const float jumpSpeed = 5.0f;      // Jump impulse speed
    const float groundHeight = 1.8f;   // Y position when on the ground

    // Capture the mouse for FPS-style look
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_MOUSEMOTION) {
                float sensitivity = 0.002f;
                camera.yaw   += event.motion.xrel * sensitivity;
                camera.pitch -= event.motion.yrel * sensitivity;
                if (camera.pitch > 1.57f)
                    camera.pitch = 1.57f;
                if (camera.pitch < -1.57f)
                    camera.pitch = -1.57f;
            }
            // Jump on spacebar if on the ground
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    if (camera.position.y <= groundHeight + 0.01f) {
                        verticalVelocity = jumpSpeed;
                    }
                }
            }
        }
        
        // Use keyboard for horizontal (player) movement.
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        float speed = 10.0f * deltaTime;
        // For player movement, ignore pitch: only use yaw.
        Vec3 moveForward = { cos(camera.yaw), 0, sin(camera.yaw) };
        moveForward = normalize(moveForward);
        Vec3 moveRight = normalize(cross(moveForward, {0,1,0}));
        
        if (keystate[SDL_SCANCODE_W])
            camera.position = add(camera.position, multiply(moveForward, speed));
        if (keystate[SDL_SCANCODE_S])
            camera.position = subtract(camera.position, multiply(moveForward, speed));
        if (keystate[SDL_SCANCODE_A])
            camera.position = subtract(camera.position, multiply(moveRight, speed));
        if (keystate[SDL_SCANCODE_D])
            camera.position = add(camera.position, multiply(moveRight, speed));
        
        // Apply gravity to vertical velocity and update camera's y position.
        verticalVelocity += gravity * deltaTime;
        camera.position.y += verticalVelocity * deltaTime;
        if (camera.position.y < groundHeight) {
            camera.position.y = groundHeight;
            verticalVelocity = 0.0f;
        }
        
        // Clear the screen and depth buffer
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);  // Sky-blue background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        
        // Activate texture unit 0 and bind our texture.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // Set the sampler uniform so the shader knows to use texture unit 0.
        GLint texUniformLoc = glGetUniformLocation(shaderProgram, "ourTexture");
        glUniform1i(texUniformLoc, 0);
        
        // Create transformation matrices.
        Mat4 projection = perspectiveMatrix(45.0f * 3.1415926f/180.0f,
                                            static_cast<float>(SCREEN_WIDTH)/SCREEN_HEIGHT,
                                            0.1f, 100.0f);
        // For view direction, include pitch.
        Vec3 viewDirection = { cos(camera.yaw)*cos(camera.pitch), sin(camera.pitch), sin(camera.yaw)*cos(camera.pitch) };
        Vec3 cameraTarget = add(camera.position, viewDirection);
        Mat4 view = lookAtMatrix(camera.position, cameraTarget, {0,1,0});
        Mat4 model = identityMatrix();
        Mat4 pv = multiplyMatrix(projection, view);
        Mat4 mvp = multiplyMatrix(pv, model);
        
        GLint mvpLoc = glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(worldVertices.size() / 5));
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
