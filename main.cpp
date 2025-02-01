// main.cpp
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>
#include <cstdlib>  // for rand()
#include "math.h"
#include "shader.h"
#include "cube.h"      // Updated: defines BlockType {BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_SAND, BLOCK_TREE_LOG, BLOCK_LEAVES}
#include "camera.h"
#include "texture.h"
#include "noise.h"     // Provides perlinNoise() and fbmNoise()

// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Global terrain parameters
const int g_maxHeight = 12;  // Maximum terrain height (in blocks)

// Chunk parameters
const int chunkSize = 16;     // Each chunk is 16x16 blocks (x/z)
const int renderDistance = 5; // In chunks

// Player collision dimensions (in world units)
const float playerWidth  = 0.6f;
const float playerHeight = 1.8f;

// Forward declaration so that getTerrainHeightAt can be used in functions that come before its definition.
int getTerrainHeightAt(int x, int z);


// --- Biome Definitions ---
enum Biome { BIOME_PLAINS, BIOME_DESERT, BIOME_EXTREME_HILLS, BIOME_FOREST };

// Determine the biome at (x,z) using a very low-frequency Perlin noise value.
Biome getBiome(int x, int z) {
    float n = perlinNoise(x * 0.001f, z * 0.001f);
    // Example thresholds:
    if (n < -0.5f)
        return BIOME_DESERT;
    else if (n > 0.5f)
        return BIOME_EXTREME_HILLS;
    else if (n >= -0.2f && n <= 0.2f)
        return BIOME_FOREST;
    else
        return BIOME_PLAINS;
}

// --- Global Extra Blocks for Collision ---
// For blocks not part of the terrain height (e.g., trees)
std::map<std::tuple<int,int,int>, BlockType> extraBlocks;

// Helper: Return true if the block type should have collision.
bool blockHasCollision(BlockType type) {
    // Let leaves be non-solid (the player can move through them).
    return (type != BLOCK_LEAVES);
}

// --- Collision Function ---
// Check if a block exists at (bx,by,bz) either as part of the procedural terrain or
// in the extraBlocks map (for trees, etc.).
bool isSolidBlock(int bx, int by, int bz) {
    std::tuple<int,int,int> key(bx, by, bz);
    if (extraBlocks.find(key) != extraBlocks.end()) {
        BlockType type = extraBlocks[key];
        if (blockHasCollision(type))
            return true;
    }
    // For terrain, assume blocks exist from y = 0 up to the terrain height.
    int terrainHeight = getTerrainHeightAt(bx, bz);
    return (by >= 0 && by <= terrainHeight);
}

