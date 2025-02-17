#include "world.h"
#include "noise.h"    // for setNoiseSeed(...) if you use it
#include <iostream>
#include <fstream>

// -----------------------------------------------------------------------------
// The global map for additional / overridden blocks (trees, placed/destroyed).
// This must be defined exactly once across your project.
// -----------------------------------------------------------------------------
std::map<std::tuple<int,int,int>, BlockType> extraBlocks;

// -----------------------------------------------------------------------------
// loadWorld(...)
// Reads a text file and populates:
//   - outSeed, plus calls setNoiseSeed(...) to match your terrain generation
//   - outPlayerX, outPlayerY, outPlayerZ
//   - extraBlocks (including negative-sentinel entries for destroyed blocks)
// -----------------------------------------------------------------------------
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

    // 1) Read the seed
    in >> outSeed;
    // 2) Read the player position
    in >> outPlayerX >> outPlayerY >> outPlayerZ;

    // If you want your terrain to match that seed, do this:
    setNoiseSeed(outSeed);

    // 3) Read how many entries we stored in extraBlocks
    int count;
    in >> count;

    // Clear any existing entries, then read them from file
    extraBlocks.clear();
    for(int i=0; i<count; i++)
    {
        int bx, by, bz;
        int typeInt;   // we store block types as an int
        in >> bx >> by >> bz >> typeInt;

        // For normal blocks (>=0) or negative sentinel (<0), record them
        extraBlocks[{bx, by, bz}] = (BlockType)typeInt;
    }

    in.close();

    std::cout << "[loadWorld] Loaded seed=" << outSeed 
              << " player(" << outPlayerX << ","
              << outPlayerY << ","
              << outPlayerZ << "), "
              << "extraBlocks=" << count << "\n";
    return true;
}

// -----------------------------------------------------------------------------
// saveWorld(...)
// Writes out:
//   - seed
//   - player position
//   - all entries in extraBlocks, including negative-sentinel ones
// so that destroyed blocks remain destroyed after reloading.
// -----------------------------------------------------------------------------
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

    // 1) Write the seed
    out << seed << "\n";
    // 2) Write the player position
    out << playerX << " " << playerY << " " << playerZ << "\n";

    // 3) Count how many blocks we have in extraBlocks. 
    //    IMPORTANT: we do NOT skip negative sentinel now. 
    //    That way, destroyed blocks remain recorded.
    int count = (int)extraBlocks.size();
    out << count << "\n";

    // 4) Write each coordinate + type
    for(const auto &kv : extraBlocks)
    {
        const auto &pos = kv.first;
        BlockType bType = kv.second;

        int bx = std::get<0>(pos);
        int by = std::get<1>(pos);
        int bz = std::get<2>(pos);

        // bType might be >= 0 (normal blocks) or <0 (destroyed sentinel).
        // We save everything so we can restore it exactly.
        out << bx << " " 
            << by << " " 
            << bz << " " 
            << (int)bType << "\n";
    }

    out.close();

    std::cout << "[saveWorld] Saved seed=" << seed
              << " player(" << playerX << ","
              << playerY << ","
              << playerZ << ") with " 
              << count << " block overrides.\n";
    return true;
}

