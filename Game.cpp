// Game.cpp
#include "Game.h"
#include "glfw3.h"
#include <iostream>

Game::Game() : isRunning(true) {}

void Game::run(GLFWwindow* window) {
    while (!glfwWindowShouldClose(window)) {
        processInput(window);
        update();
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Game::processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void Game::update() {
    player.update();
}

void Game::render() {
    renderer.render(world, player);
}

