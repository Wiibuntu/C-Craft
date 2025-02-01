#ifndef CUBE_H
#define CUBE_H

#include <vector>

// The cube vertex data now includes texture coordinates (x,y,z,u,v)
// There are 6 faces, 2 triangles per face, 3 vertices per triangle = 36 vertices.
extern float cubeVertices[];

// Add a cube with the given position offset (x, y, z) to the vertices vector.
void addCube(std::vector<float>& vertices, float x, float y, float z);

#endif // CUBE_H
