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
#include <sstream>
#include <iomanip>

#include "math.h"
#include "shader.h"
#include "cube.h"
#include "camera.h"
#include "texture.h"
#include "noise.h"
#include "world.h"
#include "inventory.h"
#include "globals.h"

// -------------------- GLOBALS --------------------
GLuint handTex = 0;

// Screen (declared extern in globals.h)
int SCREEN_WIDTH  = 960;
int SCREEN_HEIGHT = 540;

// UI shader + quad buffer
GLuint uiShader = 0;
GLuint uiVAO = 0;
GLuint uiVBO = 0;

// World shader/texture (declared extern in globals.h)
GLuint worldShader = 0;
GLuint texID = 0;

// Chunk settings
static const int chunkSize = 16;
static const int renderDistance = 10;
static const int MAX_CHUNK_UPLOADS_PER_FRAME = 2;

// Sea level + vertical limits
static const int SEA_LEVEL = 24;
static const int MAX_WORLD_Y = 112;
static const int MIN_WORLD_Y = 0;

// Timing
static const float TICK_INTERVAL = 0.5f;

// Player physics
static const float playerWidth  = 0.6f;
static const float playerHeight = 2.0f;
static const float EYE_HEIGHT   = 2.0f; // camera 2 blocks high
static const float WORLD_FLOOR_LIMIT = -10.0f;
static const float GRAVITY = -9.81f;
static const float JUMP_SPEED = 5.0f;

// -------------------- HELPERS --------------------
static float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }

Mat4 translateMatrix(float tx, float ty, float tz) {
    Mat4 mat = identityMatrix();
    mat.m[12] = tx; mat.m[13] = ty; mat.m[14] = tz;
    return mat;
}
Mat4 scaleMatrix(float sx, float sy, float sz) {
    Mat4 mat = identityMatrix();
    mat.m[0] = sx; mat.m[5] = sy; mat.m[10] = sz;
    return mat;
}
static float smoothstep(float edge0, float edge1, float x) {
    float t = (x - edge0) / (edge1 - edge0);
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static float mix(float a, float b, float t) { return a + t * (b - a); }

// Local "pseudo-3D" noise built from 2D perlin/fbm, so we don't need noise.h changes.
static float noise3Pseudo(float x, float y, float z) {
    float a = perlinNoise(x, y);
    float b = perlinNoise(y, z);
    float c = perlinNoise(x, z);
    return (a + b + c) * (1.0f / 3.0f); // roughly [-1..1]
}
static float fbmNoise3Pseudo(float x, float y, float z, int octaves, float lacunarity, float gain) {
    float amp = 1.0f;
    float freq = 1.0f;
    float sum = 0.0f;
    float norm = 0.0f;
    for(int i=0;i<octaves;i++){
        sum += noise3Pseudo(x*freq, y*freq, z*freq) * amp;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    if(norm > 0.0f) sum /= norm;
    return sum;
}

// -------------------- BIOMES --------------------
enum Biome {
    BIOME_OCEAN,
    BIOME_BEACH,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_DESERT,
    BIOME_TAIGA,
    BIOME_SWAMP,
    BIOME_EXTREME_HILLS
};

static const char* biomeToString(Biome b) {
    switch(b) {
        case BIOME_OCEAN:         return "OCEAN";
        case BIOME_BEACH:         return "BEACH";
        case BIOME_PLAINS:        return "PLAINS";
        case BIOME_FOREST:        return "FOREST";
        case BIOME_DESERT:        return "DESERT";
        case BIOME_TAIGA:         return "TAIGA";
        case BIOME_SWAMP:         return "SWAMP";
        case BIOME_EXTREME_HILLS: return "EXTREME_HILLS";
        default:                  return "UNKNOWN";
    }
}

// -------------------- WORLD STATE --------------------
struct Chunk {
    int chunkX, chunkZ;
    std::vector<float> vertices;
    GLuint VAO = 0, VBO = 0;
};
std::unordered_map<std::pair<int,int>, Chunk, PairHash> chunks;

// World state accessed from worker + main thread
static std::mutex gWorldMutex;

// -------------------- ASYNC CHUNK PIPELINE --------------------
struct ChunkJob { int cx, cz; bool rebuild; };
struct ChunkResult { int cx, cz; std::vector<float> verts; bool rebuild; };

static std::mutex gJobMutex;
static std::condition_variable gJobCV;
static std::queue<ChunkJob> gJobQueue;

static std::mutex gDoneMutex;
static std::queue<ChunkResult> gDoneQueue;

static std::atomic<bool> gWorkerRunning{true};

static std::mutex gRequestedMutex;
static std::unordered_set<long long> gRequested;

static long long packChunkKey(int cx, int cz) {
    return ((long long)cx << 32) ^ (unsigned int)cz;
}

// -------------------- SHADERS --------------------
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
    if(texColor.a < 0.1) discard;

    FragColor = vec4(texColor.rgb * lighting, texColor.a);
}
)";

static const char* uiVertSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform mat4 uProj;
void main(){ gl_Position = uProj * vec4(aPos, 0.0, 1.0); }
)";

static const char* uiFragSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main(){ FragColor = uColor; }
)";

// -------------------- UI INIT --------------------
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

// Ortho proj in screen pixels (origin bottom-left for our UI coords)
static Mat4 orthoPixels(int w, int h) {
    Mat4 proj = {};
    proj.m[0]  = 2.0f/(float)w;
    proj.m[5]  = 2.0f/(float)h;
    proj.m[10] = -1.0f;
    proj.m[15] = 1.0f;
    proj.m[12] = -1.0f;
    proj.m[13] = -1.0f;
    return proj;
}

