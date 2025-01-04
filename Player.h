// Player.h
#ifndef PLAYER_H
#define PLAYER_H

class Player {
public:
    Player();
    void update();
    float getX() const;
    float getY() const;
    float getZ() const;

private:
    float x, y, z;
};

#endif
