#include "perlin_noise.h"
#include <algorithm>  // For std::shuffle
#include <random>     // For std::default_random_engine
#include <cmath>

float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

float perlinNoise(float x, float y) {
    int X = (int)floor(x) & 255;
    int Y = (int)floor(y) & 255;

    x -= floor(x);
    y -= floor(y);

    float u = fade(x);
    float v = fade(y);

    static int p[512];
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < 256; ++i) {
            p[i] = i;
        }

        // Use std::shuffle with a random number generator
        std::random_device rd;
        std::default_random_engine rng(rd());
        std::shuffle(p, p + 256, rng);

        for (int i = 0; i < 256; ++i) {
            p[256 + i] = p[i];
        }

        initialized = true;
    }

    int aa = p[p[X] + Y];
    int ab = p[p[X] + Y + 1];
    int ba = p[p[X + 1] + Y];
    int bb = p[p[X + 1] + Y + 1];

    return lerp(
        lerp(grad(aa, x, y, 0), grad(ba, x - 1, y, 0), u),
        lerp(grad(ab, x, y - 1, 0), grad(bb, x - 1, y - 1, 0), u),
        v);
}
