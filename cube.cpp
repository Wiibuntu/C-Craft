#include "cube.h"

// Cube geometry: one cube (block) is made of 36 vertices.
// The cube is defined in the positive octant from (0,0,0) to (1,1,1).
float cubeVertices[] = {
    // Front face
    0, 0, 1,   1, 0, 1,   1, 1, 1,
    0, 0, 1,   1, 1, 1,   0, 1, 1,
    // Back face
    1, 0, 0,   0, 0, 0,   0, 1, 0,
    1, 0, 0,   0, 1, 0,   1, 1, 0,
    // Left face
    0, 0, 0,   0, 0, 1,   0, 1, 1,
    0, 0, 0,   0, 1, 1,   0, 1, 0,
    // Right face
    1, 0, 1,   1, 0, 0,   1, 1, 0,
    1, 0, 1,   1, 1, 0,   1, 1, 1,
    // Top face
    0, 1, 1,   1, 1, 1,   1, 1, 0,
    0, 1, 1,   1, 1, 0,   0, 1, 0,
    // Bottom face
    0, 0, 0,   1, 0, 0,   1, 0, 1,
    0, 0, 0,   1, 0, 1,   0, 0, 1,
};

void addCube(std::vector<float>& vertices, float x, float y, float z) {
    for (int i = 0; i < 36 * 3; i += 3) {
        vertices.push_back(cubeVertices[i]   + x);
        vertices.push_back(cubeVertices[i+1] + y);
        vertices.push_back(cubeVertices[i+2] + z);
    }
}
