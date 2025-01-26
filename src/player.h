#ifndef PLAYER_H
#define PLAYER_H

#include "renderer.h"

// Define an Input struct for handling player input
struct Input {
    bool moveForward;
    bool moveBackward;
    bool moveLeft;
    bool moveRight;
    bool jump;
};

class Player {
public:
    Player();
    void update(const Input& input);
};

#endif
