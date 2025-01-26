#version 330 core

in vec2 TexCoord; // Texture coordinates from the vertex shader

out vec4 FragColor; // Final output color

uniform sampler2D texture1; // Texture sampler

void main() {
    FragColor = texture(texture1, TexCoord); // Sample the texture
}
