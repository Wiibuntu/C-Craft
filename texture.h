#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>

// Load a texture from file and return its OpenGL texture ID.
GLuint loadTexture(const char* filename);

#endif // TEXTURE_H
