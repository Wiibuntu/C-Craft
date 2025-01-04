// Game.cpp
#include "Game.h"
#include "glfw3.h"
#include <iostream>

Game::Game() : isRunning(true) {}

void Game::run() {
    while (isRunning) {
        processInput();
        update();
        render();
    }
}

void Game::processInput() {
    // Handle input (move player, exit game, etc.)
}

void Game::update() {
    // Update world, player, physics, etc.
    player.update();
}

void Game::render() {
    renderer.render(world, player);
}


