#include <iostream>
#include "renderer.h"
#include "world.h"
#include "player.h"

int main() {
    Renderer renderer(800, 600, "Minecraft Clone");
    Player player;
    World world;

    while (!renderer.shouldClose()) {
        renderer.clear();

        // Handle input manually
        Input input = {};
        // Add actual input handling logic here
        input.moveForward = false; // Replace with real input checks
        input.moveBackward = false;
        input.moveLeft = false;
        input.moveRight = false;
        input.jump = false;

        world.render(renderer, player);
        player.update(input);

        renderer.swapBuffers();
    }

    return 0;
}
