#ifndef TREE_GENERATOR_H
#define TREE_GENERATOR_H

#include "world.h"
#include <glm/glm.hpp>

class TreeGenerator {
public:
    static void generateTree(World& world, glm::vec3 basePosition);
};

#endif
