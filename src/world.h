#ifndef WORLD_H
#define WORLD_H

#include <vector>
#include "renderer.h"
#include "block.h"
#include "perlin_noise.h"

class Player; // Forward declare Player

class World {
public:
    World();
    void render(Renderer& renderer, const Player& player);
    void generate();
    BlockType getBlockAtPosition(const glm::vec3& position) const;

private:
    std::vector<Block> blocks; // Store blocks in the world
};

#endif
