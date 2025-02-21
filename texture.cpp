#include "texture.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glew.h>

GLuint loadTexture(const char* filename) {
    // Flip the image vertically so that (0,0) is at the bottom left.
    stbi_set_flip_vertically_on_load(true);
    
    int width, height, originalChannels;
    // Force 4 channels (RGBA)
    unsigned char* image = stbi_load(filename, &width, &height, &originalChannels, 4);
    if (!image) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        return 0;
    }
    
    // Now we know for sure we have 4 channels
    GLenum format = GL_RGBA;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Use clamp-to-edge to stretch textures instead of bleeding.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Use anisotropic filtering if available
    GLfloat maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, (GLint)maxAniso);

    // High-quality filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload data (RGBA, 8-bit)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // alignment
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);
    return textureID;
}

