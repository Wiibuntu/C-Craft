#ifndef UTILS_H
#define UTILS_H

#include <glm/glm.hpp>

glm::vec3 toWorldCoordinates(int x, int y, int z);
glm::vec3 roundToNearestBlock(glm::vec3 position);

#endif
