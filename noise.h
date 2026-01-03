#ifndef NOISE_H
#define NOISE_H

// 2D Perlin noise in roughly [-1, 1]
float perlinNoise(float x, float y);

// 3D Perlin noise in roughly [-1, 1]
float perlinNoise3D(float x, float y, float z);

// 2D fBm noise
float fbmNoise(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);

// 3D fBm noise
float fbmNoise3D(float x, float y, float z, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);

// Seed permutation
void setNoiseSeed(unsigned int seed);

#endif // NOISE_H

