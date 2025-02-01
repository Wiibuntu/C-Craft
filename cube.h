#ifndef CUBE_H
#define CUBE_H

#include <vector>

// Declare the cube vertex array (36 vertices with 3 components each)
extern float cubeVertices[];

// Add a cube with the given offset (x, y, z) to the vertices vector.
void addCube(std::vector<float>& vertices, float x, float y, float z);

#endif // CUBE_H
