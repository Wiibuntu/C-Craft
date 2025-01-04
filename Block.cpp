// Block.cpp
#include "Block.h"

Block::Block(int x, int y, int z) : x(x), y(y), z(z) {}

int Block::getX() const { return x; }
int Block::getY() const { return y; }
int Block::getZ() const { return z; }
