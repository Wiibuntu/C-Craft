#ifndef CAMERA_H
#define CAMERA_H

#include "math.h"

struct Camera {
    Vec3 position;  // Player's feet position
    float yaw;      // Horizontal angle in radians
    float pitch;    // Vertical angle in radians
};

#endif // CAMERA_H
