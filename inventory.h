#ifndef INVENTORY_H
#define INVENTORY_H

#include <vector>
#include "camera.h"
#include "world.h" // For BlockType constants like BLOCK_GRASS, BLOCK_STONE, etc.

class Inventory
{
public:
    Inventory();

    // Toggle (open/close) the inventory
    void toggle();
    bool isOpen() const { return m_isOpen; }

    // Update inventory logic (mouse input, etc.)
    void update(float dt, Camera &camera);

    // Render the inventory in the center of the screen
    void render();

    // Returns which block is currently selected by the user
    int getSelectedBlock() const { return m_selectedBlock; }

private:
    bool m_isOpen;          // True if the inventory overlay is active
    int  m_selectedBlock;   // The currently selected block type

    // Simple list of block IDs to display in the inventory
    std::vector<int> m_items;
};

#endif

