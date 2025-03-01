#ifndef CUBE_H
#define CUBE_H

#include <vector>

// Block types used for terrain and extra features.
enum BlockType {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_BEDROCK, // New block type for bedrock
    BLOCK_TREE_LOG,
    BLOCK_LEAVES,
    BLOCK_WATER
};

// Adds a cube at position (x,y,z) with textures chosen based on the block type.
// Each vertex is defined with 3 position floats and 2 UV floats.
void addCube(std::vector<float>& vertices, float x, float y, float z, BlockType blockType);

#endif // CUBE_H

