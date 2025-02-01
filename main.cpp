#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>         // for floor()
#include "math.h"
#include "shader.h"
#include "cube.h"
#include "camera.h"
#include "texture.h"
#include "noise.h"  // Perlin noise module

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Global constants for world generation and collision detection
const int g_worldSize   = 64;    // The world extends from -32 to +31 in x and z.
const float g_frequency = 0.1f;  // Frequency used in Perlin noise.
const int g_maxHeight   = 8;     // Maximum block column height.

// Player collision dimensions (in world units)
const float playerWidth  = 0.6f;  // Full width of player's collision box.
const float playerHeight = 1.8f;  // Height of player's collision box.

// Helper function: returns true if a block exists at (bx,by,bz).
bool isSolidBlock(int bx, int by, int bz) {
    // Outside our world bounds (in x and z) we assume no block exists.
    if (bx < -g_worldSize/2 || bx >= g_worldSize/2 ||
        bz < -g_worldSize/2 || bz >= g_worldSize/2)
        return false;
    
    // Compute the block column height at (bx, bz) using Perlin noise.
    float noiseValue = perlinNoise(bx * g_frequency, bz * g_frequency);
    float normalized = (noiseValue + 1.0f) / 2.0f; // Map from [-1,1] to [0,1]
    int height = static_cast<int>(normalized * g_maxHeight);
    
    // Blocks exist for y from 0 to height.
    return (by >= 0 && by <= height);
}

// Helper function: returns true if the player's AABB at position 'pos' intersects any block.
bool checkCollision(const Vec3 & pos) {
    float half = playerWidth / 2.0f;
    // Player's bounding box (assuming pos is the player's feet position)
    float minX = pos.x - half;
    float maxX = pos.x + half;
    float minY = pos.y;
    float maxY = pos.y + playerHeight;
    float minZ = pos.z - half;
    float maxZ = pos.z + half;
    
    // Determine the range of block coordinates to check.
    int startX = static_cast<int>(floor(minX));
    int endX   = static_cast<int>(floor(maxX));
    int startY = static_cast<int>(floor(minY));
    int endY   = static_cast<int>(floor(maxY));
    int startZ = static_cast<int>(floor(minZ));
    int endZ   = static_cast<int>(floor(maxZ));
    
    // Check all candidate blocks that might intersect the player's AABB.
    for (int bx = startX; bx <= endX; bx++) {
        for (int by = startY; by <= endY; by++) {
            for (int bz = startZ; bz <= endZ; bz++) {
                if (isSolidBlock(bx, by, bz)) {
                    // Block's AABB: from (bx,by,bz) to (bx+1,by+1,bz+1)
                    float blockMinX = bx;
                    float blockMaxX = bx + 1;
                    float blockMinY = by;
                    float blockMaxY = by + 1;
                    float blockMinZ = bz;
                    float blockMaxZ = bz + 1;
                    
                    bool intersect = (maxX > blockMinX && minX < blockMaxX &&
                                      maxY > blockMinY && minY < blockMaxY &&
                                      maxZ > blockMinZ && minZ < blockMaxZ);
                    if (intersect)
                        return true;
                }
            }
        }
    }
    return false;
}

// Vertex shader: passes position and texture coordinates.
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

