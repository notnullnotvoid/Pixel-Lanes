#ifndef GLUTILS_H
#define GLUTILS_H

#ifdef __APPLE__
# include <OpenGL/gl3.h>
#else
# include <GL/gl.h>
#endif

#include "types.hpp"

struct Texture {
	uint handle;
	int width;
	int height;
};

Texture load_texture(const char * imagePath);
GLuint create_shader(GLenum shaderType, const char * source);
GLuint create_program(const char * vertexShaderString, const char * fragmentShaderString);
void log_error(const char * when);

#endif
