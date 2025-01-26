#include "tree_generator.h"
#include "block.h" // Include for BlockType
#include "world.h" // Include World to access addBlock

void TreeGenerator::generateTree(World& world, glm::vec3 basePosition) {
    // Generate trunk
    for (int i = 0; i < 5; ++i) {
        world.addBlock(BlockType::Wood, basePosition + glm::vec3(0, i, 0));
    }

    // Generate leaves
    for (int x = -2; x <= 2; ++x) {
        for (int y = 4; y <= 6; ++y) {
            for (int z = -2; z <= 2; ++z) {
                if (abs(x) + abs(y - 5) + abs(z) <= 3) {
                    world.addBlock(BlockType::Grass, basePosition + glm::vec3(x, y, z));
                }
            }
        }
    }
}
