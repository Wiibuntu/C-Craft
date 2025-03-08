#ifndef WORLD_H
#define WORLD_H

#include <string>
#include <unordered_map>
#include <tuple>
#include "cube.h"
#include "globals.h"

// extraBlocks is used for terrain overrides (trees, modifications, etc.)
extern std::unordered_map<std::tuple<int, int, int>, BlockType, TupleHash> extraBlocks;

// waterLevels stores water at a given (x,y,z) with a water level (1–8),
// where 8 indicates a source cell.
extern std::unordered_map<std::tuple<int, int, int>, int, TupleHash> waterLevels;

bool loadWorld(const char* filename, int &outSeed,
               float &outPlayerX, float &outPlayerY, float &outPlayerZ);
bool saveWorld(const char* filename, int seed,
               float playerX, float playerY, float playerZ);

// Returns true if the block at the given coordinates is solid.
bool isSolidBlock(int bx, int by, int bz);

#endif

