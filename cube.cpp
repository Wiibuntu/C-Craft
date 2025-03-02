#include "cube.h"
#include <cstdlib>  // for rand()
#include <vector>

// We assume a texture atlas that is 16x16 tiles.
const float tileSize = 1.0f / 16.0f;

// Define tile coordinates (in grid units) for each block texture.
// (Coordinates are given as (tileX, tileY) with (0,0) at bottom-left.)
  
// Grass block:
const float grassTopTileX    = 0.0f, grassTopTileY    = 15.0f;
const float grassSideTileX   = 3.0f, grassSideTileY   = 15.0f;
const float grassBottomTileX = 2.0f, grassBottomTileY = 15.0f;

// Dirt block (two variants):
const float dirtTile1X = 2.0f, dirtTile1Y = 15.0f;
const float dirtTile2X = 2.0f, dirtTile2Y = 15.0f;

// Stone block:
const float stoneTileX = 1.0f, stoneTileY = 15.0f;

// Sand block:
const float sandTileX = 2.0f, sandTileY = 14.0f;

// Bedrock block:
const float bedrockTileX = 1.0f, bedrockTileY = 14.0f; // Adjust as needed

// Tree log block:
const float treeLogTopTileX = 5.0f, treeLogTopTileY = 14.0f;
const float treeLogSideTileX = 4.0f, treeLogSideTileY = 14.0f;

// Tree leaves block:
const float leavesTileX = 4.0f, leavesTileY = 12.0f;

// Water block:
const float waterTileX = 13.0f, waterTileY = 3.0f; // pick an empty tile

// New blocks UV coordinates (example values)
// Wooden Planks:
const float woodenPlanksTileX = 4.0f, woodenPlanksTileY = 15.0f;
// Cobblestone:
const float cobblestoneTileX = 0.0f, cobblestoneTileY = 14.0f;
// Gravel:
const float gravelTileX = 3.0f, gravelTileY = 14.0f;
// Bricks:
const float bricksTileX = 7.0f, bricksTileY = 15.0f;
// Glass:
const float glassTileX = 1.0f, glassTileY = 12.0f;
// Sponge:
const float spongeTileX = 0.0f, spongeTileY = 12.0f;
// Wool blocks (using row 14 as an example):
const float woolWhiteTileX = 1.0f,  woolWhiteTileY  = 8.0f;
const float woolRedTileX = 1.0f,    woolRedTileY    = 7.0f;
const float woolBlackTileX = 1.0f,  woolBlackTileY  = 8.0f;
const float woolGreyTileX = 2.0f,   woolGreyTileY   = 8.0f;
const float woolPinkTileX = 2.0f,   woolPinkTileY   = 7.0f;
const float woolLimeGreenTileX = 2.0f, woolLimeGreenTileY = 6.0f;
const float woolGreenTileX = 1.0f,  woolGreenTileY  = 6.0f;
const float woolBrownTileX = 1.0f,  woolBrownTileY  = 5.0f;
const float woolYellowTileX = 2.0f, woolYellowTileY = 5.0f;
const float woolLightBlueTileX = 2.0f, woolLightBlueTileY = 4.0f;
const float woolBlueTileX = 1.0f,  woolBlueTileY  = 4.0f;
const float woolPurpleTileX = 1.0f, woolPurpleTileY = 3.0f;
const float woolVioletTileX = 2.0f, woolVioletTileY = 3.0f;
const float woolTurquoiseTileX = 1.0f, woolTurquoiseTileY = 2.0f;
const float woolOrangeTileX = 2.0f, woolOrangeTileY = 2.0f;

// Helper function to compute UV coordinates for a given tile.
// Returns UVs for the four corners: lower-left, lower-right, upper-right, upper-left.
void getTileUV(float tileX, float tileY, float uv[4][2]) {
    uv[0][0] = tileX * tileSize;       uv[0][1] = tileY * tileSize;
    uv[1][0] = (tileX + 1) * tileSize; uv[1][1] = tileY * tileSize;
    uv[2][0] = (tileX + 1) * tileSize; uv[2][1] = (tileY + 1) * tileSize;
    uv[3][0] = tileX * tileSize;       uv[3][1] = (tileY + 1) * tileSize;
}

