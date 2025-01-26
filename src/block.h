#ifndef BLOCK_H
#define BLOCK_H

#include <glm/glm.hpp>
#include "renderer.h"

enum class BlockType {
    Grass,
    Dirt,
    Stone,
    Wood
};

class Block {
public:
    Block(BlockType type, glm::vec3 position);

    BlockType getType() const;
    glm::vec3 getPosition() const;

    void render(Renderer& renderer) const;

private:
    BlockType type;
    glm::vec3 position;
};

#endif
