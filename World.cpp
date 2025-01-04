// World.cpp
#include "World.h"
#include <cstdlib>

World::World() {
    generate();
}

void World::generate() {
    // Simple random terrain generation
    for (int x = 0; x < 100; x++) {
        for (int z = 0; z < 100; z++) {
            int height = rand() % 10 + 1;
            for (int y = 0; y < height; y++) {
                blocks.push_back(Block(x, y, z));
            }
        }
    }
}

Block World::getBlock(int x, int y, int z) {
    for (auto& block : blocks) {
        if (block.getX() == x && block.getY() == y && block.getZ() == z) {
            return block;
        }
    }
    return Block(-1, -1, -1); // Default invalid block
}
