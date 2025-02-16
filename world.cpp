#include "world.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <cstdlib>

std::map<std::tuple<int,int,int>, BlockType> extraBlocks;

bool saveWorld(const std::string& filename, int seed,
               float playerX, float playerY, float playerZ)
{
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Could not open file for saving world: " << filename << std::endl;
        return false;
    }

    // Save the seed
    outFile << "SEED " << seed << "\n";

    // Save the player’s location
    outFile << "PLAYER " << playerX << " " << playerY << " " << playerZ << "\n";

    // Then each extra block: x y z blockType
    for (auto & kv : extraBlocks) {
        auto coord = kv.first;
        BlockType type = kv.second;
        int x = std::get<0>(coord);
        int y = std::get<1>(coord);
        int z = std::get<2>(coord);
        outFile << x << " " << y << " " << z << " " << type << "\n";
    }

    outFile.close();
    std::cout << "World saved to " << filename << std::endl;
    return true;
}

bool loadWorld(const std::string& filename, int & outSeed,
               float & outPlayerX, float & outPlayerY, float & outPlayerZ)
{
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        std::cerr << "Could not open file for loading world: " << filename << std::endl;
        return false;
    }

    extraBlocks.clear();

    // We set default in case file has no PLAYER line
    outPlayerX = 0.0f;
    outPlayerY = 10.0f;
    outPlayerZ = 0.0f;

    std::string token;
    while (true) {
        if (!(inFile >> token)) {
            break; // no more data
        }
        if (token == "SEED") {
            inFile >> outSeed;
        } else if (token == "PLAYER") {
            inFile >> outPlayerX >> outPlayerY >> outPlayerZ;
        } else {
            // We assume this is an integer for x
            int x = std::stoi(token);
            int y, z, typeInt;
            inFile >> y >> z >> typeInt;
            BlockType btype = static_cast<BlockType>(typeInt);
            extraBlocks[std::make_tuple(x, y, z)] = btype;
        }
    }

    inFile.close();
    std::cout << "World loaded from " << filename << std::endl;
    return true;
}
