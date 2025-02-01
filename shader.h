#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>

GLuint compileShader(GLenum type, const char* source);
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);

#endif // SHADER_H
