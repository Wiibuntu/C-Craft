#ifndef NOISE_H
#define NOISE_H

// Returns a basic Perlin noise value for the given (x, y) coordinates.
// (Range roughly [-1,1])
float perlinNoise(float x, float y);

// Returns fractal Brownian motion (fBm) noise using Perlin noise.
// octaves: number of layers, lacunarity: frequency multiplier, gain: amplitude multiplier.
float fbmNoise(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f);

#endif // NOISE_H
