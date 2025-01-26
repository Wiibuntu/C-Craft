#ifndef WORLD_H
#define WORLD_H

#include <vector>       // Include vector for std::vector
#include "renderer.h"
#include "block.h"      // Include Block and BlockType

class Player; // Forward declare Player

class World {
public:
    World();
    void render(Renderer& renderer, const Player& player);
    void generate();
    void addBlock(BlockType type, glm::vec3 position); // Add a block to the world

private:
    std::vector<Block> blocks; // Store blocks in the world
};

#endif