// Fragment shader: samples from a texture.
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

    // Request an OpenGL 3.3 Core context.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create window with OpenGL context.
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

    // Initialize GLEW.
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
    
    // Enable vsync for stable timing.
    if (SDL_GL_SetSwapInterval(1) < 0) {
        std::cerr << "Warning: Unable to set VSync! SDL Error: " 
                  << SDL_GetError() << std::endl;
    }
    
    glEnable(GL_DEPTH_TEST);

    // Build and compile shader program.
    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // Load texture (place a file named "texture.png" in your project directory).
    GLuint textureID = loadTexture("texture.png");
    if (textureID == 0) {
        std::cerr << "Texture failed to load!" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // Generate world geometry using Perlin noise.
    // For each (x, z) in the grid, compute a column height and add cubes for that column.
    std::vector<float> worldVertices;
    for (int x = -g_worldSize/2; x < g_worldSize/2; ++x) {
        for (int z = -g_worldSize/2; z < g_worldSize/2; ++z) {
            float noiseValue = perlinNoise(x * g_frequency, z * g_frequency);
            float normalized = (noiseValue + 1.0f) / 2.0f;
            int height = static_cast<int>(normalized * g_maxHeight);
            for (int y = 0; y <= height; ++y) {
                addCube(worldVertices, static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
            }
        }
    }
    
    // Create VAO and VBO for the world geometry.
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, worldVertices.size() * sizeof(float),
                 worldVertices.data(), GL_STATIC_DRAW);
    
    // Each vertex has 5 floats: 3 for position, 2 for texture coordinates.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);

    // Set up the camera (player).
    // For collision purposes, treat camera.position as the player's feet.
    Camera camera;
    camera.position = { 0.0f, 0.0f, 20.0f }; // Start at y = 0 (feet level).
    // The view will be offset by the player's eye height when building the view matrix.
    camera.yaw   = -3.14f / 2;  // Facing toward -Z.
    camera.pitch = 0.0f;
    
    // Player physics.
    float verticalVelocity = 0.0f;
    const float gravity = -9.81f;  // Gravity acceleration.
    const float jumpSpeed = 5.0f;    // Jump impulse speed.

    // Capture the mouse for FPS-style look.
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();

    // Main loop.
    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // Process events.
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
            // Jump: check if there's a solid block immediately below the player's feet.
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    // Check the block directly below (using a small offset).
                    int footX = static_cast<int>(floor(camera.position.x));
                    int footY = static_cast<int>(floor(camera.position.y - 0.1f));
                    int footZ = static_cast<int>(floor(camera.position.z));
                    if (isSolidBlock(footX, footY, footZ)) {
                        verticalVelocity = jumpSpeed;
                    }
                }
            }
        }
        
        // Use keyboard for horizontal (player) movement.
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        float speed = 10.0f * deltaTime;
        // For horizontal movement, ignore pitch and only use yaw.
        Vec3 moveForward = { cos(camera.yaw), 0, sin(camera.yaw) };
        moveForward = normalize(moveForward);
        Vec3 moveRight = normalize(cross(moveForward, {0, 1, 0}));
        
        Vec3 horizontalDelta = {0, 0, 0};
        if (keystate[SDL_SCANCODE_W])
            horizontalDelta = add(horizontalDelta, multiply(moveForward, speed));
        if (keystate[SDL_SCANCODE_S])
            horizontalDelta = subtract(horizontalDelta, multiply(moveForward, speed));
        if (keystate[SDL_SCANCODE_A])
            horizontalDelta = subtract(horizontalDelta, multiply(moveRight, speed));
        if (keystate[SDL_SCANCODE_D])
            horizontalDelta = add(horizontalDelta, multiply(moveRight, speed));
        
        // Attempt horizontal movement.
        Vec3 newPosHoriz = camera.position;
        newPosHoriz.x += horizontalDelta.x;
        newPosHoriz.z += horizontalDelta.z;
        if (!checkCollision(newPosHoriz))
            camera.position.x = newPosHoriz.x,
            camera.position.z = newPosHoriz.z;
        // (If collision occurs, horizontal movement is canceled.)
        
        // Apply gravity to vertical velocity.
        verticalVelocity += gravity * deltaTime;
        float verticalDelta = verticalVelocity * deltaTime;
        Vec3 newPosVert = camera.position;
        newPosVert.y += verticalDelta;
        if (!checkCollision(newPosVert)) {
            camera.position.y = newPosVert.y;
        } else {
            // Collision in vertical movement; cancel vertical velocity.
            verticalVelocity = 0;
        }
        
        // Build the view matrix.
        // Since camera.position represents the player's feet,
        // add an eye offset so that the camera view is from roughly 1.6 units above the feet.
        Vec3 eyePosition = camera.position;
        eyePosition.y += 1.6f;
        Vec3 viewDirection = {
            cos(camera.yaw) * cos(camera.pitch),
            sin(camera.pitch),
            sin(camera.yaw) * cos(camera.pitch)
        };
        Vec3 cameraTarget = add(eyePosition, viewDirection);
        Mat4 view = lookAtMatrix(eyePosition, cameraTarget, {0, 1, 0});
        Mat4 projection = perspectiveMatrix(45.0f * 3.1415926f / 180.0f,
                                             static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT,
                                             0.1f, 100.0f);
        Mat4 model = identityMatrix();
        Mat4 pv = multiplyMatrix(projection, view);
        Mat4 mvp = multiplyMatrix(pv, model);
        
        // Render the scene.
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);  // Sky-blue background.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(shaderProgram);
        // Bind the texture.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLint texUniformLoc = glGetUniformLocation(shaderProgram, "ourTexture");
        glUniform1i(texUniformLoc, 0);
        
        // Pass the MVP matrix.
        GLint mvpLoc = glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(worldVertices.size() / 5));
        glBindVertexArray(0);
        
        SDL_GL_SwapWindow(window);
    }
    
    // Cleanup.
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
