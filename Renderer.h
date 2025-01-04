// Renderer.h
#ifndef RENDERER_H
#define RENDERER_H

#include "World.h"
#include "Player.h"

class Renderer {
public:
    void render(World& world, Player& player);
};

#endif