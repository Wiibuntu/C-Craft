#ifndef CAMERA_H
#define CAMERA_H

#include "math.h"

// A simple camera (player) structure with position, yaw, and pitch.
struct Camera {
    Vec3 position;
    float yaw;   // rotation around Y-axis (in radians)
    float pitch; // rotation around X-axis (in radians)
};

#endif // CAMERA_H
