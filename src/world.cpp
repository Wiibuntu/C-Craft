#include "world.h"
#include <iostream>
#include "player.h"
#include <glm/glm.hpp>

World::World() {
    generate();
}

void World::generate() {
    std::cout << "Generating world..." << std::endl;

    // Generate terrain using Perlin noise
    int worldWidth = 16;  // Width of the world in blocks
    int worldHeight = 16; // Depth of the world in blocks
    float scale = 0.1f;   // Noise scale

    for (int x = 0; x < worldWidth; ++x) {
        for (int z = 0; z < worldHeight; ++z) {
            float height = perlinNoise(x * scale, z * scale) * 8.0f; // Scale height
            int blockHeight = static_cast<int>(height);

            // Add ground blocks
            for (int y = 0; y <= blockHeight; ++y) {
                BlockType type = (y == blockHeight) ? BlockType::Grass : BlockType::Dirt;
                blocks.emplace_back(type, glm::vec3(x, y, z));
            }
        }
    }

    std::cout << "World generated with " << blocks.size() << " blocks." << std::endl;
}

void World::render(Renderer& renderer, const Player& player) {
    for (const Block& block : blocks) {
        block.render(renderer);
    }
}

void World::addBlock(BlockType type, glm::vec3 position) {
    blocks.emplace_back(type, position);
}

// Ensure this matches the declaration in world.h
BlockType World::getBlockAtPosition(const glm::vec3& position) const {
    for (const Block& block : blocks) {
        if (block.getPosition() == position) {
            return block.getType();
        }
    }
    return BlockType::Air; // Default to air if no block is found
}