// addCube: Generates geometry for a cube at (x,y,z) using textures selected by blockType.
void addCube(std::vector<float>& vertices, float x, float y, float z, BlockType blockType) {
    float uvTop[4][2], uvSide[4][2], uvBottom[4][2];
    
    // Select the UV coordinates based on the block type.
    if (blockType == BLOCK_GRASS) {
        getTileUV(grassTopTileX, grassTopTileY, uvTop);
        getTileUV(grassSideTileX, grassSideTileY, uvSide);
        getTileUV(grassBottomTileX, grassBottomTileY, uvBottom);
    } else if (blockType == BLOCK_DIRT) {
        int variant = rand() % 2;
        if (variant == 0) {
            getTileUV(dirtTile1X, dirtTile1Y, uvTop);
            getTileUV(dirtTile1X, dirtTile1Y, uvSide);
            getTileUV(dirtTile1X, dirtTile1Y, uvBottom);
        } else {
            getTileUV(dirtTile2X, dirtTile2Y, uvTop);
            getTileUV(dirtTile2X, dirtTile2Y, uvSide);
            getTileUV(dirtTile2X, dirtTile2Y, uvBottom);
        }
    } else if (blockType == BLOCK_STONE) {
        getTileUV(stoneTileX, stoneTileY, uvTop);
        getTileUV(stoneTileX, stoneTileY, uvSide);
        getTileUV(stoneTileX, stoneTileY, uvBottom);
    } else if (blockType == BLOCK_SAND) {
        getTileUV(sandTileX, sandTileY, uvTop);
        getTileUV(sandTileX, sandTileY, uvSide);
        getTileUV(sandTileX, sandTileY, uvBottom);
    } else if (blockType == BLOCK_BEDROCK) {
        getTileUV(bedrockTileX, bedrockTileY, uvTop);
        getTileUV(bedrockTileX, bedrockTileY, uvSide);
        getTileUV(bedrockTileX, bedrockTileY, uvBottom);
    } else if (blockType == BLOCK_TREE_LOG) {
        getTileUV(treeLogTopTileX, treeLogTopTileY, uvTop);
        getTileUV(treeLogSideTileX, treeLogSideTileY, uvSide);
        getTileUV(treeLogTopTileX, treeLogTopTileY, uvBottom);
    } else if (blockType == BLOCK_LEAVES) {
        getTileUV(leavesTileX, leavesTileY, uvTop);
        getTileUV(leavesTileX, leavesTileY, uvSide);
        getTileUV(leavesTileX, leavesTileY, uvBottom);
    } else if (blockType == BLOCK_WATER) {
        getTileUV(waterTileX, waterTileY, uvTop);
        getTileUV(waterTileX, waterTileY, uvSide);
        getTileUV(waterTileX, waterTileY, uvBottom);
    }
    // New blocks:
    else if (blockType == BLOCK_WOODEN_PLANKS) {
        getTileUV(woodenPlanksTileX, woodenPlanksTileY, uvTop);
        getTileUV(woodenPlanksTileX, woodenPlanksTileY, uvSide);
        getTileUV(woodenPlanksTileX, woodenPlanksTileY, uvBottom);
    } else if (blockType == BLOCK_COBBLESTONE) {
        getTileUV(cobblestoneTileX, cobblestoneTileY, uvTop);
        getTileUV(cobblestoneTileX, cobblestoneTileY, uvSide);
        getTileUV(cobblestoneTileX, cobblestoneTileY, uvBottom);
    } else if (blockType == BLOCK_GRAVEL) {
        getTileUV(gravelTileX, gravelTileY, uvTop);
        getTileUV(gravelTileX, gravelTileY, uvSide);
        getTileUV(gravelTileX, gravelTileY, uvBottom);
    } else if (blockType == BLOCK_BRICKS) {
        getTileUV(bricksTileX, bricksTileY, uvTop);
        getTileUV(bricksTileX, bricksTileY, uvSide);
        getTileUV(bricksTileX, bricksTileY, uvBottom);
    } else if (blockType == BLOCK_GLASS) {
        getTileUV(glassTileX, glassTileY, uvTop);
        getTileUV(glassTileX, glassTileY, uvSide);
        getTileUV(glassTileX, glassTileY, uvBottom);
    } else if (blockType == BLOCK_SPONGE) {
        getTileUV(spongeTileX, spongeTileY, uvTop);
        getTileUV(spongeTileX, spongeTileY, uvSide);
        getTileUV(spongeTileX, spongeTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_WHITE) {
        getTileUV(woolWhiteTileX, woolWhiteTileY, uvTop);
        getTileUV(woolWhiteTileX, woolWhiteTileY, uvSide);
        getTileUV(woolWhiteTileX, woolWhiteTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_RED) {
        getTileUV(woolRedTileX, woolRedTileY, uvTop);
        getTileUV(woolRedTileX, woolRedTileY, uvSide);
        getTileUV(woolRedTileX, woolRedTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_BLACK) {
        getTileUV(woolBlackTileX, woolBlackTileY, uvTop);
        getTileUV(woolBlackTileX, woolBlackTileY, uvSide);
        getTileUV(woolBlackTileX, woolBlackTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_GREY) {
        getTileUV(woolGreyTileX, woolGreyTileY, uvTop);
        getTileUV(woolGreyTileX, woolGreyTileY, uvSide);
        getTileUV(woolGreyTileX, woolGreyTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_PINK) {
        getTileUV(woolPinkTileX, woolPinkTileY, uvTop);
        getTileUV(woolPinkTileX, woolPinkTileY, uvSide);
        getTileUV(woolPinkTileX, woolPinkTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_LIME_GREEN) {
        getTileUV(woolLimeGreenTileX, woolLimeGreenTileY, uvTop);
        getTileUV(woolLimeGreenTileX, woolLimeGreenTileY, uvSide);
        getTileUV(woolLimeGreenTileX, woolLimeGreenTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_GREEN) {
        getTileUV(woolGreenTileX, woolGreenTileY, uvTop);
        getTileUV(woolGreenTileX, woolGreenTileY, uvSide);
        getTileUV(woolGreenTileX, woolGreenTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_BROWN) {
        getTileUV(woolBrownTileX, woolBrownTileY, uvTop);
        getTileUV(woolBrownTileX, woolBrownTileY, uvSide);
        getTileUV(woolBrownTileX, woolBrownTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_YELLOW) {
        getTileUV(woolYellowTileX, woolYellowTileY, uvTop);
        getTileUV(woolYellowTileX, woolYellowTileY, uvSide);
        getTileUV(woolYellowTileX, woolYellowTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_LIGHT_BLUE) {
        getTileUV(woolLightBlueTileX, woolLightBlueTileY, uvTop);
        getTileUV(woolLightBlueTileX, woolLightBlueTileY, uvSide);
        getTileUV(woolLightBlueTileX, woolLightBlueTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_BLUE) {
        getTileUV(woolBlueTileX, woolBlueTileY, uvTop);
        getTileUV(woolBlueTileX, woolBlueTileY, uvSide);
        getTileUV(woolBlueTileX, woolBlueTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_PURPLE) {
        getTileUV(woolPurpleTileX, woolPurpleTileY, uvTop);
        getTileUV(woolPurpleTileX, woolPurpleTileY, uvSide);
        getTileUV(woolPurpleTileX, woolPurpleTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_VIOLET) {
        getTileUV(woolVioletTileX, woolVioletTileY, uvTop);
        getTileUV(woolVioletTileX, woolVioletTileY, uvSide);
        getTileUV(woolVioletTileX, woolVioletTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_TURQUOISE) {
        getTileUV(woolTurquoiseTileX, woolTurquoiseTileY, uvTop);
        getTileUV(woolTurquoiseTileX, woolTurquoiseTileY, uvSide);
        getTileUV(woolTurquoiseTileX, woolTurquoiseTileY, uvBottom);
    } else if (blockType == BLOCK_WOOL_ORANGE) {
        getTileUV(woolOrangeTileX, woolOrangeTileY, uvTop);
        getTileUV(woolOrangeTileX, woolOrangeTileY, uvSide);
        getTileUV(woolOrangeTileX, woolOrangeTileY, uvBottom);
    }
    
    // Define the eight corners of the cube.
    float x0 = x,     x1 = x + 1;
    float y0 = y,     y1 = y + 1;
    float z0 = z,     z1 = z + 1;
    
    // Front face (z = z1)
    {
        vertices.insert(vertices.end(), { x0, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvSide[3][0], uvSide[3][1] });
    }
    
    // Back face (z = z0)
    {
        vertices.insert(vertices.end(), { x1, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y0, z0, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvSide[3][0], uvSide[3][1] });
    }
    
    // Left face (x = x0)
    {
        vertices.insert(vertices.end(), { x0, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y0, z1, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvSide[3][0], uvSide[3][1] });
    }
    
    // Right face (x = x1)
    {
        vertices.insert(vertices.end(), { x1, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z0, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvSide[3][0], uvSide[3][1] });
    }
    
    // Top face (y = y1)
    {
        vertices.insert(vertices.end(), { x0, y1, z1, uvTop[0][0], uvTop[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvTop[1][0], uvTop[1][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvTop[2][0], uvTop[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvTop[0][0], uvTop[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvTop[2][0], uvTop[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvTop[3][0], uvTop[3][1] });
    }
    
    // Bottom face (y = y0)
    {
        vertices.insert(vertices.end(), { x0, y0, z0, uvBottom[0][0], uvBottom[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z0, uvBottom[1][0], uvBottom[1][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvBottom[2][0], uvBottom[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z0, uvBottom[0][0], uvBottom[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvBottom[2][0], uvBottom[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z1, uvBottom[3][0], uvBottom[3][1] });
    }
}

