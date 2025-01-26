#ifndef INVENTORY_H
#define INVENTORY_H

#include <unordered_map>
#include "block.h"

class Inventory {
public:
    Inventory();

    void addBlock(BlockType type, int count);
    bool removeBlock(BlockType type, int count);
    int getBlockCount(BlockType type) const;

private:
    std::unordered_map<BlockType, int> blocks;
};

#endif
