// World.h
#ifndef WORLD_H
#define WORLD_H

#include "Block.h"
#include <vector>

class World {
public:
    World();
    void generate();
    Block getBlock(int x, int y, int z);

private:
    std::vector<Block> blocks;
};

#endif
