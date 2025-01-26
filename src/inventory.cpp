#include "inventory.h"

Inventory::Inventory() {}

void Inventory::addBlock(BlockType type, int count) {
    blocks[type] += count;
}

bool Inventory::removeBlock(BlockType type, int count) {
    if (blocks[type] >= count) {
        blocks[type] -= count;
        return true;
    }
    return false;
}

int Inventory::getBlockCount(BlockType type) const {
    auto it = blocks.find(type);
    return it != blocks.end() ? it->second : 0;
}
