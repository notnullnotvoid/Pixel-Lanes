#include "Imm.hpp"
#include "system.hpp"
#include "glutils.hpp"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Font load_font(const char * bmfont) {
	enum BlockType {
		OTHER, COMMON, PAGE, CHAR,
	};

	Font font = {};
	font.kernBias = -2;
	font.chars = (Char *) malloc(128 * sizeof(Char));
	memset(font.chars, 0, 128 * sizeof(Char));

	char * text = read_entire_file(bmfont);

	if (text == nullptr) {
		fprintf(stderr, "ERROR: Could not load file %s\n", bmfont);
		fflush(stdout);
		exit(1);
	}

	char * relativePath = nullptr;
	Char * ch = nullptr;

	//kerning pairs appear to have very little visual impact, especially at small glyph sizes,
	//so we leave them out of our implementation for size/memory/simplicity/performance reasons

	//simple stateful parsing algorithm which ignores line breaks because it can.
	//this assumes lots of things about the given font file for the sake of simplicity.
	//some things we assume:
	//- glyphs are contained only in one image
	//- characters in the atlas are only in the ASCII range
	//(basically it should work on all single-texture ascii bmfonts output by Hiero)
	BlockType type = OTHER;
	char * token = strtok(text, " \t\r\n");
	while (token != nullptr) {
		     if (!strcmp(token, "common" )) { type = COMMON;  }
		else if (!strcmp(token, "page"   )) { type = PAGE;    }
		else if (!strcmp(token, "char"   )) { type = CHAR;    }
		else if (type != OTHER) {
			//parse key-value pair
			char * eq = strchr(token, '=');
			if (eq == nullptr) {
				type = OTHER;
			} else {
				*eq = '\0';
				char * key = token;
				char * value = eq + 1;

				switch (type) {
					case COMMON: {
						if (!strcmp(key, "lineHeight")) {
							font.lineHeight = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "base")) {
							font.base = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "scaleW")) {
							font.scaleW = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "scaleH")) {
							font.scaleH = strtol(value, nullptr, 10);
						}
					} break;
					case PAGE: {
						if (!strcmp(key, "file")) {
							*strchr(value + 1, '"') = '\0';
							relativePath = value + 1;
						}
					} break;
					case CHAR: {
						if (!strcmp(key, "id")) {
							ch = font.chars + strtol(value, nullptr, 10);
						} else if (!strcmp(key, "x")) {
							ch->x = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "y")) {
							ch->y = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "width")) {
							ch->width = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "height")) {
							ch->height = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "xoffset")) {
							ch->xoffset = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "yoffset")) {
							ch->yoffset = strtol(value, nullptr, 10);
						} else if (!strcmp(key, "xadvance")) {
							ch->xadvance = strtol(value, nullptr, 10);
						}
					} break;
				}
			}
		}

		token = strtok(nullptr, " \t\r\n");
	}

	//make "absolute" (root-relative) path from file-relative path
	char * slash = strrchr(bmfont, '/');
	int folderPathLen = slash - bmfont + 1;
	char * imagePath = (char *) malloc(folderPathLen + strlen(relativePath) + 1);
	strncpy(imagePath, bmfont, folderPathLen);
	strcpy(imagePath + folderPathLen, relativePath);

	free(text); //not used past this point

	//load image
	font.tex = load_texture(imagePath);
	if (font.tex.handle == 0) {
		exit(1);
	}

	free(imagePath); //not used past this point

	log_error("FONT_LOAD");

	return font;
}

void free_font(Font &font) {
	free(font.chars);
	font.chars = nullptr;
	glDeleteTextures(1, &font.tex.handle);
	font.tex.handle = 0;
}
