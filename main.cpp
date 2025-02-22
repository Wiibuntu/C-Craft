#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <tuple>
#include <cstdlib>   // for rand(), srand
#include <ctime>     // for time(NULL)

#include "math.h"
#include "shader.h"
#include "cube.h"
#include "camera.h"
#include "texture.h"
#include "noise.h"
#include "world.h"
#include "inventory.h"

// -----------------------------------------------------------------------------
// Window / screen
// -----------------------------------------------------------------------------
int SCREEN_WIDTH  = 1280;
int SCREEN_HEIGHT = 720;

// Terrain parameters
static const int chunkSize      = 16;
static const int renderDistance = 8;

// Player and world limits
static const float playerWidth  = 0.6f;
static const float playerHeight = 1.8f;
static const float WORLD_FLOOR_LIMIT = -10.0f;

// Gravity & jump speed
static const float GRAVITY    = -9.81f;
static const float JUMP_SPEED =  5.0f;

// 3D pipeline
GLuint worldShader = 0;
GLuint texID       = 0;

// 2D UI pipeline
GLuint uiShader    = 0;
GLuint uiVAO       = 0;
GLuint uiVBO       = 0;

// A chunk holds geometry for a 16x16 area.
struct Chunk {
    int chunkX, chunkZ;
    std::vector<float> vertices;
    GLuint VAO, VBO;
};

std::map<std::pair<int,int>, Chunk> chunks;  // chunk storage

// -----------------------------------------------------------------------------
// Biome definitions (added BIOME_OCEAN)
// -----------------------------------------------------------------------------
enum Biome {
    BIOME_PLAINS,
    BIOME_DESERT,
    BIOME_EXTREME_HILLS,
    BIOME_FOREST,
    BIOME_OCEAN
};

static Biome getBiome(int x, int z)
{
    // Use very low-frequency noise to determine ocean biome.
    float oceanNoise = perlinNoise(x * 0.001f, z * 0.001f);
    if(oceanNoise < -0.8f)
        return BIOME_OCEAN;
    float freq1 = 0.0035f, freq2 = 0.0037f;
    float n1 = perlinNoise(x * freq1, z * freq1);
    float n2 = perlinNoise((x+1000)*freq2, (z+1000)*freq2);
    float combined = 0.5f * (n1 + n2);
    if(combined < -0.4f)
        return BIOME_DESERT;
    else if(combined < -0.1f)
        return BIOME_PLAINS;
    else if(combined < 0.2f)
        return BIOME_FOREST;
    else
        return BIOME_EXTREME_HILLS;
}

int getTerrainHeightAt(int x, int z)
{
    Biome b = getBiome(x, z);
    if(b == BIOME_OCEAN)
        return 8; // flat ocean floor.
    if(b == BIOME_EXTREME_HILLS)
    {
        float freq = 0.0007f;
        int octaves = 8;
        float lacunarity = 2.3f, gain = 0.5f;
        float n = fbmNoise(x * freq, z * freq, octaves, lacunarity, gain);
        float normalized = 0.5f * (n + 1.0f);
        if(normalized < 0.0f) normalized = 0.0f;
        if(normalized > 1.0f) normalized = 1.0f;
        return (int)(powf(normalized, 2.0f) * 40.0f);
    }
    else
    {
        float n = fbmNoise(x * 0.01f, z * 0.01f, 6, 2.0f, 0.5f);
        float normalized = 0.5f * (n + 1.0f);
        return (int)(normalized * 24.0f);
    }
}

// -----------------------------------------------------------------------------
// Collision functions
// -----------------------------------------------------------------------------
static bool blockHasCollision(BlockType t)
{
    // Water is non-solid.
    // Tree leaves are now considered “breakable” so we do not treat them as non-hittable.
    return (t != BLOCK_WATER);
}

