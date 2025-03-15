#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <tuple>
#include <cstdlib>
#include <ctime>

#include "math.h"       // Provides identityMatrix(), multiplyMatrix(), vector math, etc.
#include "shader.h"     // Shader compilation and program creation
#include "cube.h"       // Contains BlockType definitions and addCube()
#include "camera.h"
#include "texture.h"    // loadTexture()
#include "noise.h"
#include "world.h"
#include "inventory.h"
#include "globals.h"

// Global texture variable for the hand.
GLuint handTex = 0;

#ifndef BLOCK_NONE
#define BLOCK_NONE -1
#endif

// Forward declarations for UI functions.
int drawPauseMenu(int screenW, int screenH);
void drawFlyIndicator(bool isFlying, int screenW, int screenH);
void drawFirstPersonHand3D(int screenW, int screenH, const Mat4 &proj); // (unused in new approach)

// --- Helper functions for constructing model matrices ---
Mat4 translateMatrix(float tx, float ty, float tz) {
    Mat4 mat = identityMatrix();
    mat.m[12] = tx;
    mat.m[13] = ty;
    mat.m[14] = tz;
    return mat;
}

Mat4 scaleMatrix(float sx, float sy, float sz) {
    Mat4 mat = identityMatrix();
    mat.m[0]  = sx;
    mat.m[5]  = sy;
    mat.m[10] = sz;
    return mat;
}

// --- Additional helper functions ---
static float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    if(t < 0) t = 0;
    if(t > 1) t = 1;
    return t * t * (3 - 2 * t);
}

static float mix(float a, float b, float t) {
    return a + t * (b - a);
}

// --- Global constant for tick timing ---
static const float TICK_INTERVAL = 0.5f; // seconds per tick

// --- Forward declaration of raycastBlock ---
static bool raycastBlock(const Vec3 &start, const Vec3 &dir, float maxDist, int &outX, int &outY, int &outZ);

// --- Definition of raycastBlock function ---
static bool raycastBlock(const Vec3 &start, const Vec3 &dir, float maxDist, int &outX, int &outY, int &outZ) {
    float step = 0.1f, traveled = 0.0f;
    while(traveled < maxDist) {
        Vec3 pos = add(start, multiply(dir, traveled));
        int bx = (int)std::floor(pos.x);
        int by = (int)std::floor(pos.y);
        int bz = (int)std::floor(pos.z);
        std::tuple<int,int,int> key = {bx, by, bz};
        if(isSolidBlock(bx, by, bz) ||
            (extraBlocks.find(key) != extraBlocks.end() && extraBlocks[key] == BLOCK_LEAVES)) {
            outX = bx; outY = by; outZ = bz;
        return true;
            }
            traveled += step;
    }
    return false;
}

