#include "block.h"

Block::Block(BlockType type, glm::vec3 position) : type(type), position(position) {}

BlockType Block::getType() const {
    return type;
}

glm::vec3 Block::getPosition() const {
    return position;
}

void Block::render(Renderer& renderer) const {
    // Use renderer to draw the block using its position and type
}
