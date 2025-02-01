#ifndef CAMERA_H
#define CAMERA_H

#include "math.h"

// Simple camera structure with position, yaw, and pitch.
struct Camera {
    Vec3 position;
    float yaw;   // Rotation around Y-axis (in radians)
    float pitch; // Rotation around X-axis (in radians)
};

#endif // CAMERA_H
