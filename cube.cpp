#include "cube.h"
#include <cstdlib>  // for rand()
#include <cmath>

// Define the size of one tile in UV coordinates.
const float tileSize = 1.0f / 16.0f;

// Texture atlas tile positions (tile coordinates, where (0,0) is bottom-left):
// Grass block:
const float grassTopTileX    = 0.0f, grassTopTileY    = 15.0f;
const float grassSideTileX   = 1.0f, grassSideTileY   = 15.0f;
const float grassBottomTileX = 2.0f, grassBottomTileY = 15.0f;
// Dirt block (two variants):
const float dirtTile1X = 0.0f, dirtTile1Y = 14.0f;
const float dirtTile2X = 1.0f, dirtTile2Y = 14.0f;
// Stone block:
const float stoneTileX = 2.0f, stoneTileY = 14.0f;

// Helper function to compute UV coordinates for a given tile.
void getTileUV(float tileX, float tileY, float uv[4][2]) {
    // uv[0]: lower-left, uv[1]: lower-right, uv[2]: upper-right, uv[3]: upper-left.
    uv[0][0] = tileX * tileSize;         uv[0][1] = tileY * tileSize;
    uv[1][0] = (tileX + 1) * tileSize;     uv[1][1] = tileY * tileSize;
    uv[2][0] = (tileX + 1) * tileSize;     uv[2][1] = (tileY + 1) * tileSize;
    uv[3][0] = tileX * tileSize;           uv[3][1] = (tileY + 1) * tileSize;
}

// The addCube function generates a cube (with 36 vertices) at position (x,y,z)
// with texture coordinates chosen based on blockType.
void addCube(std::vector<float>& vertices, float x, float y, float z, BlockType blockType) {
    // Determine the tile to use for each face.
    // For side faces, we use a different tile for grass than for dirt/stone.
    float topTileX, topTileY;
    float sideTileX, sideTileY;
    float bottomTileX, bottomTileY;
    
    if (blockType == BLOCK_GRASS) {
        topTileX = grassTopTileX;       topTileY = grassTopTileY;
        sideTileX = grassSideTileX;     sideTileY = grassSideTileY;
        bottomTileX = grassBottomTileX; bottomTileY = grassBottomTileY;
    } else if (blockType == BLOCK_DIRT) {
        // Randomly choose between two dirt variants.
        int variant = rand() % 2;
        if (variant == 0) {
            topTileX = sideTileX = bottomTileX = dirtTile1X;
            topTileY = sideTileY = bottomTileY = dirtTile1Y;
        } else {
            topTileX = sideTileX = bottomTileX = dirtTile2X;
            topTileY = sideTileY = bottomTileY = dirtTile2Y;
        }
    } else if (blockType == BLOCK_STONE) {
        topTileX = sideTileX = bottomTileX = stoneTileX;
        topTileY = sideTileY = bottomTileY = stoneTileY;
    }
    
    float uvTop[4][2], uvSide[4][2], uvBottom[4][2];
    getTileUV(topTileX, topTileY, uvTop);
    getTileUV(sideTileX, sideTileY, uvSide);
    getTileUV(bottomTileX, bottomTileY, uvBottom);
    
    // Define the 8 corners of the cube.
    // Cube extends from (x, y, z) to (x+1, y+1, z+1)
    float x0 = x,     x1 = x + 1;
    float y0 = y,     y1 = y + 1;
    float z0 = z,     z1 = z + 1;
    
    // We'll add vertices face by face. Each face is 2 triangles (6 vertices),
    // with each vertex having 3 position floats and 2 UV floats.
    // Order: front, back, left, right, top, bottom.
    
    // Front face (z = z1) using side texture.
    // Vertices: (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y0, z1), (x1, y1, z1), (x0, y1, z1)
    {
        // Triangle 1
        float pos[3];
        // Vertex 1
        pos[0] = x0; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        // Vertex 2
        pos[0] = x1; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[1][0], uvSide[1][1] });
        // Vertex 3
        pos[0] = x1; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        // Triangle 2
        pos[0] = x0; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[3][0], uvSide[3][1] });
    }
    
    // Back face (z = z0) using side texture.
    {
        float pos[3];
        // Triangle 1
        pos[0] = x1; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x0; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[1][0], uvSide[1][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        // Triangle 2
        pos[0] = x1; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[3][0], uvSide[3][1] });
    }
    
    // Left face (x = x0) using side texture.
    {
        float pos[3];
        // Triangle 1
        pos[0] = x0; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x0; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[1][0], uvSide[1][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        // Triangle 2
        pos[0] = x0; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[3][0], uvSide[3][1] });
    }
    
    // Right face (x = x1) using side texture.
    {
        float pos[3];
        // Triangle 1
        pos[0] = x1; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x1; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[1][0], uvSide[1][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        // Triangle 2
        pos[0] = x1; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[0][0], uvSide[0][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[2][0], uvSide[2][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uvSide[3][0], uvSide[3][1] });
    }
    
    // Top face (y = y1). Use top texture for grass, else use side texture.
    {
        float pos[3];
        const float (*uv)[2] = (blockType == BLOCK_GRASS) ? uvTop : uvSide;
        // Triangle 1
        pos[0] = x0; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[0][0], uv[0][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[1][0], uv[1][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[2][0], uv[2][1] });
        // Triangle 2
        pos[0] = x0; pos[1] = y1; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[0][0], uv[0][1] });
        pos[0] = x1; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[2][0], uv[2][1] });
        pos[0] = x0; pos[1] = y1; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[3][0], uv[3][1] });
    }
    
    // Bottom face (y = y0). Use bottom texture for grass; for others, use side.
    {
        float pos[3];
        const float (*uv)[2] = (blockType == BLOCK_GRASS) ? uvBottom : uvSide;
        // Triangle 1
        pos[0] = x0; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[0][0], uv[0][1] });
        pos[0] = x1; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[1][0], uv[1][1] });
        pos[0] = x1; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[2][0], uv[2][1] });
        // Triangle 2
        pos[0] = x0; pos[1] = y0; pos[2] = z0;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[0][0], uv[0][1] });
        pos[0] = x1; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[2][0], uv[2][1] });
        pos[0] = x0; pos[1] = y0; pos[2] = z1;
        vertices.insert(vertices.end(), { pos[0], pos[1], pos[2], uv[3][0], uv[3][1] });
    }
}
