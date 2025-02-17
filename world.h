#ifndef WORLD_H
#define WORLD_H

#include <string>
#include <map>
#include <tuple>
#include "cube.h"


// -----------------------------------------------------------------------------
// extraBlocks is often used to store additional blocks that are not part
// of the base terrain (for trees, modifications, etc.). If you remove a block
// by setting it to BLOCK_AIR in this map, your chunk-building code can skip it.
// -----------------------------------------------------------------------------
extern std::map<std::tuple<int, int, int>, BlockType> extraBlocks;

// -----------------------------------------------------------------------------
// Example function you might have for saving/loading a world. Adjust as needed.
// -----------------------------------------------------------------------------
bool loadWorld(const char* filename, int &outSeed,
               float &outPlayerX, float &outPlayerY, float &outPlayerZ);

bool saveWorld(const char* filename, int seed,
               float playerX, float playerY, float playerZ);

// -----------------------------------------------------------------------------
// If you store anything else in "world.h" (e.g., global variables or utility
// functions), place them here.
// -----------------------------------------------------------------------------

#endif
