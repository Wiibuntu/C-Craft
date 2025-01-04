#ifndef GAME_H
#define GAME_H

#include "World.h"
#include "Player.h"
#include "Renderer.h"

class Game {
public:
    Game();
    void run();

private:
    void processInput();
    void update();
    void render();

    bool isRunning;
    World world;
    Player player;
    Renderer renderer;
};

#endif