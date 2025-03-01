#include "world.h"
#include "noise.h"    // for setNoiseSeed(...)
#include <iostream>
#include <fstream>

// Define extraBlocks (for terrain overrides)
std::unordered_map<std::tuple<int,int,int>, BlockType, TupleHash> extraBlocks;

// Define waterLevels (maps (x,y,z) to water level 1–8)
std::unordered_map<std::tuple<int,int,int>, int, TupleHash> waterLevels;

bool loadWorld(const char* filename,
               int &outSeed,
               float &outPlayerX,
               float &outPlayerY,
               float &outPlayerZ)
{
    std::ifstream in(filename);
    if(!in) {
        std::cerr << "[loadWorld] Could not open file '" << filename << "'\n";
        return false;
    }
    in >> outSeed;
    in >> outPlayerX >> outPlayerY >> outPlayerZ;
    setNoiseSeed(outSeed);
    int count;
    in >> count;
    extraBlocks.clear();
    for(int i = 0; i < count; i++)
    {
        int bx, by, bz, typeInt;
        in >> bx >> by >> bz >> typeInt;
        extraBlocks[{bx,by,bz}] = (BlockType)typeInt;
    }
    in.close();
    std::cout << "[loadWorld] Loaded seed=" << outSeed 
              << " player(" << outPlayerX << "," << outPlayerY << "," << outPlayerZ << "), "
              << "extraBlocks=" << count << "\n";
    return true;
}

bool saveWorld(const char* filename,
               int seed,
               float playerX,
               float playerY,
               float playerZ)
{
    std::ofstream out(filename);
    if(!out) {
        std::cerr << "[saveWorld] Could not open file '" << filename << "'\n";
        return false;
    }
    out << seed << "\n";
    out << playerX << " " << playerY << " " << playerZ << "\n";
    int count = (int)extraBlocks.size();
    out << count << "\n";
    for(const auto &kv : extraBlocks)
    {
        const auto &pos = kv.first;
        BlockType bType = kv.second;
        int bx = std::get<0>(pos);
        int by = std::get<1>(pos);
        int bz = std::get<2>(pos);
        out << bx << " " << by << " " << bz << " " << (int)bType << "\n";
    }
    out.close();
    std::cout << "[saveWorld] Saved seed=" << seed
              << " player(" << playerX << "," << playerY << "," << playerZ << ") with " 
              << count << " block overrides.\n";
    return true;
}

