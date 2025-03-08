#ifndef CUBE_H
#define CUBE_H

#include <vector>

// Block types used for terrain and extra features.
enum BlockType {
    BLOCK_NONE = -1,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_BEDROCK, // New block type for bedrock
    BLOCK_TREE_LOG,
    BLOCK_LEAVES,
    BLOCK_WATER,
    // New blocks:
    BLOCK_WOODEN_PLANKS,
    BLOCK_COBBLESTONE,
    BLOCK_GRAVEL,
    BLOCK_BRICKS,
    BLOCK_GLASS,
    BLOCK_SPONGE,
    BLOCK_WOOL_WHITE,
    BLOCK_WOOL_RED,
    BLOCK_WOOL_BLACK,
    BLOCK_WOOL_GREY,
    BLOCK_WOOL_PINK,
    BLOCK_WOOL_LIME_GREEN,
    BLOCK_WOOL_GREEN,
    BLOCK_WOOL_BROWN,
    BLOCK_WOOL_YELLOW,
    BLOCK_WOOL_LIGHT_BLUE,
    BLOCK_WOOL_BLUE,
    BLOCK_WOOL_PURPLE,
    BLOCK_WOOL_VIOLET,
    BLOCK_WOOL_TURQUOISE,
    BLOCK_WOOL_ORANGE
};

// Adds a cube at position (x,y,z) with textures chosen based on the block type.
// Each vertex is defined with 3 position floats and 2 UV floats.
// If cullFaces is true (the default), then only faces adjacent to air (or non-solid blocks)
// are added.
void addCube(std::vector<float>& vertices, float x, float y, float z, BlockType blockType, bool cullFaces = true);

#endif // CUBE_H

