// main.cpp
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <utility>
#include "math.h"
#include "shader.h"
#include "cube.h"      // Contains BlockType enum and addCube(...)
#include "camera.h"
#include "texture.h"
#include "noise.h"     // Provides perlinNoise and fbmNoise

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Global parameters for terrain generation
const int g_maxHeight = 12;  // Maximum terrain height in blocks

// Chunk parameters
const int chunkSize = 16;     // Each chunk is 16 x 16 blocks in x/z
const int renderDistance = 5; // Number of chunks to load in each direction from the player

// Player collision dimensions (in world units)
const float playerWidth  = 0.6f;
const float playerHeight = 1.8f;

// --- Helper Functions ---

// Use fbmNoise for more variety in terrain.
int getTerrainHeightAt(int x, int z) {
    // Adjust the scale (0.01f) as desired for zooming the noise.
    float noiseValue = fbmNoise(x * 0.01f, z * 0.01f, 6, 2.0f, 0.5f);
    float normalized = (noiseValue + 1.0f) / 2.0f;  // Map noise from [-1,1] to [0,1]
    int height = static_cast<int>(normalized * g_maxHeight);
    return height;
}

// For collision and world generation, we assume that a block exists at (bx,by,bz)
// if by is between 0 and the terrain height at (bx, bz).
bool isSolidBlock(int bx, int by, int bz) {
    int terrainHeight = getTerrainHeightAt(bx, bz);
    return (by >= 0 && by <= terrainHeight);
}

// Check collision of the player's axis-aligned bounding box (AABB).
// The player's position is defined as the feet (bottom center) of the player.
bool checkCollision(const Vec3 & pos) {
    float half = playerWidth / 2.0f;
    // Compute AABB bounds
    float minX = pos.x - half;
    float maxX = pos.x + half;
    float minY = pos.y;
    float maxY = pos.y + playerHeight;
    float minZ = pos.z - half;
    float maxZ = pos.z + half;
    
    int startX = static_cast<int>(floor(minX));
    int endX   = static_cast<int>(floor(maxX));
    int startY = static_cast<int>(floor(minY));
    int endY   = static_cast<int>(floor(maxY));
    int startZ = static_cast<int>(floor(minZ));
    int endZ   = static_cast<int>(floor(maxZ));
    
    for (int bx = startX; bx <= endX; bx++) {
        for (int by = startY; by <= endY; by++) {
            for (int bz = startZ; bz <= endZ; bz++) {
                if (isSolidBlock(bx, by, bz)) {
                    // Each block occupies [bx, bx+1] x [by, by+1] x [bz, bz+1]
                    if (maxX > bx && minX < bx + 1 &&
                        maxY > by && minY < by + 1 &&
                        maxZ > bz && minZ < bz + 1)
                        return true;
                }
            }
        }
    }
    return false;
}

// --- Chunk System ---

// Define a structure to hold chunk data.
struct Chunk {
    int chunkX, chunkZ;          // Chunk coordinates (in chunk units)
    std::vector<float> vertices; // Vertex data for this chunk (each vertex: 5 floats)
    GLuint VAO, VBO;             // OpenGL buffers for rendering the chunk
};

// Global container to store generated chunks. The key is a pair (chunkX, chunkZ).
std::map<std::pair<int, int>, Chunk> chunks;

// Generate geometry for a given chunk at chunk coordinates (chunkX, chunkZ).
Chunk generateChunk(int chunkX, int chunkZ) {
    Chunk chunk;
    chunk.chunkX = chunkX;
    chunk.chunkZ = chunkZ;
    chunk.vertices.clear();
    
    // Loop over local coordinates within the chunk.
    for (int localX = 0; localX < chunkSize; localX++) {
        for (int localZ = 0; localZ < chunkSize; localZ++) {
            // Convert local coordinates to world coordinates.
            int worldX = chunkX * chunkSize + localX;
            int worldZ = chunkZ * chunkSize + localZ;
            int height = getTerrainHeightAt(worldX, worldZ);
            // For each column, choose block type based on height.
            for (int y = 0; y <= height; y++) {
                BlockType type;
                if (y == height) {
                    type = BLOCK_GRASS;  // Top block is grass.
                } else if ((height - y) <= 6) {
                    type = BLOCK_DIRT;   // Next 6 layers: dirt.
                } else {
                    type = BLOCK_STONE;  // Deeper layers: stone.
                }
                addCube(chunk.vertices, static_cast<float>(worldX),
                        static_cast<float>(y),
                        static_cast<float>(worldZ), type);
            }
        }
    }
    
    // Create VAO and VBO for this chunk.
    glGenVertexArrays(1, &chunk.VAO);
    glGenBuffers(1, &chunk.VBO);
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, chunk.vertices.size() * sizeof(float),
                 chunk.vertices.data(), GL_STATIC_DRAW);
    // Each vertex: 3 floats position, 2 floats UV.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    
    return chunk;
}