static void uiDrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float verts[12] = {
        x,   y,
        x+w, y,
        x+w, y+h,
        x,   y,
        x+w, y+h,
        x,   y+h
    };

    glUseProgram(uiShader);
    Mat4 proj = orthoPixels(SCREEN_WIDTH, SCREEN_HEIGHT);
    glUniformMatrix4fv(glGetUniformLocation(uiShader, "uProj"), 1, GL_FALSE, proj.m);
    glUniform4f(glGetUniformLocation(uiShader, "uColor"), r, g, b, a);

    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// -------------------- 5x7 DEV FONT --------------------
// Each glyph is 5 wide x 7 tall, stored as 7 rows of 5 bits (MSB->LSB not important; we use bit tests).
static void glyph5x7(char c, uint8_t outRows[7]) {
    // default = blank
    for(int i=0;i<7;i++) outRows[i]=0;

    // uppercase everything to keep table small
    if(c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');

    switch(c) {
        case ' ': return;

        case '-': { uint8_t r[7]={0,0,0,0b11111,0,0,0}; memcpy(outRows,r,7); return; }
        case '_': { uint8_t r[7]={0,0,0,0,0,0,0b11111}; memcpy(outRows,r,7); return; }
        case ':': { uint8_t r[7]={0,0b00100,0b00100,0,0b00100,0b00100,0}; memcpy(outRows,r,7); return; }
        case '.': { uint8_t r[7]={0,0,0,0,0,0b00100,0b00100}; memcpy(outRows,r,7); return; }

        // digits
        case '0': { uint8_t r[7]={0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case '1': { uint8_t r[7]={0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}; memcpy(outRows,r,7); return; }
        case '2': { uint8_t r[7]={0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111}; memcpy(outRows,r,7); return; }
        case '3': { uint8_t r[7]={0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110}; memcpy(outRows,r,7); return; }
        case '4': { uint8_t r[7]={0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}; memcpy(outRows,r,7); return; }
        case '5': { uint8_t r[7]={0b11111,0b10000,0b10000,0b11110,0b00001,0b00001,0b11110}; memcpy(outRows,r,7); return; }
        case '6': { uint8_t r[7]={0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case '7': { uint8_t r[7]={0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}; memcpy(outRows,r,7); return; }
        case '8': { uint8_t r[7]={0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case '9': { uint8_t r[7]={0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100}; memcpy(outRows,r,7); return; }

        // letters A-Z (minimal set used by overlay/biomes; included full for safety)
        case 'A': { uint8_t r[7]={0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}; memcpy(outRows,r,7); return; }
        case 'B': { uint8_t r[7]={0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}; memcpy(outRows,r,7); return; }
        case 'C': { uint8_t r[7]={0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case 'D': { uint8_t r[7]={0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110}; memcpy(outRows,r,7); return; }
        case 'E': { uint8_t r[7]={0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}; memcpy(outRows,r,7); return; }
        case 'F': { uint8_t r[7]={0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}; memcpy(outRows,r,7); return; }
        case 'G': { uint8_t r[7]={0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case 'H': { uint8_t r[7]={0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}; memcpy(outRows,r,7); return; }
        case 'I': { uint8_t r[7]={0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110}; memcpy(outRows,r,7); return; }
        case 'J': { uint8_t r[7]={0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100}; memcpy(outRows,r,7); return; }
        case 'K': { uint8_t r[7]={0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001}; memcpy(outRows,r,7); return; }
        case 'L': { uint8_t r[7]={0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}; memcpy(outRows,r,7); return; }
        case 'M': { uint8_t r[7]={0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}; memcpy(outRows,r,7); return; }
        case 'N': { uint8_t r[7]={0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001}; memcpy(outRows,r,7); return; }
        case 'O': { uint8_t r[7]={0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case 'P': { uint8_t r[7]={0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000}; memcpy(outRows,r,7); return; }
        case 'Q': { uint8_t r[7]={0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101}; memcpy(outRows,r,7); return; }
        case 'R': { uint8_t r[7]={0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}; memcpy(outRows,r,7); return; }
        case 'S': { uint8_t r[7]={0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}; memcpy(outRows,r,7); return; }
        case 'T': { uint8_t r[7]={0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}; memcpy(outRows,r,7); return; }
        case 'U': { uint8_t r[7]={0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}; memcpy(outRows,r,7); return; }
        case 'V': { uint8_t r[7]={0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100}; memcpy(outRows,r,7); return; }
        case 'W': { uint8_t r[7]={0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010}; memcpy(outRows,r,7); return; }
        case 'X': { uint8_t r[7]={0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001}; memcpy(outRows,r,7); return; }
        case 'Y': { uint8_t r[7]={0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100}; memcpy(outRows,r,7); return; }
        case 'Z': { uint8_t r[7]={0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111}; memcpy(outRows,r,7); return; }

        default: return;
    }
}

static void uiDrawText(float x, float y, const std::string &text, float scale,
                       float r, float g, float b, float a,
                       bool shadow)
{
    // Our UI coords are bottom-left origin.
    // For "top-left overlay", caller should convert using SCREEN_HEIGHT.
    const float px = scale;
    const float py = scale;
    const float charW = 6.0f * px;   // 5 px glyph + 1 px gap
    const float charH = 8.0f * py;   // 7 px glyph + 1 px gap

    auto drawPass = [&](float ox, float oy, float rr, float gg, float bb, float aa){
        float cx = x + ox;
        float cy = y + oy;
        for(char c : text) {
            uint8_t rows[7];
            glyph5x7(c, rows);
            for(int row=0; row<7; row++){
                uint8_t bits = rows[row];
                for(int col=0; col<5; col++){
                    if(bits & (1u << (4-col))) {
                        uiDrawRect(cx + col*px, cy + (6-row)*py, px, py, rr, gg, bb, aa);
                    }
                }
            }
            cx += charW;
        }
    };

    if(shadow) {
        drawPass(scale, -scale, 0.0f, 0.0f, 0.0f, a*0.75f);
    }
    drawPass(0.0f, 0.0f, r, g, b, a);
}

// -------------------- BIOME MAP / HEIGHT --------------------
static void getClimate(float x, float z, float &temp01, float &humid01, float &weird01)
{
    float t = fbmNoise(x * 0.0008f + 100.0f, z * 0.0008f - 100.0f, 4, 2.0f, 0.5f);
    float h = fbmNoise(x * 0.0008f - 200.0f, z * 0.0008f + 200.0f, 4, 2.0f, 0.5f);
    float w = fbmNoise(x * 0.00035f + 999.0f, z * 0.00035f - 999.0f, 3, 2.0f, 0.5f);

    temp01  = clampf((t + 1.0f) * 0.5f, 0.0f, 1.0f);
    humid01 = clampf((h + 1.0f) * 0.5f, 0.0f, 1.0f);
    weird01 = clampf((w + 1.0f) * 0.5f, 0.0f, 1.0f);
}

static float ridge2D(float x, float z, float freq)
{
    float n = fbmNoise(x * freq, z * freq, 5, 2.0f, 0.5f);
    float r = 1.0f - std::fabs(n);
    return r * r;
}

static float oceanMask(float x, float z)
{
    return fbmNoise(x * 0.0006f, z * 0.0006f, 4, 2.0f, 0.5f);
}

static Biome getBiome(int x, int z)
{
    float temp, humid, weird;
    getClimate((float)x, (float)z, temp, humid, weird);

    float ocean = oceanMask((float)x, (float)z);
    if(ocean < -0.35f)
        return BIOME_OCEAN;

    // Rare extreme hills
    float r = ridge2D((float)x, (float)z, 0.0010f);
    float rare = fbmNoise(x * 0.00018f + 50.0f, z * 0.00018f - 50.0f, 2, 2.0f, 0.5f);
    float rare01 = (rare + 1.0f) * 0.5f;
    if(r > 0.72f && weird > 0.62f && rare01 > 0.72f)
        return BIOME_EXTREME_HILLS;

    if(humid > 0.78f && temp > 0.35f)
        return BIOME_SWAMP;

    if(temp > 0.70f && humid < 0.32f)
        return BIOME_DESERT;

    if(temp < 0.28f)
        return BIOME_TAIGA;

    if(humid > 0.55f)
        return BIOME_FOREST;

    return BIOME_PLAINS;
}

// "Near ocean" test for desert water rule
static bool nearOcean(int x, int z)
{
    float o0 = oceanMask((float)x, (float)z);
    if(o0 < -0.20f) return true;

    const int d = 32;
    float o1 = oceanMask((float)(x + d), (float)z);
    float o2 = oceanMask((float)(x - d), (float)z);
    float o3 = oceanMask((float)x, (float)(z + d));
    float o4 = oceanMask((float)x, (float)(z - d));
    return (o1 < -0.25f || o2 < -0.25f || o3 < -0.25f || o4 < -0.25f);
}

static int getHeight2D(int x, int z, Biome b)
{
    float xf = (float)x;
    float zf = (float)z;

    float macro = fbmNoise(xf * 0.0015f + 3000.0f, zf * 0.0015f - 3000.0f, 5, 2.0f, 0.5f);
    float base  = fbmNoise(xf * 0.0065f,          zf * 0.0065f,          5, 2.0f, 0.5f);
    float detail= fbmNoise(xf * 0.020f + 1000.0f, zf * 0.020f - 1000.0f, 4, 2.0f, 0.5f);

    float ridged = ridge2D(xf, zf, 0.0035f);
    float cliffy = (ridged > 0.72f) ? (ridged - 0.72f) * 18.0f : 0.0f;

    float h = (float)SEA_LEVEL;
    h += macro  * 18.0f;
    h += base   * 10.0f;
    h += detail *  4.0f;
    h += cliffy;

    float ocean = oceanMask(xf, zf);
    if(ocean < -0.15f) {
        float t = smoothstep(-0.35f, -0.15f, ocean);
        h = mix((float)SEA_LEVEL - 20.0f, h, t);
    }

    if(b == BIOME_DESERT) h += 2.5f;
    if(b == BIOME_SWAMP)  h = std::min(h, (float)SEA_LEVEL + 2.0f);
    if(b == BIOME_FOREST) h += 1.0f;

    h = clampf(h, 4.0f, 90.0f);
    return (int)std::floor(h);
}

static int getExtremeHillsBase(int x, int z)
{
    float xf = (float)x, zf = (float)z;

    float bigRidge = ridge2D(xf, zf, 0.0010f);
    float midRidge = ridge2D(xf + 100.0f, zf - 100.0f, 0.0022f);

    float shape = bigRidge * 0.75f + midRidge * 0.25f;
    float peaks = shape * shape;

    float macro = fbmNoise(xf * 0.0012f + 777.0f, zf * 0.0012f - 777.0f, 4, 2.0f, 0.5f);
    float noise = fbmNoise(xf * 0.006f, zf * 0.006f, 4, 2.0f, 0.5f);

    float h = (float)SEA_LEVEL + 12.0f;
    h += peaks * 60.0f;
    h += macro * 10.0f;
    h += noise * 5.0f;

    h = clampf(h, (float)SEA_LEVEL + 10.0f, 104.0f);
    return (int)h;
}

// Caves/overhang-ish carve using pseudo-3D
static bool isCaveCarve(int x, int y, int z, int surfaceY)
{
    if(y >= surfaceY - 4) return false;
    if(y <= 4) return false;

    float nx = x * 0.075f;
    float ny = y * 0.075f;
    float nz = z * 0.075f;

    float n = fbmNoise3Pseudo(nx, ny, nz, 3, 2.0f, 0.5f);
    return (n > 0.55f);
}

// Extreme hills density field (also uses pseudo-3D)
static bool mountainDensity(int x, int y, int z, int baseSurface)
{
    float xf = (float)x;
    float yf = (float)y;
    float zf = (float)z;

    float d = (float)(baseSurface - y);

    float over = fbmNoise3Pseudo(xf * 0.035f, yf * 0.040f, zf * 0.035f, 4, 2.0f, 0.5f);
    float shelves = fbmNoise3Pseudo(xf * 0.020f + 200.0f, yf * 0.030f - 50.0f, zf * 0.020f + 300.0f,
                                    3, 2.0f, 0.5f);

    float cliff = ridge2D(xf, zf, 0.0024f);
    float cliffBoost = (cliff > 0.65f) ? (cliff - 0.65f) * 12.0f : 0.0f;

    float density = d + (over * 10.0f) + (shelves * 6.0f) + cliffBoost;

    if(y > baseSurface + 12)
        density -= (float)(y - (baseSurface + 12)) * 1.2f;

    return density > 0.0f;
}

// -------------------- SURFACE BLOCKS --------------------
static BlockType surfaceTopForBiome(Biome b, int surfaceY)
{
    if(b == BIOME_DESERT) return BLOCK_SAND;
    if(b == BIOME_BEACH)  return BLOCK_SAND;

    if(b == BIOME_EXTREME_HILLS) {
        if(surfaceY >= SEA_LEVEL + 40) return BLOCK_STONE;
        return BLOCK_GRASS;
    }

    return BLOCK_GRASS;
}

static BlockType surfaceFillerForBiome(Biome b, int surfaceY)
{
    if(b == BIOME_DESERT) return BLOCK_SAND;
    if(b == BIOME_BEACH)  return BLOCK_SAND;

    if(b == BIOME_EXTREME_HILLS) {
        if(surfaceY >= SEA_LEVEL + 40) return BLOCK_STONE;
        return BLOCK_DIRT;
    }

    return BLOCK_DIRT;
}

// -------------------- TREES --------------------
static bool shouldPlaceTree(Biome b, int x, int z, int surfaceY)
{
    if(surfaceY <= SEA_LEVEL + 1) return false;

    // Required: PLAINS + EXTREME_HILLS must spawn trees
    if(!(b == BIOME_FOREST || b == BIOME_PLAINS || b == BIOME_TAIGA || b == BIOME_EXTREME_HILLS))
        return false;

    // slope check
    auto hAt = [&](int ax, int az)->int {
        if(b == BIOME_EXTREME_HILLS) return getExtremeHillsBase(ax, az);
        return getHeight2D(ax, az, b);
    };
    int hE = hAt(x+2, z);
    int hW = hAt(x-2, z);
    int hN = hAt(x, z+2);
    int hS = hAt(x, z-2);
    int slope = std::max(std::max(std::abs(hE - hW), std::abs(hN - hS)),
                         std::max(std::abs(hE - surfaceY), std::abs(hN - surfaceY)));

    int maxSlope = (b == BIOME_EXTREME_HILLS) ? 6 : 4;
    if(slope >= maxSlope) return false;

    float n = fbmNoise(x * 0.10f, z * 0.10f, 2, 2.0f, 0.5f);
    float n01 = (n + 1.0f) * 0.5f;

    float thresh = 0.90f;
    if(b == BIOME_FOREST) thresh = 0.78f;
    else if(b == BIOME_TAIGA) thresh = 0.84f;
    else if(b == BIOME_EXTREME_HILLS) thresh = 0.84f;
    else if(b == BIOME_PLAINS) thresh = 0.88f;

    float macro = fbmNoise(x * 0.02f + 500.0f, z * 0.02f - 500.0f, 2, 2.0f, 0.5f);
    float macro01 = (macro + 1.0f) * 0.5f;
    float bonus = 0.0f;
    if(b == BIOME_PLAINS || b == BIOME_EXTREME_HILLS) bonus = (macro01 - 0.5f) * 0.06f;
    thresh -= bonus;

    return n01 > thresh;
}

static void addProceduralTree(std::vector<float> &out, int x, int y, int z)
{
    int trunkH = 4 + (std::abs((int)std::floor(perlinNoise(x * 0.3f, z * 0.3f) * 10.0f)) % 2);

    for(int i = 0; i < trunkH; i++)
        addCube(out, (float)x, (float)(y + i), (float)z, BLOCK_TREE_LOG, true);

    int topY = y + trunkH;
    for(int dx = -2; dx <= 2; dx++){
        for(int dz = -2; dz <= 2; dz++){
            for(int dy = -2; dy <= 2; dy++){
                int ax = x + dx;
                int ay = topY + dy;
                int az = z + dz;
                int dist = std::abs(dx) + std::abs(dy) + std::abs(dz);
                if(dist > 5) continue;
                addCube(out, (float)ax, (float)ay, (float)az, BLOCK_LEAVES, true);
            }
        }
    }
}

// -------------------- COLLISION / SOLIDITY --------------------
static bool blockHasCollision(BlockType t) {
    return (t != BLOCK_WATER);
}

bool isSolidBlock(int bx, int by, int bz) {
    auto key = std::make_tuple(bx, by, bz);

    // overrides
    if(extraBlocks.find(key) != extraBlocks.end()){
        BlockType t = extraBlocks[key];
        if((int)t < 0) return false;
        return blockHasCollision(t);
    }

    // flowing water set
    if(waterLevels.find(key) != waterLevels.end())
        return false;

    Biome b = getBiome(bx, bz);
    int h = (b == BIOME_EXTREME_HILLS) ? getExtremeHillsBase(bx, bz) : getHeight2D(bx, bz, b);
    return (by >= 0 && by <= h);
}

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

// -------------------- SPAWN --------------------
static int surfaceYAt(int x, int z) {
    Biome b = getBiome(x, z);
    return (b == BIOME_EXTREME_HILLS) ? getExtremeHillsBase(x, z) : getHeight2D(x, z, b);
}

static void sanitizeLoadedSpawn(float &x, float &y, float &z) {
    int tx = (int)std::floor(x);
    int tz = (int)std::floor(z);

    int surface = surfaceYAt(tx, tz) + 2;
    if(y < (float)surface) y = (float)surface;

    Vec3 pos = {x, y, z};
    int lift = 0;
    while(lift < 256 && checkCollisionFeet(pos)) {
        pos.y += 1.0f;
        lift++;
    }
    x = pos.x; y = pos.y; z = pos.z;
}

static void findSafeSpawn(float &outX, float &outY, float &outZ) {
    const int maxRadius = 128;
    const int step      = 4;

    int bestX = 0, bestZ = 0;
    int bestScore = 1000000000;

    for(int r = 0; r <= maxRadius; r += step) {
        for(int dx = -r; dx <= r; dx += step) {
            for(int dz = -r; dz <= r; dz += step) {
                if(std::abs(dx) != r && std::abs(dz) != r) continue;

                int x = dx, z = dz;
                Biome b = getBiome(x, z);
                if(b == BIOME_OCEAN) continue;

                int h = surfaceYAt(x, z);
                int hE = surfaceYAt(x + 4, z);
                int hW = surfaceYAt(x - 4, z);
                int hN = surfaceYAt(x, z + 4);
                int hS = surfaceYAt(x, z - 4);
                int slope = std::max(std::max(std::abs(hE - hW), std::abs(hN - hS)),
                                     std::max(std::abs(hE - h),  std::abs(hN - h)));

                int score = slope * 10 + std::abs(h - SEA_LEVEL);
                if(score >= bestScore) continue;

                Vec3 pos = {(float)x + 0.5f, (float)h + 2.0f, (float)z + 0.5f};

                int lift = 0;
                while(lift < 256 && checkCollisionFeet(pos)) {
                    pos.y += 1.0f;
                    lift++;
                }
                if(lift >= 256) continue;

                bestScore = score;
                bestX = x; bestZ = z;
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

// -------------------- RAYCAST --------------------
static bool raycastBlock(const Vec3 &start, const Vec3 &dir, float maxDist, int &outX, int &outY, int &outZ) {
    float step = 0.1f, traveled = 0.0f;
    while(traveled < maxDist) {
        Vec3 pos = add(start, multiply(dir, traveled));
        int bx = (int)std::floor(pos.x);
        int by = (int)std::floor(pos.y);
        int bz = (int)std::floor(pos.z);

        bool solid = false;
        {
            std::lock_guard<std::mutex> lk(gWorldMutex);
            solid = isSolidBlock(bx, by, bz);
        }
        if(solid) { outX = bx; outY = by; outZ = bz; return true; }
        traveled += step;
    }
    return false;
}

// -------------------- WATER FLOW --------------------
bool canWaterFlowInto(int x, int y, int z) {
    std::tuple<int,int,int> key = {x, y, z};
    if(extraBlocks.find(key) != extraBlocks.end())
        return false;

    int terrainHeight = surfaceYAt(x, z);
    if(y <= terrainHeight)
        return false;

    return true;
}

static void getChunkCoords(int bx, int bz, int &cx, int &cz) {
    cx = bx / 16; if(bx < 0 && bx % 16 != 0) cx--;
    cz = bz / 16; if(bz < 0 && bz % 16 != 0) cz--;
}

static const int NEAR_CHUNK_RADIUS = 2;

static void rebuildChunkAsync(int cx, int cz);
static void updateWaterFlow(const Camera &camera, float /*dt*/) {
    int playerChunkX = (int)std::floor(camera.position.x / (float)chunkSize);
    int playerChunkZ = (int)std::floor(camera.position.z / (float)chunkSize);

    std::vector<std::tuple<int,int,int>> waterKeys;
    {
        std::lock_guard<std::mutex> lk(gWorldMutex);
        waterKeys.reserve(waterLevels.size());
        for(auto &entry : waterLevels)
            waterKeys.push_back(entry.first);
    }

    for(auto key : waterKeys) {
        int x, y, z;
        std::tie(x, y, z) = key;

        int cellChunkX = x / 16; if(x < 0 && x % 16 != 0) cellChunkX--;
        int cellChunkZ = z / 16; if(z < 0 && z % 16 != 0) cellChunkZ--;
        if (std::abs(cellChunkX - playerChunkX) > NEAR_CHUNK_RADIUS ||
            std::abs(cellChunkZ - playerChunkZ) > NEAR_CHUNK_RADIUS)
            continue;

        int level = 0;
        {
            std::lock_guard<std::mutex> lk(gWorldMutex);
            auto it = waterLevels.find(key);
            if(it == waterLevels.end()) continue;
            level = it->second;
        }

        // Down
        if(y > 0) {
            bool can = false;
            {
                std::lock_guard<std::mutex> lk(gWorldMutex);
                can = canWaterFlowInto(x, y - 1, z);
            }
            if(can) {
                std::tuple<int,int,int> below = {x, y - 1, z};
                bool changed = false;
                {
                    std::lock_guard<std::mutex> lk(gWorldMutex);
                    int belowLevel = 0;
                    auto itB = waterLevels.find(below);
                    if(itB != waterLevels.end()) belowLevel = itB->second;
                    if(8 > belowLevel) {
                        waterLevels[below] = 8;
                        changed = true;
                    }
                }
                if(changed) {
                    int cx, cz; getChunkCoords(x, z, cx, cz);
                    rebuildChunkAsync(cx, cz);
                }
            }
        }

        // Side
        if(level > 1) {
            int offsets[4][3] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
            for(int i = 0; i < 4; i++) {
                int nx = x + offsets[i][0];
                int ny = y;
                int nz = z + offsets[i][2];

                bool can = false;
                {
                    std::lock_guard<std::mutex> lk(gWorldMutex);
                    can = canWaterFlowInto(nx, ny, nz);
                }
                if(!can) continue;

                std::tuple<int,int,int> neighbor = {nx, ny, nz};
                bool changed = false;
                {
                    std::lock_guard<std::mutex> lk(gWorldMutex);
                    int neighborLevel = 0;
                    auto itN = waterLevels.find(neighbor);
                    if(itN != waterLevels.end()) neighborLevel = itN->second;
                    int newLevel = level - 1;
                    if(newLevel > neighborLevel && newLevel > 1) {
                        waterLevels[neighbor] = newLevel;
                        changed = true;
                    }
                }
                if(changed) {
                    int cx, cz; getChunkCoords(nx, nz, cx, cz);
                    rebuildChunkAsync(cx, cz);
                }
            }
        }
    }
}

// -------------------- CPU CHUNK BUILD --------------------
static BlockType overrideAtLocked(int x, int y, int z, bool &hasOverride, bool &carved)
{
    auto key = std::make_tuple(x, y, z);
    auto it = extraBlocks.find(key);
    if(it == extraBlocks.end()) {
        hasOverride = false;
        carved = false;
        return BLOCK_NONE;
    }
    hasOverride = true;
    carved = ((int)it->second < 0);
    return it->second;
}

static bool waterAtLocked(int x, int y, int z)
{
    auto key = std::make_tuple(x, y, z);
    return (waterLevels.find(key) != waterLevels.end());
}

static void buildChunkVerticesCPU(int cx, int cz, std::vector<float> &outVerts)
{
    outVerts.clear();
    outVerts.reserve(16 * 16 * 36 * 5);

    for(int lx = 0; lx < 16; lx++){
        for(int lz = 0; lz < 16; lz++){
            int wx = cx * 16 + lx;
            int wz = cz * 16 + lz;

            Biome b = getBiome(wx, wz);

            int surfaceY = (b == BIOME_EXTREME_HILLS)
                ? getExtremeHillsBase(wx, wz)
                : getHeight2D(wx, wz, b);

            // beaches
            if(b != BIOME_DESERT && b != BIOME_EXTREME_HILLS) {
                if(surfaceY <= SEA_LEVEL + 2 && surfaceY >= SEA_LEVEL - 2)
                    b = BIOME_BEACH;
            }

            int yMin = MIN_WORLD_Y;
            int yMax = std::min(MAX_WORLD_Y, (b == BIOME_EXTREME_HILLS) ? (surfaceY + 20) : (surfaceY + 1));

            for(int y = yMin; y <= yMax; y++){
                bool hasOverride=false, carved=false, hasWater=false;

                BlockType ov = BLOCK_NONE;
                {
                    std::lock_guard<std::mutex> lk(gWorldMutex);
                    ov = overrideAtLocked(wx, y, wz, hasOverride, carved);
                    hasWater = waterAtLocked(wx, y, wz);
                }

                if(hasOverride) {
                    if(carved) continue;
                    addCube(outVerts, (float)wx, (float)y, (float)wz, ov, true);
                    continue;
                }
                if(hasWater) continue;

                bool solid = false;

                if(b == BIOME_EXTREME_HILLS) {
                    solid = mountainDensity(wx, y, wz, surfaceY);
                    if(solid && isCaveCarve(wx, y, wz, surfaceY))
                        solid = false;
                } else {
                    if(y <= surfaceY) {
                        solid = true;
                        if(isCaveCarve(wx, y, wz, surfaceY))
                            solid = false;
                    }
                }

                if(!solid) continue;

                BlockType bt = BLOCK_STONE;

                if(y == 0) bt = BLOCK_BEDROCK;
                else if(y < 3) bt = BLOCK_STONE;
                else {
                    int depth = surfaceY - y;
                    if(depth == 0) bt = surfaceTopForBiome(b, surfaceY);
                    else if(depth < 4) bt = surfaceFillerForBiome(b, surfaceY);
                    else bt = BLOCK_STONE;
                }

                addCube(outVerts, (float)wx, (float)y, (float)wz, bt, true);
            }

            // ---- WATER FILL RULES ----
            // Desert: NO inland water; only if near ocean.
            bool allowWaterFill = true;
            if(b == BIOME_DESERT && !nearOcean(wx, wz)) allowWaterFill = false;

            if(allowWaterFill) {
                if(b == BIOME_OCEAN || b == BIOME_SWAMP || surfaceY < SEA_LEVEL) {
                    for(int y = surfaceY + 1; y <= SEA_LEVEL; y++) {
                        bool hasOverride=false, carved=false, hasWater=false;
                        {
                            std::lock_guard<std::mutex> lk(gWorldMutex);
                            (void)overrideAtLocked(wx, y, wz, hasOverride, carved);
                            hasWater = waterAtLocked(wx, y, wz);
                        }
                        if(hasOverride || hasWater) continue;
                        addCube(outVerts, (float)wx, (float)y, (float)wz, BLOCK_WATER, true);
                    }
                }
            }

            // ---- TREES ----
            if(shouldPlaceTree(b, wx, wz, surfaceY)) {
                bool hasOverride=false, carved=false;
                {
                    std::lock_guard<std::mutex> lk(gWorldMutex);
                    (void)overrideAtLocked(wx, surfaceY + 1, wz, hasOverride, carved);
                }
                if(!hasOverride) {
                    addProceduralTree(outVerts, wx, surfaceY + 1, wz);
                }
            }
        }
    }

    // Explicit water cells (flow system)
    std::vector<std::tuple<int,int,int>> wl;
    {
        std::lock_guard<std::mutex> lk(gWorldMutex);
        wl.reserve(waterLevels.size());
        for(auto &kv : waterLevels) wl.push_back(kv.first);
    }
    for(auto &k : wl) {
        int bx = std::get<0>(k);
        int by = std::get<1>(k);
        int bz = std::get<2>(k);
        int ccx, ccz;
        getChunkCoords(bx, bz, ccx, ccz);
        if(ccx == cx && ccz == cz) {
            addCube(outVerts, (float)bx, (float)by, (float)bz, BLOCK_WATER, true);
        }
    }
}

// -------------------- WORKER THREAD --------------------
static void chunkWorkerThread() {
    while(gWorkerRunning.load()) {
        ChunkJob job;
        {
            std::unique_lock<std::mutex> lk(gJobMutex);
            gJobCV.wait(lk, []{ return !gWorkerRunning.load() || !gJobQueue.empty(); });
            if(!gWorkerRunning.load()) break;
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

// -------------------- MAIN THREAD UPLOAD --------------------
static void uploadChunkToGPU(int cx, int cz, const std::vector<float> &verts, bool rebuild) {
    std::pair<int,int> key = {cx, cz};

    if(rebuild) {
        auto it = chunks.find(key);
        if(it != chunks.end()) {
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

static void requestChunkAsync(int cx, int cz) {
    std::pair<int,int> key = {cx, cz};
    if(chunks.find(key) != chunks.end()) return;

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
    std::pair<int,int> key = {cx, cz};
    if(chunks.find(key) == chunks.end()) return;
    {
        std::lock_guard<std::mutex> lk(gJobMutex);
        gJobQueue.push({cx, cz, true});
    }
    gJobCV.notify_one();
}

// -------------------- RENDER --------------------
static void renderChunks(const Mat4 &view, const Mat4 &proj, const Vec3 &viewPos) {
    glUseProgram(worldShader);

    Mat4 VP = multiplyMatrix(proj, view);
    glUniformMatrix4fv(glGetUniformLocation(worldShader, "MVP"), 1, GL_FALSE, VP.m);
    glUniform1i(glGetUniformLocation(worldShader, "ourTexture"), 0);
    glUniform3f(glGetUniformLocation(worldShader, "sunDirection"), -0.3f, 1.0f, -0.2f);
    glUniform3f(glGetUniformLocation(worldShader, "viewPos"), viewPos.x, viewPos.y, viewPos.z);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);

    for(auto &entry : chunks) {
        Chunk &chunk = entry.second;
        if(chunk.vertices.empty()) continue;
        glBindVertexArray(chunk.VAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(chunk.vertices.size() / 5));
    }
    glBindVertexArray(0);
}

// -------------------- DEV OVERLAY --------------------
static std::string fmtFloat1(float v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << v;
    return ss.str();
}

static void drawDevOverlay(bool show, const Vec3 &playerFeet) {
    if(!show) return;

    // Convert top-left desired position to our bottom-left UI coords:
    // top margin of 10px, line height ~ 12*scale
    float scale = 2.0f;
    float left = 10.0f;
    float top = (float)SCREEN_HEIGHT - 10.0f;

    // draw semi-transparent backing
    float panelW = 380.0f;
    float panelH = 4.0f * (10.0f * scale) + 12.0f;
    uiDrawRect(left - 6.0f, top - panelH + 6.0f, panelW, panelH, 0.0f, 0.0f, 0.0f, 0.35f);

    int bx = (int)std::floor(playerFeet.x);
    int bz = (int)std::floor(playerFeet.z);
    Biome b = getBiome(bx, bz);

    std::string line1 = "C-CRAFT";
    std::string line2 = "XYZ: " + fmtFloat1(playerFeet.x) + " " + fmtFloat1(playerFeet.y) + " " + fmtFloat1(playerFeet.z);
    std::string line3 = std::string("BIOME: ") + biomeToString(b);
    std::string line4 = "VERSION PLACEHOLDER";

    // Our uiDrawText expects bottom-left y
    float lineY = top - (10.0f * scale);

    uiDrawText(left, lineY, line1, scale, 1,1,1,1, true);
    lineY -= (10.0f * scale);
    uiDrawText(left, lineY, line2, scale, 1,1,1,1, true);
    lineY -= (10.0f * scale);
    uiDrawText(left, lineY, line3, scale, 1,1,1,1, true);
    lineY -= (10.0f * scale);
    uiDrawText(left, lineY, line4, scale, 1,1,1,1, true);
}

// -------------------- MAIN --------------------
int main(int, char**) {
    float loadedX = 0.0f, loadedY = 30.0f, loadedZ = 0.0f;
    int loadedSeed = 0;

    bool loadedOk = false;
    {
        std::lock_guard<std::mutex> lk(gWorldMutex);
        loadedOk = loadWorld("saved_world.txt", loadedSeed, loadedX, loadedY, loadedZ);
    }

    if(loadedOk) {
        std::cout << "[World] Loaded seed=" << loadedSeed
                  << " feet(" << loadedX << "," << loadedY << "," << loadedZ << ")\n";
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
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
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
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
        SDL_Quit();
        return -1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if(!glContext) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if(glewErr != GLEW_OK) {
        std::cerr << "GLEW Error: " << glewGetErrorString(glewErr) << "\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

    std::thread worker(chunkWorkerThread);

    Vec3 playerFeet = {loadedX, loadedY, loadedZ};

    Camera camera;
    camera.yaw = -3.14f/2;
    camera.pitch = 0.0f;
    camera.position = {playerFeet.x, playerFeet.y + EYE_HEIGHT, playerFeet.z};

    bool paused = false;
    bool isFlying = false;
    bool showDevOverlay = false; // <-- F3 toggles this
    float verticalVelocity = 0.0f;
    float tickAccumulator = 0.0f;

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
        0.1f, 140.0f);

    while(running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) * 0.001f;
        lastTime = now;

        camera.position = {playerFeet.x, playerFeet.y + EYE_HEIGHT, playerFeet.z};

        // Upload a couple chunks per frame to reduce stutter
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

            if(!res.rebuild) {
                std::lock_guard<std::mutex> lk(gRequestedMutex);
                gRequested.erase(packChunkKey(res.cx, res.cz));
            }
            uploadsThisFrame++;
        }

        // Water tick
        tickAccumulator += dt;
        while(tickAccumulator >= TICK_INTERVAL) {
            tickAccumulator -= TICK_INTERVAL;
            updateWaterFlow(camera, dt);
        }

        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_QUIT) running = false;

            if(ev.type == SDL_KEYDOWN) {
                if(ev.key.keysym.sym == SDLK_F3) {
                    showDevOverlay = !showDevOverlay; // <-- toggle overlay
                }
                else if(ev.key.keysym.sym == SDLK_ESCAPE) {
                    if(inventory.isOpen()) inventory.toggle();
                    paused = !paused;
                    SDL_SetRelativeMouseMode(paused ? SDL_FALSE : SDL_TRUE);
                }
                else if(ev.key.keysym.sym == SDLK_e) {
                    inventory.toggle();
                    SDL_SetRelativeMouseMode(inventory.isOpen() ? SDL_FALSE : SDL_TRUE);
                }
                else if(!paused && !inventory.isOpen() && !isFlying && ev.key.keysym.sym == SDLK_SPACE) {
                    Vec3 testFeet = playerFeet;
                    testFeet.y -= 0.05f;
                    if(checkCollisionFeet(testFeet)) verticalVelocity = JUMP_SPEED;
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
                    camera.pitch = clampf(camera.pitch, -1.5f, 1.5f);
                }
            }

            // block breaking/placing
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
                    std::lock_guard<std::mutex> lk(gWorldMutex);
                    extraBlocks[{bx, by, bz}] = (BlockType)-1;
                    int ccx, ccz; getChunkCoords(bx, bz, ccx, ccz);
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
                    if(blockToPlace != (int)BLOCK_NONE) {
                        Vec3 pos = playerFeet;
                        float half = playerWidth * 0.5f;
                        float minX = pos.x - half, maxX = pos.x + half;
                        float minY = pos.y,        maxY = pos.y + playerHeight;
                        float minZ = pos.z - half, maxZ = pos.z + half;

                        if(!(px + 1 > minX && px < maxX &&
                             py + 1 > minY && py < maxY &&
                             pz + 1 > minZ && pz < maxZ)) {

                            std::lock_guard<std::mutex> lk(gWorldMutex);
                            extraBlocks[{px, py, pz}] = (BlockType)blockToPlace;
                            int ccx, ccz; getChunkCoords(px, pz, ccx, ccz);
                            rebuildChunkAsync(ccx, ccz);
                        }
                    }
                }
            }
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

        newFeet.x += move.x * speed * dt;
        if(checkCollisionFeet(newFeet)) newFeet.x = playerFeet.x;

        newFeet.z += move.z * speed * dt;
        if(checkCollisionFeet(newFeet)) newFeet.z = playerFeet.z;

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

                if(checkCollisionFeet(testPos)) { verticalVelocity = 0.0f; break; }
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
        camera.position = {playerFeet.x, playerFeet.y + EYE_HEIGHT, playerFeet.z};

        // Request chunks around player
        int pcx = (int)std::floor(playerFeet.x / (float)chunkSize);
        int pcz = (int)std::floor(playerFeet.z / (float)chunkSize);
        for(int cx = pcx - renderDistance; cx <= pcx + renderDistance; cx++){
            for(int cz = pcz - renderDistance; cz <= pcz + renderDistance; cz++){
                requestChunkAsync(cx, cz);
            }
        }

        inventory.update(dt, camera);

        // Render
        glViewport(0,0,SCREEN_WIDTH,SCREEN_HEIGHT);
        glClearColor(0.55f,0.75f,1.0f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 view = lookAtMatrix(camera.position, add(camera.position, forward), {0,1,0});
        renderChunks(view, projWorld, camera.position);

        // Dev overlay (F3)
        glDisable(GL_DEPTH_TEST);
        drawDevOverlay(showDevOverlay, playerFeet);
        glEnable(GL_DEPTH_TEST);

        SDL_GL_SwapWindow(window);
    }

    // Save
    {
        std::lock_guard<std::mutex> lk(gWorldMutex);
        saveWorld("saved_world.txt", loadedSeed, playerFeet.x, playerFeet.y, playerFeet.z);
    }

    // Shutdown worker
    gWorkerRunning.store(false);
    gJobCV.notify_all();
    if(worker.joinable()) worker.join();

    // Cleanup
    glDeleteProgram(worldShader);
    glDeleteProgram(uiShader);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);

    for(auto &kv : chunks) {
        glDeleteVertexArrays(1, &kv.second.VAO);
        glDeleteBuffers(1, &kv.second.VBO);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

