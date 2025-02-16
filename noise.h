#ifndef NOISE_H
#define NOISE_H

// Returns a basic Perlin noise value in the range roughly [-1, 1].
float perlinNoise(float x, float y);

// Returns fractal Brownian motion (fBm) noise based on Perlin noise.
// Parameters:
//   octaves: number of noise layers
//   lacunarity: frequency multiplier
//   gain: amplitude multiplier
float fbmNoise(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);

// Allows setting a custom seed for the Perlin permutation.
// If not called, it uses a default permutation array.
void setNoiseSeed(unsigned int seed);

#endif // NOISE_H
