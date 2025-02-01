#include "cube.h"
#include <cstdlib>  // for rand()
#include <cmath>

// Assume a 16x16 texture atlas.
const float tileSize = 1.0f / 16.0f;

// Define tile coordinates (in grid units) for each block texture.

// Grass block:
const float grassTopTileX    = 0.0f, grassTopTileY    = 15.0f;
const float grassSideTileX   = 1.0f, grassSideTileY   = 15.0f;
const float grassBottomTileX = 2.0f, grassBottomTileY = 15.0f;

// Dirt block (two variants):
const float dirtTile1X = 0.0f, dirtTile1Y = 14.0f;
const float dirtTile2X = 1.0f, dirtTile2Y = 14.0f;

// Stone block:
const float stoneTileX = 2.0f, stoneTileY = 14.0f;

// Sand block:
const float sandTileX = 3.0f, sandTileY = 14.0f;

// Tree log block:
const float treeLogTopTileX = 4.0f, treeLogTopTileY = 14.0f;
const float treeLogSideTileX = 5.0f, treeLogSideTileY = 14.0f;

// Helper function to compute UV coordinates for a given tile.
// Returns UVs for the four corners: lower-left, lower-right, upper-right, upper-left.
void getTileUV(float tileX, float tileY, float uv[4][2]) {
    uv[0][0] = tileX * tileSize;         uv[0][1] = tileY * tileSize;
    uv[1][0] = (tileX + 1) * tileSize;     uv[1][1] = tileY * tileSize;
    uv[2][0] = (tileX + 1) * tileSize;     uv[2][1] = (tileY + 1) * tileSize;
    uv[3][0] = tileX * tileSize;           uv[3][1] = (tileY + 1) * tileSize;
}

// addCube: Generates geometry for a cube at (x,y,z) using textures selected by blockType.
void addCube(std::vector<float>& vertices, float x, float y, float z, BlockType blockType) {
    float uvTop[4][2], uvSide[4][2], uvBottom[4][2];
    
    // Choose the correct texture tiles based on block type.
    if (blockType == BLOCK_GRASS) {
        getTileUV(grassTopTileX, grassTopTileY, uvTop);
        getTileUV(grassSideTileX, grassSideTileY, uvSide);
        getTileUV(grassBottomTileX, grassBottomTileY, uvBottom);
    } else if (blockType == BLOCK_DIRT) {
        // Randomly choose one of the two dirt variants.
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
    } else if (blockType == BLOCK_TREE_LOG) {
        // Tree logs: use one tile for top/bottom and one for the sides.
        getTileUV(treeLogTopTileX, treeLogTopTileY, uvTop);
        getTileUV(treeLogSideTileX, treeLogSideTileY, uvSide);
        getTileUV(treeLogTopTileX, treeLogTopTileY, uvBottom);
    }
    
    // Define the eight corners of the cube.
    float x0 = x,     x1 = x + 1;
    float y0 = y,     y1 = y + 1;
    float z0 = z,     z1 = z + 1;
    
    // Add vertices for each face (two triangles per face).
    // Each vertex: position (x,y,z) followed by UV (u,v).
    
    // Front face (z = z1) uses the side texture.
    {
        vertices.insert(vertices.end(), { x0, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvSide[3][0], uvSide[3][1] });
    }
    
    // Back face (z = z0) uses the side texture.
    {
        vertices.insert(vertices.end(), { x1, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y0, z0, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvSide[3][0], uvSide[3][1] });
    }
    
    // Left face (x = x0) uses the side texture.
    {
        vertices.insert(vertices.end(), { x0, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y0, z1, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z0, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvSide[3][0], uvSide[3][1] });
    }
    
    // Right face (x = x1) uses the side texture.
    {
        vertices.insert(vertices.end(), { x1, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z0, uvSide[1][0], uvSide[1][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvSide[0][0], uvSide[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvSide[2][0], uvSide[2][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvSide[3][0], uvSide[3][1] });
    }
    
    // Top face (y = y1) uses the top texture.
    {
        vertices.insert(vertices.end(), { x0, y1, z1, uvTop[0][0], uvTop[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z1, uvTop[1][0], uvTop[1][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvTop[2][0], uvTop[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z1, uvTop[0][0], uvTop[0][1] });
        vertices.insert(vertices.end(), { x1, y1, z0, uvTop[2][0], uvTop[2][1] });
        vertices.insert(vertices.end(), { x0, y1, z0, uvTop[3][0], uvTop[3][1] });
    }
    
    // Bottom face (y = y0) uses the bottom texture.
    {
        vertices.insert(vertices.end(), { x0, y0, z0, uvBottom[0][0], uvBottom[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z0, uvBottom[1][0], uvBottom[1][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvBottom[2][0], uvBottom[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z0, uvBottom[0][0], uvBottom[0][1] });
        vertices.insert(vertices.end(), { x1, y0, z1, uvBottom[2][0], uvBottom[2][1] });
        vertices.insert(vertices.end(), { x0, y0, z1, uvBottom[3][0], uvBottom[3][1] });
    }
}
