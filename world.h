#ifndef WORLD_H
#define WORLD_H

#include <string>
#include <map>
#include <tuple>
#include "cube.h"

// We continue to store extraBlocks in world.cpp
extern std::map<std::tuple<int,int,int>, BlockType> extraBlocks;

// Save the current "extraBlocks", seed, and player location to a file.
bool saveWorld(const std::string& filename, int seed, float playerX, float playerY, float playerZ);

// Load "extraBlocks", seed, and player location from a file.
// If found, sets outSeed and outPlayerX/Y/Z. Otherwise returns false.
bool loadWorld(const std::string& filename, int & outSeed, float & outPlayerX, float & outPlayerY, float & outPlayerZ);

#endif // WORLD_H
