#ifndef CUBE_H
#define CUBE_H

#include <vector>

// Enum for block types.
enum BlockType {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE
};

// addCube now takes a block type parameter.
void addCube(std::vector<float>& vertices, float x, float y, float z, BlockType blockType);

#endif // CUBE_H
