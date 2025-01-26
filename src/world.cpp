#include "world.h"
#include "player.h"

World::World() {
    generate();
}

void World::render(Renderer& renderer, const Player& player) {
    for (const Block& block : blocks) {
        block.render(renderer);
    }
}

void World::generate() {
    // Implement terrain generation logic
}

void World::addBlock(BlockType type, glm::vec3 position) {
    blocks.emplace_back(type, position); // Add a block to the world
}