// --- Shaders ---
// (Same as before – make sure your shader sources match your texture atlas usage.)
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
    // --- Initialize SDL and OpenGL ---
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    SDL_Window* window = SDL_CreateWindow("Infinite Voxel Engine",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "GLEW Error: " << glewGetErrorString(glewError) << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    if (SDL_GL_SetSwapInterval(1) < 0) {
        std::cerr << "Warning: Unable to set VSync! SDL Error: " << SDL_GetError() << std::endl;
    }
    glEnable(GL_DEPTH_TEST);
    
    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    GLuint textureID = loadTexture("texture.png");
    if (textureID == 0) {
        std::cerr << "Texture failed to load!" << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    // --- Determine a safe spawn position ---
    // For example, use (0,0) in world coordinates.
    int spawnX = 0;
    int spawnZ = 0;
    int terrainHeight = getTerrainHeightAt(spawnX, spawnZ);
    // Place player's feet 1 unit above the terrain.
    Vec3 spawnPos = { static_cast<float>(spawnX), static_cast<float>(terrainHeight + 1), static_cast<float>(spawnZ) };
    while (checkCollision(spawnPos))
        spawnPos.y += 0.1f;
    
    // Set up the camera (player). Note: camera.position is the player's feet.
    Camera camera;
    camera.position = spawnPos;
    camera.yaw   = -3.14f / 2;  // Facing -Z
    camera.pitch = 0.0f;
    
    float verticalVelocity = 0.0f;
    const float gravity = -9.81f;
    const float jumpSpeed = 5.0f;
    
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
    Uint32 lastTime = SDL_GetTicks();
    bool running = true;
    SDL_Event event;
    
    // --- Main Loop ---
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
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    int footX = static_cast<int>(floor(camera.position.x));
                    int footY = static_cast<int>(floor(camera.position.y - 0.1f));
                    int footZ = static_cast<int>(floor(camera.position.z));
                    if (isSolidBlock(footX, footY, footZ))
                        verticalVelocity = jumpSpeed;
                }
            }
        }
        
        // --- Update Player Movement ---
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        float speed = 10.0f * deltaTime;
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
        if (!checkCollision(newPosHoriz)) {
            camera.position.x = newPosHoriz.x;
            camera.position.z = newPosHoriz.z;
        }
        
        // Apply gravity and vertical movement.
        verticalVelocity += gravity * deltaTime;
        float verticalDelta = verticalVelocity * deltaTime;
        Vec3 newPosVert = camera.position;
        newPosVert.y += verticalDelta;
        if (!checkCollision(newPosVert))
            camera.position.y = newPosVert.y;
        else
            verticalVelocity = 0;
        
        // --- Chunk Management ---
        // Determine player's current chunk coordinates.
        int playerChunkX = static_cast<int>(floor(camera.position.x / chunkSize));
        int playerChunkZ = static_cast<int>(floor(camera.position.z / chunkSize));
        // Generate chunks within render distance.
        for (int cx = playerChunkX - renderDistance; cx <= playerChunkX + renderDistance; cx++) {
            for (int cz = playerChunkZ - renderDistance; cz <= playerChunkZ + renderDistance; cz++) {
                std::pair<int, int> key = {cx, cz};
                if (chunks.find(key) == chunks.end()) {
                    Chunk chunk = generateChunk(cx, cz);
                    chunks.insert({key, chunk});
                }
            }
        }
        
        // --- Rendering ---
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f); // Sky-blue background.
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLint texUniformLoc = glGetUniformLocation(shaderProgram, "ourTexture");
        glUniform1i(texUniformLoc, 0);
        
        // Build view-projection matrix.
        // The eye position is offset upward (e.g., 1.6 units) from the player's feet.
        Vec3 eyePosition = camera.position;
        eyePosition.y += 1.6f;
        Vec3 viewDirection = { cos(camera.yaw) * cos(camera.pitch),
                               sin(camera.pitch),
                               sin(camera.yaw) * cos(camera.pitch) };
        Vec3 cameraTarget = add(eyePosition, viewDirection);
        Mat4 view = lookAtMatrix(eyePosition, cameraTarget, {0, 1, 0});
        Mat4 projection = perspectiveMatrix(45.0f * 3.1415926f / 180.0f,
                                             static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT,
                                             0.1f, 100.0f);
        Mat4 pv = multiplyMatrix(projection, view);
        
        // Render each chunk within render distance.
        for (const auto &pair : chunks) {
            int cx = pair.first.first;
            int cz = pair.first.second;
            if (abs(cx - playerChunkX) > renderDistance ||
                abs(cz - playerChunkZ) > renderDistance)
                continue;
            const Chunk &chunk = pair.second;
            // Since vertices are generated in world coordinates, model is identity.
            Mat4 mvp = multiplyMatrix(pv, identityMatrix());
            GLint mvpLoc = glGetUniformLocation(shaderProgram, "MVP");
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
            glBindVertexArray(chunk.VAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(chunk.vertices.size() / 5));
            glBindVertexArray(0);
        }
        
        SDL_GL_SwapWindow(window);
    }
    
    // --- Cleanup ---
    // (For a full implementation, also delete each chunk's VAO/VBO)
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
