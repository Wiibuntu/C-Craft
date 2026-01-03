#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <random>
#include <algorithm>
#include <atomic>

#include "math.h"       // identityMatrix(), multiplyMatrix(), vector math, etc.
#include "shader.h"     // Shader compilation and program creation
#include "cube.h"       // BlockType definitions and addCube()
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
void drawFirstPersonHand3D(int screenW, int screenH, const Mat4 &proj); // unused

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

// --- Player physical constants ---
// "2 blocks tall" collision capsule and "2 blocks high" camera per your request.
static const float playerWidth  = 0.6f;
static const float playerHeight = 2.0f;
static const float EYE_HEIGHT   = 2.0f;

static const float WORLD_FLOOR_LIMIT = -10.0f;

static const float GRAVITY    = -9.81f;
static const float JUMP_SPEED =  5.0f;

// Screen size
int SCREEN_WIDTH  = 960;
int SCREEN_HEIGHT = 480;

// Chunk settings
static const int chunkSize      = 16;
static const int renderDistance = 10;

// Chunk streaming limits (stutter fix)
// - How many chunk uploads we allow per frame
static const int MAX_CHUNK_UPLOADS_PER_FRAME = 2;

// GL objects
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

// ----------------------------------------------------------------------------
// Chunk background generation queues (CPU only)
// ----------------------------------------------------------------------------
struct ChunkJob {
    int cx, cz;
    bool rebuild; // if true, chunk exists and we want to rebuild it
};

struct ChunkResult {
    int cx, cz;
    std::vector<float> verts;
    bool rebuild;
};

static std::mutex gJobMutex;
static std::condition_variable gJobCV;
static std::queue<ChunkJob> gJobQueue;

static std::mutex gDoneMutex;
static std::queue<ChunkResult> gDoneQueue;

static std::atomic<bool> gWorkerRunning{true};

// Track which chunks are already requested so we don't spam jobs.
static std::mutex gRequestedMutex;
static std::unordered_set<long long> gRequested; // packed (cx,cz)

// pack key
static long long packChunkKey(int cx, int cz) {
    return ( (long long)cx << 32 ) ^ (unsigned int)cz;
}

// Forward declarations
static bool raycastBlock(const Vec3 &start, const Vec3 &dir, float maxDist, int &outX, int &outY, int &outZ);
static void rebuildChunkAsync(int cx, int cz);
static void requestChunkAsync(int cx, int cz);

