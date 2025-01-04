#ifndef GAME_H
#define GAME_H

#include "World.h"
#include "Player.h"
#include "Renderer.h"
#include "glfw3.h"

class Game {
public:
    Game();
    void run(GLFWwindow* window);

private:
    void processInput(GLFWwindow* window);
    void update();
    void render();

    bool isRunning;
    World world;
    Player player;
    Renderer renderer;
};

#endif