// --- Collision Check for the Player ---
// The player's position represents the feet (bottom center).
bool checkCollision(const Vec3 & pos) {
    float half = playerWidth / 2.0f;
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

// --- Terrain Height Function ---
// Returns the terrain height at (x,z) based on biome and noise.
int getTerrainHeightAt(int x, int z) {
    Biome biome = getBiome(x, z);
    float noiseValue;
    if (biome == BIOME_EXTREME_HILLS) {
        noiseValue = fbmNoise(x * 0.005f, z * 0.005f, 6, 2.0f, 0.5f);
    } else {
        noiseValue = fbmNoise(x * 0.01f, z * 0.01f, 6, 2.0f, 0.5f);
    }
    float normalized = (noiseValue + 1.0f) / 2.0f;
    int height = static_cast<int>(normalized * g_maxHeight);
    return height;
}

// --- Chunk System ---
struct Chunk {
    int chunkX, chunkZ;          // Chunk coordinates (in chunk units)
    std::vector<float> vertices; // Geometry data (each vertex: 5 floats: 3 position, 2 UV)
    GLuint VAO, VBO;
};

std::map<std::pair<int, int>, Chunk> chunks;

// Generate geometry for a given chunk at (chunkX, chunkZ)
Chunk generateChunk(int chunkX, int chunkZ) {
    Chunk chunk;
    chunk.chunkX = chunkX;
    chunk.chunkZ = chunkZ;
    chunk.vertices.clear();
    
    // Loop over local positions within the chunk.
    for (int localX = 0; localX < chunkSize; localX++) {
        for (int localZ = 0; localZ < chunkSize; localZ++) {
            int worldX = chunkX * chunkSize + localX;
            int worldZ = chunkZ * chunkSize + localZ;
            Biome biome = getBiome(worldX, worldZ);
            int height = getTerrainHeightAt(worldX, worldZ);
            
            // --- Terrain Generation ---
            // For non-desert biomes, use grass on top, then dirt (up to 6 layers), then stone.
            // For desert, the top two layers are sand; below that, stone.
            for (int y = 0; y <= height; y++) {
                BlockType type;
                if (biome == BIOME_DESERT) {
                    if (y >= height - 1)
                        type = BLOCK_SAND;
                    else
                        type = BLOCK_STONE;
                } else { // For Plains, Extreme Hills, and Forest
                    if (y == height)
                        type = BLOCK_GRASS;
                    else if ((height - y) <= 6)
                        type = BLOCK_DIRT;
                    else
                        type = BLOCK_STONE;
                }
                addCube(chunk.vertices, static_cast<float>(worldX),
                        static_cast<float>(y),
                        static_cast<float>(worldZ), type);
            }
            
            // --- Tree Generation ---
            // Only generate trees in forest biome.
            if (biome == BIOME_FOREST) {
                // Use a lower chance (e.g., 1 in 50).
                if (rand() % 50 == 0) {
                    int trunkHeight = 4 + (rand() % 3);  // 4 to 6 blocks tall
                    // Generate trunk (tree logs)
                    for (int ty = height + 1; ty <= height + trunkHeight; ty++) {
                        addCube(chunk.vertices, static_cast<float>(worldX),
                                static_cast<float>(ty),
                                static_cast<float>(worldZ), BLOCK_TREE_LOG);
                        extraBlocks[std::make_tuple(worldX, ty, worldZ)] = BLOCK_TREE_LOG;
                    }
                    // Generate leaves around the top of the trunk.
                    int topY = height + trunkHeight;
                    // Create a 3x3 layer around the trunk (excluding the trunk itself)
                    for (int lx = worldX - 1; lx <= worldX + 1; lx++) {
                        for (int lz = worldZ - 1; lz <= worldZ + 1; lz++) {
                            if (lx == worldX && lz == worldZ)
                                continue; // skip trunk position
                            addCube(chunk.vertices, static_cast<float>(lx),
                                    static_cast<float>(topY),
                                    static_cast<float>(lz), BLOCK_LEAVES);
                            extraBlocks[std::make_tuple(lx, topY, lz)] = BLOCK_LEAVES;
                        }
                    }
                    // Optionally add a crown block one level higher at the center.
                    addCube(chunk.vertices, static_cast<float>(worldX),
                            static_cast<float>(topY + 1),
                            static_cast<float>(worldZ), BLOCK_LEAVES);
                    extraBlocks[std::make_tuple(worldX, topY + 1, worldZ)] = BLOCK_LEAVES;
                }
            }
        }
    }
    
    // Create OpenGL buffers for the chunk.
    glGenVertexArrays(1, &chunk.VAO);
    glGenBuffers(1, &chunk.VBO);
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, chunk.vertices.size() * sizeof(float),
                 chunk.vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    
    return chunk;
}

// --- Shaders ---
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

// --- Main Function ---
int main(int argc, char* argv[]) {
    // Initialize SDL and create OpenGL context.
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    SDL_Window* window = SDL_CreateWindow("Infinite Voxel Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
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
    if (SDL_GL_SetSwapInterval(1) < 0)
        std::cerr << "Warning: Unable to set VSync! SDL Error: " << SDL_GetError() << std::endl;
    
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
    
    // --- Determine a Safe Spawn Position ---
// For example, choose world coordinates (0,0) for the spawn X/Z.
int spawnX = 0;
int spawnZ = 0;

// Ensure that the spawn chunk is generated first.
int spawnChunkX = spawnX / chunkSize;
int spawnChunkZ = spawnZ / chunkSize;
std::pair<int,int> spawnChunkKey = {spawnChunkX, spawnChunkZ};
if (chunks.find(spawnChunkKey) == chunks.end()) {
    Chunk spawnChunk = generateChunk(spawnChunkX, spawnChunkZ);
    chunks.insert({spawnChunkKey, spawnChunk});
}

    // Get the terrain height at the spawn coordinates.
    int terrainHeight = getTerrainHeightAt(spawnX, spawnZ);

    // Start with a spawn height that is several units above the terrain.
    float spawnHeightOffset = 3.0f;  // This makes the player fall onto the world.
    Vec3 spawnPos = { static_cast<float>(spawnX),
                  static_cast<float>(terrainHeight) + spawnHeightOffset,
                  static_cast<float>(spawnZ) };

    // Raise the spawn position until there is no collision.
    while (checkCollision(spawnPos)) {
    spawnPos.y += 0.2f;  // Increase by 0.2 units each time.
}

    // Now, initialize the camera (player) with this safe spawn position.
    // Note: camera.position represents the player's feet.
    Camera camera;
    camera.position = spawnPos;
    camera.yaw   = -3.14f / 2;  // Facing -Z.
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
        
        Vec3 newPosHoriz = camera.position;
        newPosHoriz.x += horizontalDelta.x;
        newPosHoriz.z += horizontalDelta.z;
        if (!checkCollision(newPosHoriz)) {
            camera.position.x = newPosHoriz.x;
            camera.position.z = newPosHoriz.z;
        }
        
        verticalVelocity += gravity * deltaTime;
        float verticalDelta = verticalVelocity * deltaTime;
        Vec3 newPosVert = camera.position;
        newPosVert.y += verticalDelta;
        if (!checkCollision(newPosVert))
            camera.position.y = newPosVert.y;
        else
            verticalVelocity = 0;
        
        // --- Chunk Management ---
        int playerChunkX = static_cast<int>(floor(camera.position.x / chunkSize));
        int playerChunkZ = static_cast<int>(floor(camera.position.z / chunkSize));
        for (int cx = playerChunkX - renderDistance; cx <= playerChunkX + renderDistance; cx++) {
            for (int cz = playerChunkZ - renderDistance; cz <= playerChunkZ + renderDistance; cz++) {
                std::pair<int,int> key = {cx, cz};
                if (chunks.find(key) == chunks.end()) {
                    Chunk chunk = generateChunk(cx, cz);
                    chunks.insert({key, chunk});
                }
            }
        }
        
        // --- Rendering ---
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLint texUniformLoc = glGetUniformLocation(shaderProgram, "ourTexture");
        glUniform1i(texUniformLoc, 0);
        
        Vec3 eyePosition = camera.position;
        eyePosition.y += 1.6f;  // Eye level offset.
        Vec3 viewDirection = { cos(camera.yaw) * cos(camera.pitch),
                               sin(camera.pitch),
                               sin(camera.yaw) * cos(camera.pitch) };
        Vec3 cameraTarget = add(eyePosition, viewDirection);
        Mat4 view = lookAtMatrix(eyePosition, cameraTarget, {0, 1, 0});
        Mat4 projection = perspectiveMatrix(45.0f * 3.1415926f / 180.0f,
                                             static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT,
                                             0.1f, 100.0f);
        Mat4 pv = multiplyMatrix(projection, view);
        
        for (const auto &pair : chunks) {
            int cx = pair.first.first;
            int cz = pair.first.second;
            if (abs(cx - playerChunkX) > renderDistance ||
                abs(cz - playerChunkZ) > renderDistance)
                continue;
            const Chunk &chunk = pair.second;
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
    // (For a full implementation, you should also delete each chunk's VAO/VBO.)
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
