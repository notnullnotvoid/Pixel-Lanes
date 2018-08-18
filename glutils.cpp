#include "glutils.hpp"
#include "lib/stb_image.h"
#include "types.hpp"

Texture load_texture(const char * imagePath) {
	int w, h, c;
	u8 * image = stbi_load(imagePath, &w, &h, &c, 4);

	if (image == nullptr) {
		fflush(stdout);
		fprintf(stderr, "ERROR: Could not load file %s\n", imagePath);
		fflush(stderr);
		return { 0, 0, 0 };
	}

	uint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

	//unbind???
	// glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(image);

	return { tex, w, h };
}

GLuint create_shader(GLenum shaderType, const char * source) {
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	//handle compilation errors
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

		GLchar * log = (GLchar *) malloc((len + 1) * sizeof(GLchar));
		glGetShaderInfoLog(shader, len, NULL, log);
		const char * shaderTypeString = "unknown";
		if (shaderType == GL_VERTEX_SHADER) { shaderTypeString = "vertex"; }
		if (shaderType == GL_FRAGMENT_SHADER) { shaderTypeString = "fragment"; }
		printf("Error while compiling %s shader: %s\n", shaderTypeString, log);
		free(log);

		//TODO: proper error handling
		exit(1);
	}

	return shader;
}

GLuint create_program(const char * vertexShaderString, const char * fragmentShaderString) {
	//compile shaders
	GLuint vertexShader = create_shader(GL_VERTEX_SHADER, vertexShaderString);
	GLuint fragmentShader = create_shader(GL_FRAGMENT_SHADER, fragmentShaderString);

	//compile program
	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	//program compilation cleanup
	glDetachShader(program, vertexShader);
	glDetachShader(program, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	//handle program compilation errors
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

		GLchar * log = (GLchar *) malloc((len + 1) * sizeof(GLchar));
		glGetProgramInfoLog(program, len, NULL, log);
		printf("Error while compiling program: %s\n", log);
		free(log);

		//TODO: proper error handling
		exit(1);
	}

	return program;
}

void log_error(const char * when) {
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		printf("GL ERROR DURING %s: %d\n", when, err);
	}
}
