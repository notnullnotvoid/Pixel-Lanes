#ifndef IMM_HPP
#define IMM_HPP

#include "types.hpp"
#include "math.hpp"
#include "glutils.hpp"

struct Char {
	uint16_t x, y, width, height;
	int16_t xoffset, yoffset, xadvance;
};

struct Font {
	Char * chars;
	Texture tex;
	//TODO: since Texture struct stores its own width/height, is scaleW/scaleH unnecessary?
	uint16_t lineHeight, base, scaleW, scaleH;
	float kernBias;
};

Font load_font(const char * bmfont);
void free_font(Font &font);

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

enum Constants {
	//line cap
	SQUARE, PROJECT, ROUND,

	//line joint
	MITER, BEVEL, //ROUND,

	//line close
	CLOSE,
};

struct Color {
	uint8_t r, g, b, a;
};

struct Vertex {
	float x, y, z;
	Color c;
	float u, v;
	float f;
};

struct Imm {
	Vertex * data;

	//vertex counts
	int total;
	int used;

	//OpenGL IDs
	uint32_t program;
	uint32_t vao;
	uint32_t vbo;
	Texture tex;

	//debug stats
	int flushes;
	int vertsFlushed;
	int maxVertsFlushed;
	int lastVertsFlushed;

	Color fill;
	Color stroke;

	Mat4 matrix;
	int ww, wh, dw, dh; //window size and viewport size, respectively
	float offx, offy; //applied to all vertices

	Font font;

	float depth;

	float r; //line width (radius)
	uint8_t cap;
	uint8_t join;

	bool filling; //mmm, very filling
	bool stroking; //lol

	//line drawing state
	int lineVertexCount;
	float fx, fy; //first vertex
	float sx, sy, sdx, sdy; //second vertex
	float px, py, pdx, pdy; //previous vertex
	float lx, ly; //last vertex

	//general debug state
	int totalLineVerts;
	double startTime;

	int circleDetail(float radius);
	int circleDetail(float radius, float delta);
	void incrementDepth();
	void check(int newVerts);
	void vertex(float x, float y, Color color);
	void vertex(float x, float y, Color color, float u, float v);

	void init(int size);
	void finalize();

	void begin(int width, int height, int drawableWidth, int drawableHeight, bool textured);
	void end(double dt);

	void clip(float x, float y, float w, float h);
	void noClip();

	void flush();
	void triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color fill);
	void arcJoin(float x, float y, float start, float delta);
	void beginLine();
	void lineVertex(float x, float y);
	void lineCap(float x, float y, float angle);
	void endLine();
	void endLine(Constants close);

	void drawText(const char * text, float x, float y, float s, Color color);
	void drawTextWithColors(const char * text, float x, float y, float s,
		u8 * colors, const Color * atlas);
	void useFont(Font f);
	void useTexture(Texture texture);

	void circle(float x, float y, float r);
	void rect(float x, float y, float w, float h);
	void rect(float x, float y, float w, float h, Color c);
	void quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
		Color c);
};

float text_width(Font font, float scale, const char * text);

inline Imm create_imm(int newVerts) {
	Imm imm;
	imm.init(newVerts);
	return imm;
}

#endif
