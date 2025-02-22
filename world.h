#ifndef WORLD_H
#define WORLD_H

#include <string>
#include <map>
#include <tuple>
#include "cube.h"

// extraBlocks is used for terrain overrides (trees, modifications, etc.)
extern std::map<std::tuple<int, int, int>, BlockType> extraBlocks;

// waterLevels stores water at a given (x,y,z) with a water level (1–8),
// where 8 indicates a source cell.
extern std::map<std::tuple<int, int, int>, int> waterLevels;

// Example functions for saving/loading a world.
// (Note: waterLevels are transient and not persisted.)
bool loadWorld(const char* filename, int &outSeed,
               float &outPlayerX, float &outPlayerY, float &outPlayerZ);
bool saveWorld(const char* filename, int seed,
               float playerX, float playerY, float playerZ);

#endif

