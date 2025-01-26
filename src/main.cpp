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
    std::cout << "World initialized." << std::endl;

    while (!renderer.shouldClose()) {
        std::cout << "Rendering frame..." << std::endl;

        // Clear the screen
        renderer.clear();

        // Render the world
        world.render(renderer, player);
        std::cout << "World rendered." << std::endl;

        // Simulate input (replace with actual input handling)
        Input input = {};
        input.moveForward = false; // Example: no movement
        input.moveBackward = false;
        input.moveLeft = false;
        input.moveRight = false;
        input.jump = false;

        // Update the player
        player.update(input);
        std::cout << "Player updated." << std::endl;

        // Swap buffers and poll events
        renderer.swapBuffers();
        glfwPollEvents(); // Ensure events are handled
        std::cout << "Buffers swapped and events polled." << std::endl;
    }

    std::cout << "Game terminated." << std::endl;
    return 0;
}
