#include "cube.h"

// Cube geometry with texture coordinates (5 floats per vertex: 3 for position, 2 for tex coords)
// Each face uses the full texture (coordinates from 0 to 1).
float cubeVertices[] = {
    // Front face (z = 1)
    // Triangle 1
    0, 0, 1,   0, 0,
    1, 0, 1,   1, 0,
    1, 1, 1,   1, 1,
    // Triangle 2
    0, 0, 1,   0, 0,
    1, 1, 1,   1, 1,
    0, 1, 1,   0, 1,

    // Back face (z = 0)
    // Triangle 1
    1, 0, 0,   0, 0,
    0, 0, 0,   1, 0,
    0, 1, 0,   1, 1,
    // Triangle 2
    1, 0, 0,   0, 0,
    0, 1, 0,   1, 1,
    1, 1, 0,   0, 1,

    // Left face (x = 0)
    // Triangle 1
    0, 0, 0,   0, 0,
    0, 0, 1,   1, 0,
    0, 1, 1,   1, 1,
    // Triangle 2
    0, 0, 0,   0, 0,
    0, 1, 1,   1, 1,
    0, 1, 0,   0, 1,

    // Right face (x = 1)
    // Triangle 1
    1, 0, 1,   0, 0,
    1, 0, 0,   1, 0,
    1, 1, 0,   1, 1,
    // Triangle 2
    1, 0, 1,   0, 0,
    1, 1, 0,   1, 1,
    1, 1, 1,   0, 1,

    // Top face (y = 1)
    // Triangle 1
    0, 1, 1,   0, 0,
    1, 1, 1,   1, 0,
    1, 1, 0,   1, 1,
    // Triangle 2
    0, 1, 1,   0, 0,
    1, 1, 0,   1, 1,
    0, 1, 0,   0, 1,

    // Bottom face (y = 0)
    // Triangle 1
    0, 0, 0,   0, 0,
    1, 0, 0,   1, 0,
    1, 0, 1,   1, 1,
    // Triangle 2
    0, 0, 0,   0, 0,
    1, 0, 1,   1, 1,
    0, 0, 1,   0, 1,
};

void addCube(std::vector<float>& vertices, float x, float y, float z) {
    // The vertex data is grouped in 5 floats: the first three are position, the next two are tex coords.
    for (int i = 0; i < 36 * 5; i += 5) {
        // Add the offset only to the position components.
        vertices.push_back(cubeVertices[i]   + x);
        vertices.push_back(cubeVertices[i+1] + y);
        vertices.push_back(cubeVertices[i+2] + z);
        // Copy the texture coordinates unmodified.
        vertices.push_back(cubeVertices[i+3]);
        vertices.push_back(cubeVertices[i+4]);
    }
}