bool isSolidBlock(int bx, int by, int bz)
{
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

static bool checkCollision(const Vec3 &pos)
{
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

// -----------------------------------------------------------------------------
// generateChunk: Generates terrain for a chunk.
// This function carves out rivers (in non-desert/non-ocean biomes),
// spawns oases (now 5× more frequent), and generates ocean columns.
// Trees are generated only on normal terrain (not in lakes, rivers, or ocean).
// -----------------------------------------------------------------------------
static Chunk generateChunk(int cx, int cz)
{
    Chunk chunk;
    chunk.chunkX = cx;
    chunk.chunkZ = cz;
    std::vector<float> verts;
    verts.reserve(16 * 16 * 36 * 5);

    // Use a per-chunk seed for independent outcomes.
    unsigned int chunkSeed = (unsigned int)(cx * 73856093u ^ cz * 19349663u);
    Biome chunkBiome = getBiome(cx * 16 + 8, cz * 16 + 8);
    bool hasLake = false;
    int lakeCenterX = 0, lakeCenterZ = 0, lakeRadius = 0, lakeWaterLevel = 0;
    // Increase oasis chance 5× (now 1 in 40) for non-desert, non-ocean chunks.
    if(chunkBiome != BIOME_DESERT && chunkBiome != BIOME_OCEAN && (chunkSeed % 40 == 0))
    {
        hasLake = true;
        lakeCenterX = cx * 16 + (rand() % 16);
        lakeCenterZ = cz * 16 + (rand() % 16);
        lakeRadius = 2 + (rand() % 7); // radius between 2 and 8
        int terrainAtCenter = getTerrainHeightAt(lakeCenterX, lakeCenterZ);
        lakeWaterLevel = terrainAtCenter - 2;
        if(lakeWaterLevel < 0)
            lakeWaterLevel = 0;
    }

    // Loop over every column in the chunk.
    for(int lx = 0; lx < 16; lx++){
        for(int lz = 0; lz < 16; lz++){
            int wx = cx * 16 + lx;
            int wz = cz * 16 + lz;
            int height = getTerrainHeightAt(wx, wz);
            Biome b = getBiome(wx, wz);
            bool inLake = false;
            bool inRiver = false;
            if(hasLake){
                int dx = wx - lakeCenterX, dz = wz - lakeCenterZ;
                if(dx*dx + dz*dz <= lakeRadius * lakeRadius)
                    inLake = true;
            }
            if(b != BIOME_DESERT && b != BIOME_OCEAN){
                float riverNoise = perlinNoise(wx * 0.1f, wz * 0.1f);
                if(riverNoise < -0.85f)
                    inRiver = true;
            }

            if(b == BIOME_OCEAN)
            {
                int newHeight = 7;
                for(int y = 0; y <= newHeight; y++){
                    addCube(verts, (float)wx, (float)y, (float)wz, BLOCK_SAND);
                }
                addCube(verts, (float)wx, 8.0f, (float)wz, BLOCK_WATER);
                waterLevels[{wx, 8, wz}] = 8;
            }
            else if(inLake)
            {
                int newHeight = lakeWaterLevel - 1;
                if(newHeight < 0) newHeight = 0;
                for(int y = 0; y <= newHeight; y++){
                    BlockType type;
                    if(b == BIOME_DESERT){
                        const int sandLayers = 2, dirtLayers = 3;
                        if(y >= newHeight - sandLayers)
                            type = BLOCK_SAND;
                        else if(y >= newHeight - (sandLayers + dirtLayers))
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    }
                    else {
                        if(y == newHeight)
                            type = BLOCK_DIRT;
                        else if((newHeight - y) <= 6)
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    }
                    addCube(verts, (float)wx, (float)y, (float)wz, type);
                }
                addCube(verts, (float)wx, (float)lakeWaterLevel, (float)wz, BLOCK_WATER);
                waterLevels[{wx, lakeWaterLevel, wz}] = 8;
                for(int y = lakeWaterLevel + 1; y <= height; y++){
                    extraBlocks[{wx, y, wz}] = (BlockType)(-1);
                }
            }
            else if(inRiver)
            {
                int riverWaterLevel = 7;
                int newHeight = riverWaterLevel - 1;
                if(newHeight < 0) newHeight = 0;
                for(int y = 0; y <= newHeight; y++){
                    BlockType type;
                    if(b == BIOME_DESERT){
                        const int sandLayers = 2, dirtLayers = 3;
                        if(y >= newHeight - sandLayers)
                            type = BLOCK_SAND;
                        else if(y >= newHeight - (sandLayers + dirtLayers))
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    }
                    else {
                        if(y == newHeight)
                            type = BLOCK_DIRT;
                        else if((newHeight - y) <= 6)
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    }
                    addCube(verts, (float)wx, (float)y, (float)wz, type);
                }
                addCube(verts, (float)wx, (float)riverWaterLevel, (float)wz, BLOCK_WATER);
                waterLevels[{wx, riverWaterLevel, wz}] = 8;
                for(int y = riverWaterLevel + 1; y <= height; y++){
                    extraBlocks[{wx, y, wz}] = (BlockType)(-1);
                }
            }
            else
            {
                // Normal terrain.
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
                    }
                    else {
                        if(y == height)
                            type = BLOCK_GRASS;
                        else if((height - y) <= 6)
                            type = BLOCK_DIRT;
                        else
                            type = BLOCK_STONE;
                    }
                    addCube(verts, (float)wx, (float)y, (float)wz, type);
                }
                // Tree generation.
                int chance = 0;
                if(b == BIOME_FOREST) chance = 5;
                else if(b == BIOME_PLAINS) chance = 50;
                else if(b == BIOME_EXTREME_HILLS) chance = 80;
                if(chance > 0 && (rand() % chance == 0))
                {
                    int trunkH = 4 + (rand() % 3);
                    int baseY = height + 1;
                    for(int ty = baseY; ty < baseY + trunkH; ty++){
                        addCube(verts, (float)wx, (float)ty, (float)wz, BLOCK_TREE_LOG);
                        extraBlocks[{wx, ty, wz}] = BLOCK_TREE_LOG;
                    }
                    int topY = baseY + trunkH - 1;
                    for(int lx2 = wx - 1; lx2 <= wx + 1; lx2++){
                        for(int lz2 = wz - 1; lz2 <= wz + 1; lz2++){
                            if(lx2 == wx && lz2 == wz)
                                continue;
                            addCube(verts, (float)lx2, (float)topY, (float)lz2, BLOCK_LEAVES);
                            extraBlocks[{lx2, topY, lz2}] = BLOCK_LEAVES;
                        }
                    }
                    addCube(verts, (float)wx, (float)(topY + 1), (float)wz, BLOCK_LEAVES);
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

// -----------------------------------------------------------------------------
// rebuildChunk: Ensures that water blocks in waterLevels are retained when
// chunks are rebuilt, preventing them from being overwritten by terrain blocks.
// -----------------------------------------------------------------------------
static void rebuildChunk(int cx, int cz)
{
    Chunk &chunk = chunks[{cx, cz}];
    std::vector<float> verts;
    verts.reserve(16 * 16 * 36 * 5);
    
    for (int lx = 0; lx < 16; lx++) {
        for (int lz = 0; lz < 16; lz++) {
            int wx = cx * 16 + lx;
            int wz = cz * 16 + lz;
            int height = getTerrainHeightAt(wx, wz);

            for (int y = 0; y <= height; y++) {
                std::tuple<int, int, int> key = {wx, y, wz};

                // **Check if this position contains water first**
                if (waterLevels.find(key) != waterLevels.end()) {
                    addCube(verts, (float)wx, (float)y, (float)wz, BLOCK_WATER);
                    continue;  // Skip normal terrain generation
                }

                // **Check for extraBlocks (modifications, trees, etc.)**
                auto it = extraBlocks.find(key);
                if (it != extraBlocks.end()) {
                    BlockType ov = it->second;
                    if ((int)ov < 0) continue;  // Skip air/removed blocks
                    addCube(verts, (float)wx, (float)y, (float)wz, ov);
                }
                else {
                    // **Regular terrain generation (if no water or extra blocks exist)**
                    Biome b = getBiome(wx, wz);
                    BlockType terr;
                    if (b == BIOME_DESERT) {
                        const int sandLayers = 2, dirtLayers = 3;
                        if (y >= height - sandLayers)
                            terr = BLOCK_SAND;
                        else if (y >= height - (sandLayers + dirtLayers))
                            terr = BLOCK_DIRT;
                        else
                            terr = BLOCK_STONE;
                    }
                    else {
                        if (y == height)
                            terr = BLOCK_GRASS;
                        else if ((height - y) <= 6)
                            terr = BLOCK_DIRT;
                        else
                            terr = BLOCK_STONE;
                    }
                    addCube(verts, (float)wx, (float)y, (float)wz, terr);
                }
            }

            // **Ensure tree parts are placed above the terrain level**
            for (int y = height + 1; y < height + 20; y++) {
                std::tuple<int, int, int> key = {wx, y, wz};
                if (extraBlocks.find(key) != extraBlocks.end()) {
                    BlockType ov = extraBlocks[key];
                    if ((int)ov < 0) continue;
                    addCube(verts, (float)wx, (float)y, (float)wz, ov);
                }
            }
        }
    }

    // **Ensure water blocks are retained even if chunk updates**
    for (auto &kv : waterLevels) {
        int bx = std::get<0>(kv.first);
        int by = std::get<1>(kv.first);
        int bz = std::get<2>(kv.first);
        int ccx = bx / 16; if (bx < 0 && bx % 16 != 0) ccx--;
        int ccz = bz / 16; if (bz < 0 && bz % 16 != 0) ccz--;
        if (ccx == cx && ccz == cz)
            addCube(verts, (float)bx, (float)by, (float)bz, BLOCK_WATER);
    }

    // Upload new vertex data
    chunk.vertices = verts;
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}


// -----------------------------------------------------------------------------
// updateWaterFlow: New water flow simulation.
// For each water cell, if the cell below is empty, flow downward (becoming a source).
// Otherwise, if water level > 1, flow horizontally by setting the neighbor's level
// to current level minus one. This naturally limits horizontal spread to 6 blocks.
// -----------------------------------------------------------------------------
static void updateWaterFlow(float /*dt*/)
{
    std::map<std::tuple<int,int,int>, int> newWater = waterLevels;
    bool updated = false;
    for(auto &entry : waterLevels)
    {
        int x, y, z;
        std::tie(x, y, z) = entry.first;
        int level = entry.second;
        // Flow downward.
        if(y > 0 && !isSolidBlock(x, y-1, z)) {
            std::tuple<int,int,int> below = {x, y-1, z};
            int currentBelow = 0;
            if(newWater.find(below) != newWater.end())
                currentBelow = newWater[below];
            if(8 > currentBelow) {
                newWater[below] = 8;
                updated = true;
            }
        }
        // Flow horizontally.
        if(level > 1) {
            int offsets[4][3] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
            for(int i = 0; i < 4; i++){
                int nx = x + offsets[i][0];
                int ny = y;
                int nz = z + offsets[i][2];
                std::tuple<int,int,int> neighbor = {nx, ny, nz};
                int neighborLevel = 0;
                if(newWater.find(neighbor) != newWater.end())
                    neighborLevel = newWater[neighbor];
                int newLevel = level - 1;
                if(newLevel > neighborLevel && newLevel > 1) {
                    newWater[neighbor] = newLevel;
                    updated = true;
                }
            }
        }
    }
    waterLevels = newWater;
}

// -----------------------------------------------------------------------------
// raycastBlock: Modified to return a hit for tree leaves so that they can be broken.
// -----------------------------------------------------------------------------
static bool raycastBlock(const Vec3 &start, const Vec3 &dir, float maxDist,
                         int &outX, int &outY, int &outZ)
{
    float step = 0.1f, traveled = 0.0f;
    while(traveled < maxDist){
        Vec3 pos = add(start, multiply(dir, traveled));
        int bx = (int)std::floor(pos.x);
        int by = (int)std::floor(pos.y);
        int bz = (int)std::floor(pos.z);
        std::tuple<int,int,int> key = {bx, by, bz};
        if(isSolidBlock(bx, by, bz) ||
           (extraBlocks.find(key) != extraBlocks.end() && extraBlocks[key] == BLOCK_LEAVES))
        {
            outX = bx; outY = by; outZ = bz;
            return true;
        }
        traveled += step;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Shaders and UI drawing functions (unchanged)
// -----------------------------------------------------------------------------
static const char* worldVertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTex;
out vec2 TexCoord;
uniform mat4 MVP;
void main(){
    gl_Position = MVP * vec4(aPos, 1.0);
    TexCoord = aTex;
}
)";

static const char* worldFragSrc = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;
void main(){
    FragColor = texture(ourTexture, TexCoord);
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

static void initUI()
{
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

static void drawRect2D(float x, float y, float w, float h,
                       float r, float g, float b, float a,
                       int screenW, int screenH)
{
    float vertices[12] = { x, y, x+w, y, x+w, y+h, x, y, x+w, y+h, x, y+h };
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    Mat4 proj = {};
    proj.m[0]  = 2.0f/(float)screenW;
    proj.m[5]  = 2.0f/(float)screenH;
    proj.m[10] = -1.0f;
    proj.m[15] = 1.0f;
    proj.m[12] = -1.0f;
    proj.m[13] = -1.0f;
    glUseProgram(uiShader);
    GLint pLoc = glGetUniformLocation(uiShader, "uProj");
    glUniformMatrix4fv(pLoc, 1, GL_FALSE, proj.m);
    GLint cLoc = glGetUniformLocation(uiShader, "uColor");
    glUniform4f(cLoc, r, g, b, a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static int drawPauseMenu(int screenW, int screenH)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(uiShader);
    drawRect2D(0, 0, (float)screenW, (float)screenH, 0.0f, 0.0f, 0.0f, 0.5f, screenW, screenH);
    float bx = 300, by = 250, bw = 200, bh = 50;
    drawRect2D(bx, by, bw, bh, 0.2f, 0.6f, 1.0f, 1.0f, screenW, screenH);
    float qx = 300, qy = 150, qw = 200, qh = 50;
    drawRect2D(qx, qy, qw, qh, 1.0f, 0.3f, 0.3f, 1.0f, screenW, screenH);
    int mx, my;
    Uint32 mState = SDL_GetMouseState(&mx, &my);
    bool leftDown = (mState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    int invY = screenH - my;
    int result = 0;
    if(leftDown) {
        if(mx >= bx && mx <= (bx + bw) && invY >= by && invY <= (by + bh))
            result = 1;
        else if(mx >= qx && mx <= (qx + qw) && invY >= qy && invY <= (qy + qh))
            result = 2;
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    return result;
}

static void drawFlyIndicator(bool isFlying, int screenW, int screenH)
{
    float w = 20.0f, h = 20.0f, x = 5.0f, y = (float)screenH - h - 5.0f;
    float r = 1.0f, g = 0.0f, b = 0.0f;
    if(isFlying){
        r = 0.1f; g = 1.0f; b = 0.1f;
    }
    glDisable(GL_DEPTH_TEST);
    glUseProgram(uiShader);
    drawRect2D(x, y, w, h, r, g, b, 1.0f, screenW, screenH);
    glEnable(GL_DEPTH_TEST);
}

static void getChunkCoords(int bx, int bz, int &cx, int &cz)
{
    cx = bx / 16; if(bx < 0 && bx % 16 != 0) cx--;
    cz = bz / 16; if(bz < 0 && bz % 16 != 0) cz--;
}

int main(int /*argc*/, char* /*argv*/[])
{
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
    initUI();
    Inventory inventory;
    int spawnChunkX = (int)std::floor(loadedX / (float)chunkSize);
    int spawnChunkZ = (int)std::floor(loadedZ / (float)chunkSize);
    auto chunkKey = std::make_pair(spawnChunkX, spawnChunkZ);
    if(chunks.find(chunkKey) == chunks.end())
        chunks[chunkKey] = generateChunk(spawnChunkX, spawnChunkZ);
    Camera camera;
    camera.position = {loadedX, loadedY, loadedZ};
    camera.yaw = -3.14f/2;
    camera.pitch = 0.0f;
    bool paused = false, isFlying = false;
    float verticalVelocity = 0.0f;
    if(loadedOk) {
        int pcx = (int)std::floor(camera.position.x / (float)chunkSize);
        int pcz = (int)std::floor(camera.position.z / (float)chunkSize);
        for(int cx = pcx - renderDistance; cx <= pcx + renderDistance; cx++){
            for(int cz = pcz - renderDistance; cz <= pcz + renderDistance; cz++){
                auto cKey = std::make_pair(cx, cz);
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
    while(running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) * 0.001f;
        lastTime = now;
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
            for(auto &pair : chunks){
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
            if(isFlying){
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
            }
            else {
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
                auto key = std::make_pair(cx, cz);
                if(chunks.find(key) == chunks.end())
                    chunks[key] = generateChunk(cx, cz);
            }
        }
        updateWaterFlow(dt);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(worldShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        GLint uniTex = glGetUniformLocation(worldShader, "ourTexture");
        glUniform1i(uniTex, 0);
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