// ----------------------------------------------------------------------------
// Raycast
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// First-person hand / held block
// ----------------------------------------------------------------------------
void renderHeldBlock3D(const Mat4 &proj, int activeBlock) {
    Mat4 model = identityMatrix();
    model = multiplyMatrix(model, translateMatrix(0.8f, -0.8f, -1.5f));
    Mat4 rotY = identityMatrix();
    float angle = 0.3f;
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

void renderHandRect(const Mat4 &proj) {
    float handVerts[] = {
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
    float angle = 0.2f;
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
// Biomes + terrain height (already fixed/clamped)
// -----------------------------------------------------------------------------
static Biome getBiome(int x, int z) {
    float oceanNoise = perlinNoise(x * 0.001f, z * 0.001f);
    if(oceanNoise < -0.8f)
        return BIOME_OCEAN;

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

int getTerrainHeightAt(int x, int z) {
    Biome b = getBiome(x, z);
    if(b == BIOME_OCEAN)
        return 8;

    float normalNoise  = fbmNoise(x * 0.01f, z * 0.01f, 6, 2.0f, 0.5f);
    float normalHeight = ((normalNoise + 1.0f) * 0.5f) * (b == BIOME_DESERT ? 18.0f : 24.0f);

    float hillsNoise = fbmNoise(x * 0.0025f, z * 0.0025f, 6, 2.0f, 0.5f);
    float ridge      = 1.0f - fabs(hillsNoise);
    ridge            = ridge * ridge;

    float extremeHeight = (b == BIOME_DESERT)
        ? 18.0f + ridge * 24.0f
        : 24.0f + ridge * 36.0f;

    float combined = perlinNoise(x * 0.005f, z * 0.005f);
    float blend    = smoothstep(-0.1f, 0.1f, combined);

    float finalHeight = mix(normalHeight, extremeHeight, blend);

    if(finalHeight < 2.0f)  finalHeight = 2.0f;
    if(finalHeight > 64.0f) finalHeight = 64.0f;

    return (int) finalHeight;
}

// -----------------------------------------------------------------------------
// Collision + solidity
// -----------------------------------------------------------------------------
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

// Collision check uses player FEET position (capsule base)
static bool checkCollisionFeet(const Vec3 &feetPos) {
    float half = playerWidth * 0.5f;
    float minX = feetPos.x - half, maxX = feetPos.x + half;
    float minY = feetPos.y,        maxY = feetPos.y + playerHeight;
    float minZ = feetPos.z - half, maxZ = feetPos.z + half;

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

// -----------------------------------------------------------------------------
// Spawn safety helpers
// -----------------------------------------------------------------------------
static int surfaceYAt(int x, int z) {
    int h = getTerrainHeightAt(x, z);
    if(h < 0) h = 0;
    return h;
}

// sanitize expects FEET coords
static void sanitizeLoadedSpawn(float &x, float &y, float &z) {
    int tx = (int)std::floor(x);
    int tz = (int)std::floor(z);

    int surface = surfaceYAt(tx, tz) + 2;
    if(y < (float)surface)
        y = (float)surface;

    Vec3 pos = {x, y, z};
    int lift = 0;
    while(lift < 256 && checkCollisionFeet(pos)) {
        pos.y += 1.0f;
        lift++;
    }

    x = pos.x;
    y = pos.y;
    z = pos.z;
}

static void findSafeSpawn(float &outX, float &outY, float &outZ) {
    const int maxRadius = 128;
    const int step      = 4;

    int bestX = 0, bestZ = 0;
    int bestScore = 1000000000;

    for(int r = 0; r <= maxRadius; r += step) {
        for(int dx = -r; dx <= r; dx += step) {
            for(int dz = -r; dz <= r; dz += step) {
                if(abs(dx) != r && abs(dz) != r) continue;

                int x = dx;
                int z = dz;

                Biome b = getBiome(x, z);
                if(b == BIOME_OCEAN) continue;

                int h  = surfaceYAt(x, z);

                int hE = surfaceYAt(x + 4, z);
                int hW = surfaceYAt(x - 4, z);
                int hN = surfaceYAt(x, z + 4);
                int hS = surfaceYAt(x, z - 4);
                int slope = std::max(std::max(abs(hE - hW), abs(hN - hS)),
                                     std::max(abs(hE - h),  abs(hN - h)));

                int score = slope * 10 + abs(h - 20);
                if(score >= bestScore) continue;

                Vec3 pos = {(float)x + 0.5f, (float)h + 2.0f, (float)z + 0.5f};

                int lift = 0;
                while(lift < 256 && checkCollisionFeet(pos)) {
                    pos.y += 1.0f;
                    lift++;
                }
                if(lift >= 256) continue;

                bestScore = score;
                bestX = x;
                bestZ = z;
            }
        }
        if(bestScore <= 15) break;
    }

    int h = surfaceYAt(bestX, bestZ);
    outX = (float)bestX + 0.5f;
    outZ = (float)bestZ + 0.5f;
    outY = (float)h + 2.0f;

    Vec3 pos = {outX, outY, outZ};
    int lift = 0;
    while(lift < 256 && checkCollisionFeet(pos)) {
        pos.y += 1.0f;
        lift++;
    }
    outY = pos.y;
}

// -----------------------------------------------------------------------------
// Water flow
// -----------------------------------------------------------------------------
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

static void getChunkCoords(int bx, int bz, int &cx, int &cz) {
    cx = bx / 16; if(bx < 0 && bx % 16 != 0) cx--;
    cz = bz / 16; if(bz < 0 && bz % 16 != 0) cz--;
}

static const int NEAR_CHUNK_RADIUS = 2;

// Instead of rebuildChunk() (which used to do heavy work on main thread),
// we queue rebuilds asynchronously to avoid stutter.
static void updateWaterFlow(const Camera &camera, float /*dt*/) {
    int playerChunkX = (int)std::floor(camera.position.x / (float)chunkSize);
    int playerChunkZ = (int)std::floor(camera.position.z / (float)chunkSize);

    std::vector<std::tuple<int,int,int>> waterKeys;
    waterKeys.reserve(waterLevels.size());
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
                int cx, cz;
                getChunkCoords(x, z, cx, cz);
                rebuildChunkAsync(cx, cz);
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
                    int cx, cz;
                    getChunkCoords(nx, nz, cx, cz);
                    rebuildChunkAsync(cx, cz);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Chunk vertex building (CPU only) — runs on worker thread.
// This is the key stutter fix: no GL calls in here.
// -----------------------------------------------------------------------------
static void buildChunkVerticesCPU(int cx, int cz, std::vector<float> &outVerts) {
    outVerts.clear();
    outVerts.reserve(16 * 16 * 36 * 5);

    for(int lx = 0; lx < 16; lx++){
        for(int lz = 0; lz < 16; lz++){
            int wx = cx * 16 + lx;
            int wz = cz * 16 + lz;

            Biome b = getBiome(wx, wz);
            if(b == BIOME_OCEAN){
                const int oceanWaterHeight = 6;
                for(int y = 0; y < oceanWaterHeight; y++){
                    addCube(outVerts, (float)wx, (float)y, (float)wz, BLOCK_WATER, false);
                }
                addCube(outVerts, (float)wx, (float)oceanWaterHeight, (float)wz, BLOCK_SAND, false);
            } else {
                int height = getTerrainHeightAt(wx, wz);
                for(int y = 0; y <= height; y++){
                    std::tuple<int,int,int> key = {wx, y, wz};

                    // Overrides (including carve out)
                    auto exIt = extraBlocks.find(key);
                    if(exIt != extraBlocks.end()){
                        BlockType ov = exIt->second;
                        if((int)ov < 0) continue; // carved
                        addCube(outVerts, (float)wx, (float)y, (float)wz, ov, true);
                        continue;
                    }

                    if(waterLevels.find(key) != waterLevels.end())
                        continue;

                    BlockType terr;
                    if(b == BIOME_DESERT){
                        const int sandLayers = 2;
                        const int dirtLayers = 3;
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
                    addCube(outVerts, (float)wx, (float)y, (float)wz, terr, true);
                }
            }

            // Add overrides above terrain (trees etc)
            int height = (b == BIOME_OCEAN) ? 6 : getTerrainHeightAt(wx, wz);
            for (int y = height + 1; y < height + 20; y++){
                std::tuple<int,int,int> key = {wx, y, wz};
                auto exIt = extraBlocks.find(key);
                if(exIt != extraBlocks.end()){
                    BlockType ov = exIt->second;
                    if ((int)ov < 0) continue;
                    if(waterLevels.find(key) != waterLevels.end()) continue;
                    addCube(outVerts, (float)wx, (float)y, (float)wz, ov, true);
                }
            }
        }
    }

    // Add water cells in this chunk
    for (auto &kv : waterLevels){
        int bx = std::get<0>(kv.first);
        int by = std::get<1>(kv.first);
        int bz = std::get<2>(kv.first);
        int ccx, ccz;
        getChunkCoords(bx, bz, ccx, ccz);
        if(ccx == cx && ccz == cz)
            addCube(outVerts, (float)bx, (float)by, (float)bz, BLOCK_WATER, true);
    }
}

// -----------------------------------------------------------------------------
// Worker thread: consumes ChunkJob, produces ChunkResult
// -----------------------------------------------------------------------------
static void chunkWorkerThread() {
    while(gWorkerRunning.load()) {
        ChunkJob job;
        {
            std::unique_lock<std::mutex> lk(gJobMutex);
            gJobCV.wait(lk, []{
                return !gWorkerRunning.load() || !gJobQueue.empty();
            });
            if(!gWorkerRunning.load())
                break;
            job = gJobQueue.front();
            gJobQueue.pop();
        }

        ChunkResult res;
        res.cx = job.cx;
        res.cz = job.cz;
        res.rebuild = job.rebuild;
        buildChunkVerticesCPU(job.cx, job.cz, res.verts);

        {
            std::lock_guard<std::mutex> lk(gDoneMutex);
            gDoneQueue.push(std::move(res));
        }
    }
}

// -----------------------------------------------------------------------------
// Main-thread chunk upload (GL only)
// -----------------------------------------------------------------------------
static void uploadChunkToGPU(int cx, int cz, const std::vector<float> &verts, bool rebuild) {
    std::pair<int,int> key = {cx, cz};

    if(rebuild) {
        auto it = chunks.find(key);
        if(it == chunks.end()) {
            // If it doesn't exist yet, fall back to create.
            rebuild = false;
        } else {
            // Update existing VAO/VBO
            Chunk &chunk = it->second;
            chunk.vertices = verts;
            glBindVertexArray(chunk.VAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glBindVertexArray(0);
            return;
        }
    }

    // Create new chunk
    Chunk chunk;
    chunk.chunkX = cx;
    chunk.chunkZ = cz;
    chunk.vertices = verts;

    glGenVertexArrays(1, &chunk.VAO);
    glGenBuffers(1, &chunk.VBO);

    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    chunks[key] = chunk;
}

// Request a chunk build if not already requested and not loaded.
static void requestChunkAsync(int cx, int cz) {
    std::pair<int,int> key = {cx, cz};
    if(chunks.find(key) != chunks.end())
        return;

    long long packed = packChunkKey(cx, cz);
    {
        std::lock_guard<std::mutex> lk(gRequestedMutex);
        if(gRequested.find(packed) != gRequested.end())
            return;
        gRequested.insert(packed);
    }

    {
        std::lock_guard<std::mutex> lk(gJobMutex);
        gJobQueue.push({cx, cz, false});
    }
    gJobCV.notify_one();
}

static void rebuildChunkAsync(int cx, int cz) {
    // Only rebuild if we already have the chunk loaded.
    std::pair<int,int> key = {cx, cz};
    if(chunks.find(key) == chunks.end())
        return;

    // Note: rebuilds can be spammy (water). We allow repeats but you can
    // add a "pending rebuild set" later if you want to reduce work.
    {
        std::lock_guard<std::mutex> lk(gJobMutex);
        gJobQueue.push({cx, cz, true});
    }
    gJobCV.notify_one();
}

// -----------------------------------------------------------------------------
// Render chunks
// -----------------------------------------------------------------------------
static void renderChunks(const Mat4 &view, const Mat4 &proj, const Vec3 &viewPos) {
    glUseProgram(worldShader);

    Mat4 VP = multiplyMatrix(proj, view);
    GLint mvpLoc = glGetUniformLocation(worldShader, "MVP");
    GLint texLoc = glGetUniformLocation(worldShader, "ourTexture");
    GLint sunLoc = glGetUniformLocation(worldShader, "sunDirection");
    GLint viewPosLoc = glGetUniformLocation(worldShader, "viewPos");

    glUniform1i(texLoc, 0);
    glUniform3f(sunLoc, -0.3f, 1.0f, -0.2f);
    glUniform3f(viewPosLoc, viewPos.x, viewPos.y, viewPos.z);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);

    for(auto &entry : chunks) {
        Chunk &chunk = entry.second;
        if(chunk.vertices.empty()) continue;
        glBindVertexArray(chunk.VAO);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, VP.m);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(chunk.vertices.size() / 5));
    }
    glBindVertexArray(0);
}

// -----------------------------------------------------------------------------
// Shaders and UI
// -----------------------------------------------------------------------------
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

// Simple UI (same as your existing)
int drawPauseMenu(int screenW, int screenH) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiShader);

    float overlayVerts[12] = {
        0, 0, (float)screenW, 0, (float)screenW, (float)screenH,
        0, 0, (float)screenW, (float)screenH, 0, (float)screenH
    };

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
    float resumeVerts[12] = {
        resumeX, resumeY, resumeX+resumeW, resumeY, resumeX+resumeW, resumeY+resumeH,
        resumeX, resumeY, resumeX+resumeW, resumeY+resumeH, resumeX, resumeY+resumeH
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(resumeVerts), resumeVerts);
    glUniform4f(glGetUniformLocation(uiShader, "uColor"), 0.2f, 0.6f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    float quitX = 300, quitY = 150, quitW = 200, quitH = 50;
    float quitVerts[12] = {
        quitX, quitY, quitX+quitW, quitY, quitX+quitW, quitY+quitH,
        quitX, quitY, quitX+quitW, quitY+quitH, quitX, quitY+quitH
    };
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

    glEnable(GL_DEPTH_TEST);
    return result;
}

void drawFlyIndicator(bool isFlying, int screenW, int screenH) {
    float w = 20.0f, h = 20.0f;
    float x = 5.0f;
    float y = (float)screenH - h - 5.0f;

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

void drawFirstPersonHand3D(int /*screenW*/, int /*screenH*/, const Mat4 &/*proj*/) {}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int /*argc*/, char* /*argv*/[]) {
    // These are FEET coords in the save file.
    float loadedX = 0.0f, loadedY = 30.0f, loadedZ = 0.0f;
    int loadedSeed = 0;

    bool loadedOk = loadWorld("saved_world.txt", loadedSeed, loadedX, loadedY, loadedZ);
    if(loadedOk) {
        std::cout << "[World] Loaded seed=" << loadedSeed
                  << " playerFeet(" << loadedX << "," << loadedY << "," << loadedZ << ")\n";
        sanitizeLoadedSpawn(loadedX, loadedY, loadedZ);
    } else {
        unsigned int rseed = (unsigned int)time(nullptr);
        std::cout << "[World] No saved world, random seed=" << rseed << "\n";
        setNoiseSeed(rseed);
        srand(rseed);
        loadedSeed = (int)rseed;
        findSafeSpawn(loadedX, loadedY, loadedZ);
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

    // Start worker
    std::thread worker(chunkWorkerThread);

    // Player FEET (collision base)
    Vec3 playerFeet = {loadedX, loadedY, loadedZ};

    Camera camera;
    camera.yaw = -3.14f/2;
    camera.pitch = 0.0f;
    camera.position = {playerFeet.x, playerFeet.y + EYE_HEIGHT, playerFeet.z};

    bool paused = false;
    bool isFlying = false;
    float verticalVelocity = 0.0f;
    float tickAccumulator = 0.0f;

    // Initial chunk requests (async, no stutter)
    int spawnChunkX = (int)std::floor(playerFeet.x / (float)chunkSize);
    int spawnChunkZ = (int)std::floor(playerFeet.z / (float)chunkSize);
    for(int cx = spawnChunkX - renderDistance; cx <= spawnChunkX + renderDistance; cx++){
        for(int cz = spawnChunkZ - renderDistance; cz <= spawnChunkZ + renderDistance; cz++){
            requestChunkAsync(cx, cz);
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

        // Update camera from feet (2 blocks high)
        camera.position = {playerFeet.x, playerFeet.y + EYE_HEIGHT, playerFeet.z};

        // Process finished chunks with a strict budget to prevent stutter.
        int uploadsThisFrame = 0;
        while(uploadsThisFrame < MAX_CHUNK_UPLOADS_PER_FRAME) {
            ChunkResult res;
            bool hasOne = false;
            {
                std::lock_guard<std::mutex> lk(gDoneMutex);
                if(!gDoneQueue.empty()) {
                    res = std::move(gDoneQueue.front());
                    gDoneQueue.pop();
                    hasOne = true;
                }
            }
            if(!hasOne) break;

            uploadChunkToGPU(res.cx, res.cz, res.verts, res.rebuild);

            // allow re-request if needed later (only for first-time requests)
            if(!res.rebuild) {
                std::lock_guard<std::mutex> lk(gRequestedMutex);
                gRequested.erase(packChunkKey(res.cx, res.cz));
            }

            uploadsThisFrame++;
        }

        tickAccumulator += dt;
        while(tickAccumulator >= TICK_INTERVAL) {
            tickAccumulator -= TICK_INTERVAL;
            updateWaterFlow(camera, dt);
        }

        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_QUIT)
                running = false;

            if(ev.type == SDL_KEYDOWN) {
                if(ev.key.keysym.sym == SDLK_ESCAPE) {
                    if(inventory.isOpen()) inventory.toggle();
                    paused = !paused;
                    SDL_SetRelativeMouseMode(paused ? SDL_FALSE : SDL_TRUE);
                }
                else if(ev.key.keysym.sym == SDLK_e) {
                    inventory.toggle();
                    SDL_SetRelativeMouseMode(inventory.isOpen() ? SDL_FALSE : SDL_TRUE);
                }
                else if(!paused && !inventory.isOpen() && !isFlying &&
                        ev.key.keysym.sym == SDLK_SPACE) {
                    Vec3 testFeet = playerFeet;
                    testFeet.y -= 0.05f;
                    if(checkCollisionFeet(testFeet))
                        verticalVelocity = JUMP_SPEED;
                }
                else if(ev.key.keysym.sym == SDLK_f) {
                    isFlying = !isFlying;
                    verticalVelocity = 0.0f;
                }
            }

            if(!paused && !inventory.isOpen()) {
                if(ev.type == SDL_MOUSEMOTION) {
                    float sensitivity = 0.0025f;
                    camera.yaw   += ev.motion.xrel * sensitivity;
                    camera.pitch -= ev.motion.yrel * sensitivity;
                    if(camera.pitch > 1.5f)  camera.pitch = 1.5f;
                    if(camera.pitch < -1.5f) camera.pitch = -1.5f;
                }
            }

            if(!paused && !inventory.isOpen() && ev.type == SDL_MOUSEBUTTONDOWN) {
                Vec3 forward = {
                    cosf(camera.pitch) * cosf(camera.yaw),
                    sinf(camera.pitch),
                    cosf(camera.pitch) * sinf(camera.yaw)
                };

                int bx, by, bz;
                if(!raycastBlock(camera.position, forward, 6.0f, bx, by, bz))
                    continue;

                if(ev.button.button == SDL_BUTTON_LEFT) {
                    // Break: carve out override
                    std::tuple<int,int,int> key = {bx, by, bz};
                    extraBlocks[key] = (BlockType)-1;

                    int ccx, ccz;
                    getChunkCoords(bx, bz, ccx, ccz);
                    rebuildChunkAsync(ccx, ccz);
                }
                else if(ev.button.button == SDL_BUTTON_RIGHT) {
                    Vec3 hitPos = { (float)bx + 0.5f, (float)by + 0.5f, (float)bz + 0.5f };
                    Vec3 diff = subtract(hitPos, camera.position);

                    int px=bx, py=by, pz=bz;
                    float ax = fabs(diff.x), ay = fabs(diff.y), az = fabs(diff.z);
                    if(ax > ay && ax > az) px += (diff.x > 0) ? -1 : 1;
                    else if(ay > ax && ay > az) py += (diff.y > 0) ? -1 : 1;
                    else pz += (diff.z > 0) ? -1 : 1;

                    int blockToPlace = inventory.getSelectedBlock();
                    if(blockToPlace != BLOCK_NONE) {
                        // Don’t place inside PLAYER (feet capsule)
                        Vec3 pos = playerFeet;
                        float half = playerWidth * 0.5f;
                        float minX = pos.x - half, maxX = pos.x + half;
                        float minY = pos.y,        maxY = pos.y + playerHeight;
                        float minZ = pos.z - half, maxZ = pos.z + half;

                        if(!(px + 1 > minX && px < maxX &&
                             py + 1 > minY && py < maxY &&
                             pz + 1 > minZ && pz < maxZ)) {

                            std::tuple<int,int,int> pKey = {px, py, pz};
                            extraBlocks[pKey] = (BlockType)blockToPlace;

                            int ccx, ccz;
                            getChunkCoords(px, pz, ccx, ccz);
                            rebuildChunkAsync(ccx, ccz);
                        }
                    }
                }
            }
        }

        if(paused) {
            int menuResult = drawPauseMenu(SCREEN_WIDTH, SCREEN_HEIGHT);
            if(menuResult == 1) {
                paused = false;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            } else if(menuResult == 2) {
                running = false;
            }
            SDL_GL_SwapWindow(window);
            continue;
        }

        // Movement
        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        Vec3 forward = {
            cosf(camera.pitch) * cosf(camera.yaw),
            sinf(camera.pitch),
            cosf(camera.pitch) * sinf(camera.yaw)
        };
        Vec3 right = {
            cosf(camera.yaw + 3.14159f/2.0f),
            0.0f,
            sinf(camera.yaw + 3.14159f/2.0f)
        };

        Vec3 move = {0,0,0};
        float speed = keys[SDL_SCANCODE_LSHIFT] ? 8.0f : 5.0f;

        if(!inventory.isOpen()){
            if(keys[SDL_SCANCODE_W]) { move.x += forward.x; move.z += forward.z; }
            if(keys[SDL_SCANCODE_S]) { move.x -= forward.x; move.z -= forward.z; }
            if(keys[SDL_SCANCODE_A]) { move.x -= right.x;   move.z -= right.z;   }
            if(keys[SDL_SCANCODE_D]) { move.x += right.x;   move.z += right.z;   }
        }

        float len = sqrtf(move.x*move.x + move.z*move.z);
        if(len > 0.0001f) { move.x/=len; move.z/=len; }

        Vec3 newFeet = playerFeet;

        // Horizontal move X
        newFeet.x += move.x * speed * dt;
        if(checkCollisionFeet(newFeet)) newFeet.x = playerFeet.x;

        // Horizontal move Z
        newFeet.z += move.z * speed * dt;
        if(checkCollisionFeet(newFeet)) newFeet.z = playerFeet.z;

        // Vertical
        if(isFlying) {
            if(!inventory.isOpen()){
                if(keys[SDL_SCANCODE_SPACE]) newFeet.y += speed * dt;
                if(keys[SDL_SCANCODE_LCTRL]) newFeet.y -= speed * dt;
            }
            verticalVelocity = 0.0f;
        } else {
            verticalVelocity += GRAVITY * dt;

            float targetY = newFeet.y + verticalVelocity * dt;
            float startY  = newFeet.y;
            float stepY   = (targetY > startY) ? 0.05f : -0.05f;

            float y = startY;
            while((stepY > 0.0f && y < targetY) || (stepY < 0.0f && y > targetY)) {
                float nextY = y + stepY;
                if(stepY > 0.0f && nextY > targetY) nextY = targetY;
                if(stepY < 0.0f && nextY < targetY) nextY = targetY;

                Vec3 testPos = newFeet;
                testPos.y = nextY;

                if(checkCollisionFeet(testPos)) {
                    verticalVelocity = 0.0f;
                    break;
                }
                y = nextY;
            }
            newFeet.y = y;
        }

        if(newFeet.y < WORLD_FLOOR_LIMIT) {
            float rx = newFeet.x, ry = newFeet.y, rz = newFeet.z;
            sanitizeLoadedSpawn(rx, ry, rz);
            newFeet.x = rx; newFeet.y = ry; newFeet.z = rz;
            verticalVelocity = 0.0f;
        }

        playerFeet = newFeet;

        // Load chunks around player (async)
        int pcx = (int)std::floor(playerFeet.x / (float)chunkSize);
        int pcz = (int)std::floor(playerFeet.z / (float)chunkSize);
        for(int cx = pcx - renderDistance; cx <= pcx + renderDistance; cx++){
            for(int cz = pcz - renderDistance; cz <= pcz + renderDistance; cz++){
                requestChunkAsync(cx, cz);
            }
        }

        // Update inventory logic
        inventory.update(dt, camera);

        // Render
        glViewport(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
        glClearColor(0.55f,0.75f,1.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 view = lookAtMatrix(camera.position,
                                 add(camera.position, forward),
                                 {0,1,0});

        renderChunks(view, projWorld, camera.position);

        // UI
        drawFlyIndicator(isFlying, SCREEN_WIDTH, SCREEN_HEIGHT);

        if(inventory.isOpen()){
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
        }

        SDL_GL_SwapWindow(window);
    }

    // Save FEET position (not eye)
    saveWorld("saved_world.txt", loadedSeed,
              playerFeet.x, playerFeet.y, playerFeet.z);

    // Stop worker
    gWorkerRunning.store(false);
    gJobCV.notify_all();
    if(worker.joinable())
        worker.join();

    glDeleteProgram(worldShader);
    glDeleteProgram(uiShader);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);

    for(auto &kv : chunks){
        glDeleteVertexArrays(1, &kv.second.VAO);
        glDeleteBuffers(1, &kv.second.VBO);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

