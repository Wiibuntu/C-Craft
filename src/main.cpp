#include <iostream>
#include "renderer.h"
#include "world.h"
#include "player.h"

int main() {
    std::cout << "Starting game..." << std::endl;

    Renderer renderer(800, 600, "Minecraft Clone");
    std::cout << "Renderer initialized." << std::endl;

    Player player;
    std::cout << "Player initialized." << std::endl;

    World world;
    std::cout << "World generated." << std::endl;

    while (!renderer.shouldClose()) {
        renderer.clear();

        // Render the world
        world.render(renderer, player);

        // Simulate input (to be replaced with actual input handling)
        Input input = {};
        input.moveForward = false;
        input.moveBackward = false;
        input.moveLeft = false;
        input.moveRight = false;
        input.jump = false;

        player.update(input);

        renderer.swapBuffers();
        glfwPollEvents();
    }

    std::cout << "Game terminated." << std::endl;
    return 0;
}