// --- Render the held block as a 3D cube (unchanged) ---
void renderHeldBlock3D(const Mat4 &proj, int activeBlock) {
    Mat4 model = identityMatrix();
    model = multiplyMatrix(model, translateMatrix(0.8f, -0.8f, -1.5f));
    Mat4 rotY = identityMatrix();
    float angle = 0.3f; // radians
    rotY.m[0]  = cos(angle);
    rotY.m[2]  = sin(angle);
    rotY.m[8]  = -sin(angle);
    rotY.m[10] = cos(angle);
    model = multiplyMatrix(model, rotY);
    model = multiplyMatrix(model, scaleMatrix(0.5f, 0.5f, 0.5f));
    Mat4 mvp = multiplyMatrix(proj, model);

    glUseProgram(worldShader);
    GLint mvpLoc = glGetUniformLocation(worldShader, "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    GLint texLoc = glGetUniformLocation(worldShader, "ourTexture");
    glUniform1i(texLoc, 0);

    std::vector<float> verts;
    verts.reserve(36 * 5);
    addCube(verts, 0.0f, 0.0f, 0.0f, (BlockType)activeBlock, false);

    GLuint heldVAO, heldVBO;
    glGenVertexArrays(1, &heldVAO);
    glGenBuffers(1, &heldVBO);
    glBindVertexArray(heldVAO);
    glBindBuffer(GL_ARRAY_BUFFER, heldVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindVertexArray(0);
    glDeleteBuffers(1, &heldVBO);
    glDeleteVertexArrays(1, &heldVAO);
}

// --- Render the hand as a flat 3D rectangle ---
void renderHandRect(const Mat4 &proj) {
    float handVerts[] = {
        // positions       // UVs
        0.0f,  0.0f, 0.0f,   0.0f, 0.0f,
        1.0f,  0.0f, 0.0f,   1.0f, 0.0f,
        1.0f,  1.0f, 0.0f,   1.0f, 1.0f,

        0.0f,  0.0f, 0.0f,   0.0f, 0.0f,
        1.0f,  1.0f, 0.0f,   1.0f, 1.0f,
        0.0f,  1.0f, 0.0f,   0.0f, 1.0f
    };

    Mat4 model = identityMatrix();
    model = multiplyMatrix(model, translateMatrix(0.8f, -0.8f, -0.8f));
    Mat4 rotZ = identityMatrix();
    float angle = 0.2f; // radians
    rotZ.m[0] = cos(angle);
    rotZ.m[1] = -sin(angle);
    rotZ.m[4] = sin(angle);
    rotZ.m[5] = cos(angle);
    model = multiplyMatrix(model, rotZ);
    model = multiplyMatrix(model, scaleMatrix(0.7f, 0.4f, 1.0f));

    Mat4 mvp = multiplyMatrix(proj, model);

    glUseProgram(worldShader);
    GLint mvpLoc = glGetUniformLocation(worldShader, "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, handTex);
    GLint texLoc = glGetUniformLocation(worldShader, "ourTexture");
    glUniform1i(texLoc, 0);

    GLuint handVAO, handVBO;
    glGenVertexArrays(1, &handVAO);
    glGenBuffers(1, &handVBO);
    glBindVertexArray(handVAO);
    glBindBuffer(GL_ARRAY_BUFFER, handVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(handVerts), handVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glDeleteBuffers(1, &handVBO);
    glDeleteVertexArrays(1, &handVAO);
}

// -----------------------------------------------------------------------------
// Global settings and declarations.
int SCREEN_WIDTH  = 960;
int SCREEN_HEIGHT = 480;

static const int chunkSize      = 16;
static const int renderDistance = 10;

static const float playerWidth  = 0.6f;
static const float playerHeight = 1.8f;
static const float WORLD_FLOOR_LIMIT = -10.0f;

static const float GRAVITY    = -9.81f;
static const float JUMP_SPEED =  5.0f;

GLuint worldShader = 0;
GLuint texID       = 0;

GLuint uiShader    = 0;
GLuint uiVAO       = 0;
GLuint uiVBO       = 0;

struct Chunk {
    int chunkX, chunkZ;
    std::vector<float> vertices;
    GLuint VAO, VBO;
};

std::unordered_map<std::pair<int,int>, Chunk, PairHash> chunks;

enum Biome {
    BIOME_PLAINS,
    BIOME_DESERT,
    BIOME_EXTREME_HILLS,
    BIOME_FOREST,
    BIOME_OCEAN
};

// --- Revised getBiome function ---
// Desert regions now spawn less frequently (threshold lowered),
// while extreme hills, plains and forest are determined from combined noise.
static Biome getBiome(int x, int z) {
    float oceanNoise = perlinNoise(x * 0.001f, z * 0.001f);
    if(oceanNoise < -0.8f)
        return BIOME_OCEAN;

    // Lower desert frequency by using a stricter threshold.
    float desertNoise = perlinNoise(x * 0.0007f, z * 0.0007f);
    if(desertNoise < -0.2f)
        return BIOME_DESERT;

    float combined = perlinNoise(x * 0.005f, z * 0.005f);
    if(combined < -0.1f)
        return BIOME_PLAINS;
    else if(combined < 0.0f)
        return BIOME_FOREST;
    else
        return BIOME_EXTREME_HILLS;
}

// --- New blended terrain height function ---
// For non-ocean biomes, we blend a base height (normal terrain) with an extreme hills (mountain) height.
// For deserts, both the normal and extreme components are lowered.
int getTerrainHeightAt(int x, int z) {
    Biome b = getBiome(x, z);
    if(b == BIOME_OCEAN)
        return 8;

    // Compute normal height.
    float normalNoise = fbmNoise(x * 0.01f, z * 0.01f, 6, 2.0f, 0.5f);
    float normalHeight = ((normalNoise + 1.0f) / 2.0f) * (b == BIOME_DESERT ? 18.0f : 24.0f);

    // Compute extreme hills height (mountain component) using a ridge transformation.
    float hillsNoise = fbmNoise(x * 0.002f, z * 0.002f, 6, 2.0f, 0.5f);
    float ridge = 1.0f - fabs(hillsNoise);
    float extremeHeight = (b == BIOME_DESERT)
    ? 30.0f + pow(ridge, 2.0f) * 40.0f  // for desert, lower mountains
    : 40.0f + pow(ridge, 2.0f) * 80.0f; // for others

    // Compute blend factor from combined noise.
    float combined = perlinNoise(x * 0.005f, z * 0.005f);
    float blend = smoothstep(-0.1f, 0.1f, combined);

    float finalHeight = mix(normalHeight, extremeHeight, blend);
    return (int) finalHeight;
}

static bool blockHasCollision(BlockType t) {
    return (t != BLOCK_WATER);
}

bool isSolidBlock(int bx, int by, int bz) {
    auto key = std::make_tuple(bx, by, bz);
    if(extraBlocks.find(key) != extraBlocks.end()){
        BlockType t = extraBlocks[key];
        if((int)t < 0) return false;
        return blockHasCollision(t);
    }
    if(waterLevels.find(key) != waterLevels.end())
        return false;
    int h = getTerrainHeightAt(bx, bz);
    return (by >= 0 && by <= h);
}

static bool checkCollision(const Vec3 &pos) {
    float half = playerWidth * 0.5f;
    float minX = pos.x - half, maxX = pos.x + half;
    float minY = pos.y, maxY = pos.y + playerHeight;
    float minZ = pos.z - half, maxZ = pos.z + half;
    int startX = (int)std::floor(minX), endX = (int)std::floor(maxX);
    int startY = (int)std::floor(minY), endY = (int)std::floor(maxY);
    int startZ = (int)std::floor(minZ), endZ = (int)std::floor(maxZ);
    for(int bx = startX; bx <= endX; bx++){
        for(int by = startY; by <= endY; by++){
            for(int bz = startZ; bz <= endZ; bz++){
                if(isSolidBlock(bx, by, bz)){
                    if(maxX > bx && minX < bx+1 &&
                        maxY > by && minY < by+1 &&
                        maxZ > bz && minZ < bz+1)
                        return true;
                }
            }
        }
    }
    return false;
}

bool canWaterFlowInto(int x, int y, int z) {
    std::tuple<int,int,int> key = {x, y, z};
    if(extraBlocks.find(key) != extraBlocks.end())
        return false;
    Biome b = getBiome(x, z);
    if(b != BIOME_OCEAN) {
        int terrainHeight = getTerrainHeightAt(x, z);
        if(y <= terrainHeight)
            return false;
    }
    return true;
}

static void rebuildChunk(int cx, int cz);

static const int NEAR_CHUNK_RADIUS = 2;
static void updateWaterFlow(const Camera &camera, float /*dt*/) {
    int playerChunkX = (int)std::floor(camera.position.x / (float)chunkSize);
    int playerChunkZ = (int)std::floor(camera.position.z / (float)chunkSize);
    std::vector<std::tuple<int,int,int>> waterKeys;
    for(auto &entry : waterLevels)
        waterKeys.push_back(entry.first);
    for(auto key : waterKeys) {
        int x, y, z;
        std::tie(x, y, z) = key;
        int cellChunkX = x / 16; if(x < 0 && x % 16 != 0) cellChunkX--;
        int cellChunkZ = z / 16; if(z < 0 && z % 16 != 0) cellChunkZ--;
        if (std::abs(cellChunkX - playerChunkX) > NEAR_CHUNK_RADIUS ||
            std::abs(cellChunkZ - playerChunkZ) > NEAR_CHUNK_RADIUS)
            continue;
        int level = waterLevels[key];
        if(y > 0 && canWaterFlowInto(x, y - 1, z)) {
            std::tuple<int,int,int> below = {x, y - 1, z};
            int belowLevel = 0;
            if(waterLevels.find(below) != waterLevels.end())
                belowLevel = waterLevels[below];
            if(8 > belowLevel) {
                waterLevels[below] = 8;
                int cx = x / 16; if(x < 0 && x % 16 != 0) cx--;
                int cz = z / 16; if(z < 0 && z % 16 != 0) cz--;
                rebuildChunk(cx, cz);
            }
        }
        if(level > 1) {
            int offsets[4][3] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
            for(int i = 0; i < 4; i++) {
                int nx = x + offsets[i][0];
                int ny = y;
                int nz = z + offsets[i][2];
                if(!canWaterFlowInto(nx, ny, nz))
                    continue;
                std::tuple<int,int,int> neighbor = {nx, ny, nz};
                int neighborLevel = 0;
                if(waterLevels.find(neighbor) != waterLevels.end())
                    neighborLevel = waterLevels[neighbor];
                int newLevel = level - 1;
                if(newLevel > neighborLevel && newLevel > 1) {
                    waterLevels[neighbor] = newLevel;
                    int cx = nx / 16; if(nx < 0 && nx % 16 != 0) cx--;
                    int cz = nz / 16; if(nz < 0 && nz % 16 != 0) cz--;
                    rebuildChunk(cx, cz);
                }
            }
        }
    }
}

static Chunk generateChunk(int cx, int cz) {
    Chunk chunk;
    chunk.chunkX = cx;
    chunk.chunkZ = cz;
    std::vector<float> verts;
    verts.reserve(16 * 16 * 36 * 5);
    unsigned int chunkSeed = (unsigned int)(cx * 73856093u ^ cz * 19349663u);
    Biome chunkBiome = getBiome(cx * 16 + 8, cz * 16 + 8);
    if(chunkBiome == BIOME_OCEAN) {
        const int oceanWaterLayers = 6;
        for(int y = 0; y < oceanWaterLayers; y++){
            addCube(verts, (float)(cx*16 + 0), (float)y, (float)(cz*16 + 0), BLOCK_WATER, false);
        }
    }
    for(int lx = 0; lx < 16; lx++){
        for(int lz = 0; lz < 16; lz++){
            int wx = cx * 16 + lx;
            int wz = cz * 16 + lz;
            Biome b = getBiome(wx, wz);
            if(b == BIOME_OCEAN) {
                const int oceanWaterLayers = 6;
                for(int y = 0; y < oceanWaterLayers; y++){
                    addCube(verts, (float)wx, (float)y, (float)wz, BLOCK_WATER, false);
                    waterLevels[{wx, y, wz}] = 8;
                }
                addCube(verts, (float)wx, (float)oceanWaterLayers, (float)wz, BLOCK_SAND, false);
                addCube(verts, (float)wx, (float)(oceanWaterLayers + 1), (float)wz, BLOCK_BEDROCK, false);
            } else {
                int height = getTerrainHeightAt(wx, wz);
                for(int y = 0; y <= height; y++){
                    BlockType type;
                    if(b == BIOME_DESERT){
                        const int sandLayers = 2, dirtLayers = 3;
                        if(y >= height - sandLayers)
                            type = BLOCK_SAND;
                        else if(y >= height - (sandLayers + dirtLayers))
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    } else {
                        if(y == height)
                            type = BLOCK_GRASS;
                        else if((height - y) <= 6)
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    }
                    addCube(verts, (float)wx, (float)y, (float)wz, type, true);
                }
                // --- Adjusted tree generation ---
                int chance = 0;
                if(b == BIOME_FOREST) chance = 5;
                else if(b == BIOME_PLAINS) chance = 70;  // lower frequency (1 in 70)
                else if(b == BIOME_DESERT) chance = 100;   // even fewer trees in desert
                else if(b == BIOME_EXTREME_HILLS) chance = 0;
                if(chance > 0 && (rand() % chance == 0)) {
                    int trunkH = 4 + (rand() % 3);
                    int baseY = height + 1;
                    for(int ty = baseY; ty < baseY + trunkH; ty++){
                        addCube(verts, (float)wx, (float)ty, (float)wz, BLOCK_TREE_LOG, true);
                        extraBlocks[{wx, ty, wz}] = BLOCK_TREE_LOG;
                    }
                    int topY = baseY + trunkH - 1;
                    for(int lx2 = wx - 1; lx2 <= wx + 1; lx2++){
                        for(int lz2 = wz - 1; lz2 <= wz + 1; lz2++){
                            if(lx2 == wx && lz2 == wz)
                                continue;
                            addCube(verts, (float)lx2, (float)topY, (float)lz2, BLOCK_LEAVES, false);
                            extraBlocks[{lx2, topY, lz2}] = BLOCK_LEAVES;
                        }
                    }
                    addCube(verts, (float)wx, (float)(topY + 1), (float)wz, BLOCK_LEAVES, false);
                    extraBlocks[{wx, topY + 1, wz}] = BLOCK_LEAVES;
                }
            }
        }
    }
    chunk.vertices = verts;
    glGenVertexArrays(1, &chunk.VAO);
    glGenBuffers(1, &chunk.VBO);
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return chunk;
}

static void getChunkCoords(int bx, int bz, int &cx, int &cz) {
    cx = bx / 16; if(bx < 0 && bx % 16 != 0) cx--;
    cz = bz / 16; if(bz < 0 && bz % 16 != 0) cz--;
}

static void rebuildChunk(int cx, int cz) {
    Chunk &chunk = chunks[{cx, cz}];
    std::vector<float> verts;
    verts.reserve(16 * 16 * 36 * 5);

    for (int lx = 0; lx < 16; lx++){
        for (int lz = 0; lz < 16; lz++){
            int wx = cx * 16 + lx;
            int wz = cz * 16 + lz;
            Biome b = getBiome(wx, wz);
            if(b == BIOME_OCEAN) {
                const int oceanWaterLayers = 6;
                for (int y = 0; y < oceanWaterLayers; y++){
                    addCube(verts, (float)wx, (float)y, (float)wz, BLOCK_WATER, false);
                    waterLevels[{wx, y, wz}] = 8;
                }
                addCube(verts, (float)wx, (float)oceanWaterLayers, (float)wz, BLOCK_SAND, false);
                addCube(verts, (float)wx, (float)(oceanWaterLayers + 1), (float)wz, BLOCK_BEDROCK, false);
            } else {
                int height = getTerrainHeightAt(wx, wz);
                for (int y = 0; y <= height; y++){
                    std::tuple<int,int,int> key = {wx, y, wz};
                    if (waterLevels.find(key) != waterLevels.end()){
                        addCube(verts, (float)wx, (float)y, (float)wz, BLOCK_WATER, true);
                        continue;
                    }
                    auto it = extraBlocks.find(key);
                    if (it != extraBlocks.end()){
                        BlockType ov = it->second;
                        if ((int)ov < 0) continue;
                        addCube(verts, (float)wx, (float)y, (float)wz, ov, true);
                    } else {
                        BlockType terr;
                        if(b == BIOME_DESERT){
                            const int sandLayers = 2, dirtLayers = 3;
                            if(y >= height - sandLayers)
                                terr = BLOCK_SAND;
                            else if(y >= height - (sandLayers + dirtLayers))
                                terr = BLOCK_DIRT;
                            else
                                terr = BLOCK_STONE;
                        } else {
                            if(y == height)
                                terr = BLOCK_GRASS;
                            else if((height - y) <= 6)
                                terr = BLOCK_DIRT;
                            else
                                terr = BLOCK_STONE;
                        }
                        addCube(verts, (float)wx, (float)y, (float)wz, terr, true);
                    }
                }
                for (int y = height + 1; y < height + 20; y++){
                    std::tuple<int,int,int> key = {wx, y, wz};
                    if (extraBlocks.find(key) != extraBlocks.end()){
                        BlockType ov = extraBlocks[key];
                        if ((int)ov < 0) continue;
                        addCube(verts, (float)wx, (float)y, (float)wz, ov, true);
                    }
                }
            }
        }
    }

    for (auto &kv : waterLevels){
        int bx = std::get<0>(kv.first);
        int by = std::get<1>(kv.first);
        int bz = std::get<2>(kv.first);
        int ccx, ccz;
        getChunkCoords(bx, bz, ccx, ccz);
        if(ccx == cx && ccz == cz)
            addCube(verts, (float)bx, (float)by, (float)bz, BLOCK_WATER, true);
    }

    chunk.vertices = verts;
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

// -----------------------------------------------------------------------------
// Shaders and UI drawing functions.
static const char* worldVertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTex;
uniform mat4 MVP;
out vec3 FragPos;
out vec2 TexCoord;
void main(){
    gl_Position = MVP * vec4(aPos, 1.0);
    FragPos = aPos;
    TexCoord = aTex;
}
)";

static const char* worldFragSrc = R"(
#version 330 core
in vec3 FragPos;
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;
uniform vec3 sunDirection;
uniform vec3 viewPos;
void main(){
    vec3 dx = dFdx(FragPos);
    vec3 dy = dFdy(FragPos);
    vec3 normal = normalize(cross(dx, dy));

    float diff = max(dot(normal, sunDirection), 0.0);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-sunDirection, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 ambient = vec3(0.4);
    vec3 diffuse = vec3(0.6) * diff;
    vec3 specular = vec3(0.2) * spec;
    vec3 lighting = ambient + diffuse + specular;

    vec4 texColor = texture(ourTexture, TexCoord);
    if(texColor.a < 0.1)
        discard;

    FragColor = vec4(texColor.rgb * lighting, texColor.a);
}
)";

static const char* uiVertSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform mat4 uProj;
void main(){
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* uiFragSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main(){
    FragColor = uColor;
}
)";

static void initUI() {
    uiShader = createShaderProgram(uiVertSrc, uiFragSrc);
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

int drawPauseMenu(int screenW, int screenH) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiShader);
    float overlayVerts[12] = { 0, 0, (float)screenW, 0, (float)screenW, (float)screenH,
        0, 0, (float)screenW, (float)screenH, 0, (float)screenH };
        glBindVertexArray(uiVAO);
        glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(overlayVerts), overlayVerts);
        Mat4 proj = {};
        proj.m[0]  = 2.0f/(float)screenW;
        proj.m[5]  = 2.0f/(float)screenH;
        proj.m[10] = -1.0f;
        proj.m[15] = 1.0f;
        proj.m[12] = -1.0f;
        proj.m[13] = -1.0f;
        glUniformMatrix4fv(glGetUniformLocation(uiShader, "uProj"), 1, GL_FALSE, proj.m);
        glUniform4f(glGetUniformLocation(uiShader, "uColor"), 0.0f, 0.0f, 0.0f, 0.5f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        float resumeX = 300, resumeY = 250, resumeW = 200, resumeH = 50;
        float resumeVerts[12] = { resumeX, resumeY, resumeX+resumeW, resumeY, resumeX+resumeW, resumeY+resumeH,
            resumeX, resumeY, resumeX+resumeW, resumeY+resumeH, resumeX, resumeY+resumeH };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(resumeVerts), resumeVerts);
            glUniform4f(glGetUniformLocation(uiShader, "uColor"), 0.2f, 0.6f, 1.0f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            float quitX = 300, quitY = 150, quitW = 200, quitH = 50;
            float quitVerts[12] = { quitX, quitY, quitX+quitW, quitY, quitX+quitW, quitY+quitH,
                quitX, quitY, quitX+quitW, quitY+quitH, quitX, quitY+quitH };
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quitVerts), quitVerts);
                glUniform4f(glGetUniformLocation(uiShader, "uColor"), 1.0f, 0.3f, 0.3f, 1.0f);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                int mx, my;
                Uint32 mState = SDL_GetMouseState(&mx, &my);
                bool leftDown = (mState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
                int invY = screenH - my;
                int result = 0;
                if(leftDown) {
                    if(mx >= resumeX && mx <= resumeX+resumeW && invY >= resumeY && invY <= resumeY+resumeH)
                        result = 1;
                    else if(mx >= quitX && mx <= quitX+quitW && invY >= quitY && invY <= quitY+quitH)
                        result = 2;
                }
                glDisable(GL_BLEND);
                glEnable(GL_DEPTH_TEST);
                return result;
}

void drawFlyIndicator(bool isFlying, int screenW, int screenH) {
    float w = 20.0f, h = 20.0f, x = 5.0f, y = (float)screenH - h - 5.0f;
    float r = isFlying ? 0.1f : 1.0f;
    float g = isFlying ? 1.0f : 0.0f;
    float b = isFlying ? 0.1f : 0.0f;
    glDisable(GL_DEPTH_TEST);
    glUseProgram(uiShader);
    float verts[12] = { x, y, x+w, y, x+w, y+h, x, y, x+w, y+h, x, y+h };
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    Mat4 proj = {};
    proj.m[0]  = 2.0f/(float)screenW;
    proj.m[5]  = 2.0f/(float)screenH;
    proj.m[10] = -1.0f;
    proj.m[15] = 1.0f;
    proj.m[12] = -1.0f;
    proj.m[13] = -1.0f;
    glUniformMatrix4fv(glGetUniformLocation(uiShader, "uProj"), 1, GL_FALSE, proj.m);
    glUniform4f(glGetUniformLocation(uiShader, "uColor"), r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);
}

void drawFirstPersonHand3D(int screenW, int screenH, const Mat4 &proj) {
    // (Unused in the new approach)
}

int main(int /*argc*/, char* /*argv*/[]) {
    float loadedX = 0.0f, loadedY = 30.0f, loadedZ = 0.0f;
    int loadedSeed = 0;
    bool loadedOk = loadWorld("saved_world.txt", loadedSeed, loadedX, loadedY, loadedZ);
    if(loadedOk)
        std::cout << "[World] Loaded seed=" << loadedSeed
        << " player(" << loadedX << "," << loadedY << "," << loadedZ << ")\n";
    else {
        unsigned int rseed = (unsigned int)time(nullptr);
        std::cout << "[World] No saved world, random seed=" << rseed << "\n";
        setNoiseSeed(rseed);
        srand(rseed);
        loadedSeed = (int)rseed;
    }
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("Voxel Engine",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH, SCREEN_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if(!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if(!glContext) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if(glewErr != GLEW_OK) {
        std::cerr << "GLEW Error: " << glewGetErrorString(glewErr) << std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    worldShader = createShaderProgram(worldVertSrc, worldFragSrc);
    texID = loadTexture("texture.png");
    if(!texID) {
        std::cerr << "Texture failed to load!\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    handTex = loadTexture("hand.png");
    if(!handTex) {
        std::cerr << "Hand texture failed to load!\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    initUI();
    Inventory inventory;
    int spawnChunkX = (int)std::floor(loadedX / (float)chunkSize);
    int spawnChunkZ = (int)std::floor(loadedZ / (float)chunkSize);
    std::pair<int,int> chunkKey = {spawnChunkX, spawnChunkZ};
    if(chunks.find(chunkKey) == chunks.end())
        chunks[chunkKey] = generateChunk(spawnChunkX, spawnChunkZ);
    Camera camera;
    camera.position = {loadedX, loadedY, loadedZ};
    camera.yaw = -3.14f/2;
    camera.pitch = 0.0f;
    bool paused = false, isFlying = false;
    float verticalVelocity = 0.0f;
    int tickCount = 0;
    float tickAccumulator = 0.0f;
    if(loadedOk) {
        int pcx = (int)std::floor(camera.position.x / (float)chunkSize);
        int pcz = (int)std::floor(camera.position.z / (float)chunkSize);
        for(int cx = pcx - renderDistance; cx <= pcx + renderDistance; cx++){
            for(int cz = pcz - renderDistance; cz <= pcz + renderDistance; cz++){
                std::pair<int,int> cKey = {cx, cz};
                if(chunks.find(cKey) == chunks.end())
                    chunks[cKey] = generateChunk(cx, cz);
                else
                    rebuildChunk(cx, cz);
            }
        }
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);
    Uint32 lastTime = SDL_GetTicks();
    bool running = true;
    SDL_Event ev;
    Mat4 projWorld = perspectiveMatrix(45.0f*(3.14159f/180.0f),
                                       (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                                       0.1f, 100.0f);
    while(running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) * 0.001f;
        lastTime = now;
        tickAccumulator += dt;
        while(tickAccumulator >= TICK_INTERVAL) {
            tickCount++;
            tickAccumulator -= TICK_INTERVAL;
            if(tickCount % 2 == 0) {
                updateWaterFlow(camera, TICK_INTERVAL);
            }
        }
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_QUIT) running = false;
            else if(ev.type == SDL_KEYDOWN) {
                if(ev.key.keysym.sym == SDLK_ESCAPE) {
                    paused = !paused;
                    if(inventory.isOpen()) inventory.toggle();
                    SDL_SetRelativeMouseMode(paused ? SDL_FALSE : SDL_TRUE);
                }
                else if(ev.key.keysym.sym == SDLK_f) {
                    isFlying = !isFlying;
                    verticalVelocity = 0.0f;
                    std::cout << (isFlying ? "[Fly] ON\n" : "[Fly] OFF\n");
                }
                else if(!paused && ev.key.keysym.sym == SDLK_e) {
                    inventory.toggle();
                    SDL_SetRelativeMouseMode(inventory.isOpen() ? SDL_FALSE : SDL_TRUE);
                }
                else if(!paused && !inventory.isOpen() && !isFlying &&
                    ev.key.keysym.sym == SDLK_SPACE) {
                    int footX = (int)std::floor(camera.position.x);
                int footY = (int)std::floor(camera.position.y - 0.1f);
                int footZ = (int)std::floor(camera.position.z);
                if(isSolidBlock(footX, footY, footZ))
                    verticalVelocity = JUMP_SPEED;
                    }
            }
            else if(ev.type == SDL_MOUSEMOTION) {
                if(!paused && !inventory.isOpen()) {
                    float sens = 0.002f;
                    camera.yaw += ev.motion.xrel * sens;
                    camera.pitch -= ev.motion.yrel * sens;
                    if(camera.pitch > 1.57f) camera.pitch = 1.57f;
                    if(camera.pitch < -1.57f) camera.pitch = -1.57f;
                }
            }
            else if(!paused && !inventory.isOpen() && ev.type == SDL_MOUSEBUTTONDOWN) {
                Vec3 eyePos = camera.position; eyePos.y += 1.6f;
                Vec3 viewDir = { cos(camera.yaw)*cos(camera.pitch),
                    sin(camera.pitch),
                    sin(camera.yaw)*cos(camera.pitch) };
                    viewDir = normalize(viewDir);
                    int bx, by, bz;
                    bool hit = raycastBlock(eyePos, viewDir, 5.0f, bx, by, bz);
                    if(hit) {
                        if(ev.button.button == SDL_BUTTON_LEFT) {
                            auto it = extraBlocks.find({bx,by,bz});
                            if(it != extraBlocks.end())
                                extraBlocks.erase(it);
                            else
                                extraBlocks[{bx,by,bz}] = (BlockType)(-1);
                            int cx, cz; getChunkCoords(bx, bz, cx, cz);
                            rebuildChunk(cx, cz);
                        }
                        else if(ev.button.button == SDL_BUTTON_RIGHT) {
                            float stepBack = 0.05f, traveled = 0.0f;
                            while(traveled < 5.0f) {
                                Vec3 pos = add(eyePos, multiply(viewDir, traveled));
                                int cbx = (int)std::floor(pos.x);
                                int cby = (int)std::floor(pos.y);
                                int cbz = (int)std::floor(pos.z);
                                if(cbx == bx && cby == by && cbz == bz) {
                                    float tb = traveled - stepBack;
                                    if(tb < 0) break;
                                    Vec3 placePos = add(eyePos, multiply(viewDir, tb));
                                    int pbx = (int)std::floor(placePos.x);
                                    int pby = (int)std::floor(placePos.y);
                                    int pbz = (int)std::floor(placePos.z);
                                    if(!isSolidBlock(pbx, pby, pbz)) {
                                        int blockToPlace = inventory.getSelectedBlock();
                                        extraBlocks[{pbx, pby, pbz}] = (BlockType)blockToPlace;
                                        if(blockToPlace == BLOCK_WATER)
                                            waterLevels[{pbx, pby, pbz}] = 8;
                                        int cpx, cpz; getChunkCoords(pbx, pbz, cpx, cpz);
                                        rebuildChunk(cpx, cpz);
                                    }
                                    break;
                                }
                                traveled += 0.1f;
                            }
                        }
                    }
            }
        }
        if(paused) {
            glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(worldShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID);
            GLint tLoc = glGetUniformLocation(worldShader, "ourTexture");
            glUniform1i(tLoc, 0);
            Vec3 eyePos = camera.position; eyePos.y += 1.6f;
            Vec3 viewDir = { cos(camera.yaw)*cos(camera.pitch),
                sin(camera.pitch),
                sin(camera.yaw)*cos(camera.pitch) };
                Vec3 camTgt = add(eyePos, viewDir);
                Mat4 view = lookAtMatrix(eyePos, camTgt, {0,1,0});
                Mat4 proj = perspectiveMatrix(45.0f*(3.14159f/180.0f),
                                              (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                                              0.1f, 100.0f);
                Mat4 pv = multiplyMatrix(proj, view);
                int pcx = (int)std::floor(camera.position.x/(float)chunkSize);
                int pcz = (int)std::floor(camera.position.z/(float)chunkSize);
                for(auto &pair : chunks) {
                    int cX = pair.first.first, cZ = pair.first.second;
                    if(std::abs(cX-pcx) > renderDistance || std::abs(cZ-pcz) > renderDistance)
                        continue;
                    Chunk &ch = pair.second;
                    Mat4 mvp = multiplyMatrix(pv, identityMatrix());
                    GLint mvpLoc = glGetUniformLocation(worldShader, "MVP");
                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
                    glBindVertexArray(ch.VAO);
                    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ch.vertices.size()/5));
                }
                int clicked = drawPauseMenu(SCREEN_WIDTH, SCREEN_HEIGHT);
                if(clicked == 1) {
                    paused = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                else if(clicked == 2) {
                    saveWorld("saved_world.txt", loadedSeed,
                              camera.position.x, camera.position.y, camera.position.z);
                    running = false;
                }
                SDL_GL_SwapWindow(window);
                continue;
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if(!inventory.isOpen()){
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            float speed = 10.0f * dt;
            Vec3 forward = { cos(camera.yaw), 0, sin(camera.yaw) };
            forward = normalize(forward);
            Vec3 right = normalize(cross(forward, {0,1,0}));
            Vec3 horizDelta = {0,0,0};
            if(keys[SDL_SCANCODE_W]) horizDelta = add(horizDelta, multiply(forward, speed));
            if(keys[SDL_SCANCODE_S]) horizDelta = subtract(horizDelta, multiply(forward, speed));
            if(keys[SDL_SCANCODE_A]) horizDelta = subtract(horizDelta, multiply(right, speed));
            if(keys[SDL_SCANCODE_D]) horizDelta = add(horizDelta, multiply(right, speed));
            Vec3 newPosH = camera.position;
            newPosH.x += horizDelta.x;
            newPosH.z += horizDelta.z;
            if(!checkCollision(newPosH)){
                camera.position.x = newPosH.x;
                camera.position.z = newPosH.z;
            }
            if(isFlying) {
                verticalVelocity = 0.0f;
                float flySpeed = 10.0f * dt;
                if(keys[SDL_SCANCODE_SPACE]){
                    Vec3 upPos = camera.position;
                    upPos.y += flySpeed;
                    if(!checkCollision(upPos))
                        camera.position.y = upPos.y;
                }
                if(keys[SDL_SCANCODE_LSHIFT]){
                    Vec3 downPos = camera.position;
                    downPos.y -= flySpeed;
                    if(!checkCollision(downPos))
                        camera.position.y = downPos.y;
                }
            } else {
                verticalVelocity += GRAVITY * dt;
                float dy = verticalVelocity * dt;
                Vec3 newPosV = camera.position;
                newPosV.y += dy;
                if(!checkCollision(newPosV))
                    camera.position.y = newPosV.y;
                else
                    verticalVelocity = 0.0f;
            }
            if(camera.position.y < WORLD_FLOOR_LIMIT) {
                std::cout << "[World] Player fell below kill plane => reset.\n";
                camera.position.y = 30.0f;
                verticalVelocity = 0.0f;
            }
        }
        inventory.update(dt, camera);
        int pcx = (int)std::floor(camera.position.x/(float)chunkSize);
        int pcz = (int)std::floor(camera.position.z/(float)chunkSize);
        for(int cx = pcx - renderDistance; cx <= pcx + renderDistance; cx++){
            for(int cz = pcz - renderDistance; cz <= pcz + renderDistance; cz++){
                std::pair<int,int> key = {cx, cz};
                if(chunks.find(key) == chunks.end())
                    chunks[key] = generateChunk(cx, cz);
            }
        }
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(worldShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        GLint uniTex = glGetUniformLocation(worldShader, "ourTexture");
        glUniform1i(uniTex, 0);
        Vec3 sunDir = normalize({0.3f, 1.0f, 0.3f});
        glUniform3f(glGetUniformLocation(worldShader, "sunDirection"), sunDir.x, sunDir.y, sunDir.z);
        glUniform3f(glGetUniformLocation(worldShader, "viewPos"), camera.position.x, camera.position.y, camera.position.z);

        Vec3 eyePos = camera.position; eyePos.y += 1.6f;
        Vec3 viewDir = { cos(camera.yaw)*cos(camera.pitch),
            sin(camera.pitch),
            sin(camera.yaw)*cos(camera.pitch) };
            Vec3 camTgt = add(eyePos, viewDir);
            Mat4 view = lookAtMatrix(eyePos, camTgt, {0,1,0});
            Mat4 projWorld = perspectiveMatrix(45.0f*(3.14159f/180.0f),
                                               (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                                               0.1f, 100.0f);
            Mat4 pv = multiplyMatrix(projWorld, view);
            for(auto &pair: chunks){
                int cX = pair.first.first, cZ = pair.first.second;
                if(std::abs(cX-pcx) > renderDistance || std::abs(cZ-pcz) > renderDistance)
                    continue;
                Chunk &ch = pair.second;
                Mat4 mvp = multiplyMatrix(pv, identityMatrix());
                GLint mvpLoc = glGetUniformLocation(worldShader, "MVP");
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
                glBindVertexArray(ch.VAO);
                glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ch.vertices.size()/5));
            }
            glUseProgram(uiShader);
            drawFlyIndicator(isFlying, SCREEN_WIDTH, SCREEN_HEIGHT);
            inventory.render();
            if(inventory.getSelectedBlock() != BLOCK_NONE) {
                glDisable(GL_DEPTH_TEST);
                renderHeldBlock3D(projWorld, inventory.getSelectedBlock());
                glEnable(GL_DEPTH_TEST);
            } else {
                glDisable(GL_DEPTH_TEST);
                renderHandRect(projWorld);
                glEnable(GL_DEPTH_TEST);
            }
            SDL_GL_SwapWindow(window);
    }
    saveWorld("saved_world.txt", loadedSeed,
              camera.position.x, camera.position.y, camera.position.z);
    glDeleteProgram(worldShader);
    glDeleteProgram(uiShader);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
