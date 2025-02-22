#include "noise.h"
#include <cmath>
#include <cstdlib>  // for srand, rand, etc.

// ---------------------------------------------------------------------------
// By default, we had a static permutation array for Perlin. We'll now
// allow re-seeding it using setNoiseSeed(seed). If the user never calls
// setNoiseSeed, we fall back to the original pre-defined permutation.
//
// If you want to always randomize at startup (instead of a fixed default),
// call setNoiseSeed(...) with your random seed once at initialization.
//
// ---------------------------------------------------------------------------

// Original classic permutation array from the reference Perlin code.
static int permutationDefault[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
    184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

// This array will be our actual working copy of 256 random values. 
static int permutation[256];

// Extended array for indexing: p[i] = permutation[i mod 256].
static int p[512];
static bool initialized = false;

// Forward declaration for the internal method that re-initializes p[].
static void initPermutationArray();

// This function re-initializes p[] with the current content of the
// 256-element permutation[] array.
static void initPermutationArray()
{
    for (int i = 0; i < 256; i++) {
        p[i] = permutation[i];
        p[256 + i] = permutation[i];
    }
    initialized = true;
}

// setNoiseSeed: randomizes the 256-element permutation[] array using a
// given seed, then re-initializes p[] from it.
void setNoiseSeed(unsigned int seed)
{
    // Initialize the pseudo-random generator
    std::srand(seed);

    // Fill permutation[] with the default ordering 0..255
    for (int i = 0; i < 256; i++) {
        permutation[i] = i;
    }
    // Fisher-Yates shuffle
    for (int i = 255; i > 0; i--) {
        int swapIndex = std::rand() % (i + 1);
        // swap
        int tmp = permutation[i];
        permutation[i] = permutation[swapIndex];
        permutation[swapIndex] = tmp;
    }

    // Re-init p[] with the newly shuffled permutation
    initPermutationArray();
}

// If the user never calls setNoiseSeed, we use the original classic array.
static void useDefaultPermutationIfNecessary()
{
    if (!initialized) {
        // Copy the default array into permutation
        for (int i = 0; i < 256; i++) {
            permutation[i] = permutationDefault[i];
        }
        // Then fill p[] from it
        initPermutationArray();
    }
}

// Fade, Lerp, Grad (as per standard Perlin).
static float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

static float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = (h < 8) ? x : y;
    float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// 2D Perlin noise
float perlinNoise(float x, float y)
{
    // Ensure we have an initialized p[] array
    useDefaultPermutationIfNecessary();

    float z = 0.0f; // we treat 2D as z=0
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;
    
    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);
    
    float u = fade(x);
    float v = fade(y);
    float w = fade(z);
    
    int A  = p[X] + Y;
    int AA = p[A] + Z;
    int AB = p[A + 1] + Z;
    int B  = p[X + 1] + Y;
    int BA = p[B] + Z;
    int BB = p[B + 1] + Z;
    
    float res = lerp(w, 
        lerp(v, 
            lerp(u, grad(p[AA], x, y, z),
                     grad(p[BA], x - 1, y, z)),
            lerp(u, grad(p[AB], x, y - 1, z),
                     grad(p[BB], x - 1, y - 1, z))),
        lerp(v,
            lerp(u, grad(p[AA + 1], x, y, z - 1),
                     grad(p[BA + 1], x - 1, y, z - 1)),
            lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
                     grad(p[BB + 1], x - 1, y - 1, z - 1))));
    
    return res;
}

// fractal Brownian motion
float fbmNoise(float x, float y, int octaves, float lacunarity, float gain)
{
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float sum = 0.0f;
    float maxValue = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum += perlinNoise(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return sum / maxValue;
}
