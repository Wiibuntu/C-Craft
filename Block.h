// Block.h
#ifndef BLOCK_H
#define BLOCK_H

class Block {
public:
    Block(int x, int y, int z);
    int getX() const;
    int getY() const;
    int getZ() const;

private:
    int x, y, z;
};

#endif
