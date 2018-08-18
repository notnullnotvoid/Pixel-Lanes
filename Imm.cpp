#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "glutils.hpp"
#include "Imm.hpp"
#include "math.hpp"
#include "system.hpp"

void Imm::init(int size) {
	*this = {}; //zero-initialize

	//compile unified shader
	char * vertSource = read_entire_file("res/imm.vert");
	char * fragSource = read_entire_file("res/imm.frag");
	program = create_program(vertSource, fragSource);

	//allocate data
	data = (Vertex *) malloc(sizeof(Vertex) * size);

	//allocate VAO
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//allocate VBO
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	//setup vertex attributes
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *) (3 * 4));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (4 * 4));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (6 * 4));
	glEnableVertexAttribArray(3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	total = size;
	used = 0;

	//set general properties
	fill = { 200, 200, 200, 255 };
	stroke = { 0, 0, 0, 255 };
	matrix = { 1, 0, 0, 0,
	           0, 1, 0, 0,
	           0, 0, 1, 0,
	           0, 0, 0, 1, };
	depth = 1.0f;
	r = 1;
	cap = SQUARE;
	join = MITER;
	filling = true;
	stroking = true;
}

void Imm::finalize() {
	glDeleteProgram(program);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	free(data);
}

void fit_window(Mat4 &matrix, float width, float height) {
	matrix.m00 = 2.0f/width;
	matrix.m11 = 2.0f/height;
	matrix.m03 = -1.0f;
	matrix.m13 = -1.0f;
}

void Imm::begin(int width, int height, int drawableWidth, int drawableHeight, bool textured) {
	fit_window(matrix, width, height);

	ww = width;
	wh = height;
	dw = drawableWidth;
	dh = drawableHeight;

	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	depth = 1.0f;
	if (textured) {
		// glClearColor(1, 1, 1, 1);
		// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClear(GL_DEPTH_BUFFER_BIT);
		check(6);
		incrementDepth();
		Color white = { 255, 255, 255, 255 };
		float w = tex.width;
		float h = tex.height;
		vertex(0    , 0     , white, 0, 0);
		vertex(0    , height, white, 0, h);
		vertex(width, 0     , white, w, 0);
		vertex(0    , height, white, 0, h);
		vertex(width, 0     , white, w, 0);
		vertex(width, height, white, w, h);
		// vertex(0    , 0     , white);
		// vertex(0    , height, white);
		// vertex(width, 0     , white);
		// vertex(0    , height, white);
		// vertex(width, 0     , white);
		// vertex(width, height, white);
		// log_error("FUCK EVERYTHING");
		flush();
	} else {
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

void Imm::end(double dt) {
	flush();

	#ifdef IMM_DEBUG
		printf("microseconds: %8lld  line: %4d\n",
			(int64_t)(dt * 1000000), totalLineVerts);
		printf("    flat  flushes: %4d  verts: %8d  max: %4d  last: %4d\n",
			flat.flushes, flat.vertsFlushed,
			flat.maxVertsFlushed, flat.lastVertsFlushed);
		printf("    text  flushes: %4d  verts: %8d  max: %4d  last: %4d\n\n",
			text.flushes, text.vertsFlushed,
			text.maxVertsFlushed, text.lastVertsFlushed);
	#endif

	flushes = 0;
	vertsFlushed = 0;
	maxVertsFlushed = 0;
	lastVertsFlushed = 0;

	totalLineVerts = 0;
}

void Imm::clip(float x, float y, float w, float h) {
	flush();

	float glScaleX = dw / (float) ww;
	float glScaleY = dh / (float) wh;
	float sx = x * glScaleX;
	float sy = dh - y * glScaleY - h * glScaleY;
	float sw = w * glScaleX;
	float sh = h * glScaleY;

	glScissor(sx, sy, sw, sh);
	glEnable(GL_SCISSOR_TEST);
}

void Imm::noClip() {
	flush();
	glDisable(GL_SCISSOR_TEST);
}

void Imm::incrementDepth() {
	//by resetting the depth buffer when needed, we are able to have arbitrarily many
	//layers, unlimited by depth buffer precision. in practice, the precision of this
	//algorithm seems to be very good (~1,000,000 layers), so it pretty much won't happen
	//unless you're drawing enough geometry per frame to set your computer on fire
	if (depth < -0.9999f) {
		flush();
		glClear(GL_DEPTH_BUFFER_BIT);
		//depth test will fail at depth = 1.0 after clearing the depth buffer,
		//but since we always increment before drawing anything, this should be okay
		depth = 1.0f;
	}

	//found to be the smallest possible increment value for a 24-bit depth buffer
	//through trial and error. as numbers approach zero, absolute floating point
	//precision increases, while absolute fixed point precision stays the same,
	//so regardless of representation, this value should work for all depths in
	//range (-1, 1), as long as it works for depths at either end of the range
	// depth -= 0.0000001f;

	//the above value was found to be inconsistent in some cases,
	//so we use a larger value instead
	depth -= 0.000001f;
}

void Imm::flush() {
	if (used == 0) {
		return;
	}

	//bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex.handle);

	glUseProgram(program);
	GLint scaleLocation = glGetUniformLocation(program, "scale");
	glUniform2f(scaleLocation, 1.0f / tex.width, 1.0f / tex.height);

	//upload vertex data
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, used * sizeof(Vertex), data, GL_DYNAMIC_DRAW);
	//BufferSubData is way slower than BufferData for some reason, at least with small batches
	//I have no idea why - look into this! (maybe ask on StackOverflow)
	// glBufferSubData(GL_ARRAY_BUFFER, 0, flat.used * sizeof(FlatVert), flat.data);

	glUseProgram(program);
	glBindVertexArray(vao);

	//TODO: offload this work? (get transform loc only once, and push uniform only when set)
	GLint transformLocation = glGetUniformLocation(program, "transform");
	glUniformMatrix4fv(transformLocation, 1, GL_TRUE, (float *)(&matrix));

	glDrawArrays(GL_TRIANGLES, 0, used);

	//unbind things (this isn't strictly necessary but I don't think it hurts any)
	glBindVertexArray(0);
	glUseProgram(0);

	//track debug stats
	flushes += 1;
	vertsFlushed += used;
	lastVertsFlushed = used;
	if (used > maxVertsFlushed) {
		maxVertsFlushed = used;
	}

	//reset state
	used = 0;
}

void Imm::check(int newVerts) {
	if (used + newVerts > total) {
		flush();
	}
}

void Imm::vertex(float x, float y, Color color) {
	data[used] = { x + offx, y + offy, depth, color, 0, 0, 0 };
	used += 1;
}

void Imm::vertex(float x, float y, Color color, float u, float v) {
	data[used] = { x + offx, y + offy, depth, color, u, v, 1 };
	used += 1;
}

void Imm::triangle(float x1, float y1, float x2, float y2, float x3, float y3, Color fill) {
	check(3);

	vertex(x1, y1, fill);
	vertex(x2, y2, fill);
	vertex(x3, y3, fill);
}

void Imm::drawText(const char * text, float x, float y, float s, Color color) {
	useTexture(font.tex);
	for (const char * c = text; *c != '\0'; ++c) {
		incrementDepth();

		Char * ch = font.chars + *c;

		float x0 = x + ch->xoffset * s, y0 = y + ch->yoffset * s;
		float x1 = x0 + ch->width * s, y1 = y0 + ch->height * s;
		float u0 = ch->x, v0 = ch->y;
		float u1 = u0 + ch->width, v1 = v0 + ch->height;

		check(6);

		vertex(x0, y0, color, u0, v0);
		vertex(x0, y1, color, u0, v1);
		vertex(x1, y0, color, u1, v0);
		vertex(x0, y1, color, u0, v1);
		vertex(x1, y0, color, u1, v0);
		vertex(x1, y1, color, u1, v1);

		x += font.kernBias * s + ch->xadvance * s;
	}
}

void Imm::drawTextWithColors(const char * text, float x, float y, float s,
		u8 * colors, const Color * atlas) {
	useTexture(font.tex);
	for (int c = 0; text[c] != '\0'; ++c) {
		incrementDepth();

		Char * ch = font.chars + text[c];

		float x0 = x + ch->xoffset * s, y0 = y + ch->yoffset * s;
		float x1 = x0 + ch->width * s, y1 = y0 + ch->height * s;
		float u0 = ch->x, v0 = ch->y;
		float u1 = u0 + ch->width, v1 = v0 + ch->height;

		check(6);

		Color color = atlas[colors[c]];

		vertex(x0, y0, color, u0, v0);
		vertex(x0, y1, color, u0, v1);
		vertex(x1, y0, color, u1, v0);
		vertex(x0, y1, color, u0, v1);
		vertex(x1, y0, color, u1, v0);
		vertex(x1, y1, color, u1, v1);

		x += font.kernBias * s + ch->xadvance * s;
	}
}

void Imm::useFont(Font f) {
	useTexture(font.tex);
	font = f;
}

void Imm::useTexture(Texture texture) {
	if (tex.handle != texture.handle) {
		flush();
		tex = texture;
	}
}

//estimates the number of segments per octant needed to draw a circle
//at a given radius that appears to be completely smooth
int Imm::circleDetail(float radius) {
	//TODO: use matrix scale to determine actual on-screen size
	return fminf(11, sqrtf(radius / 4)) + 1;
}

int Imm::circleDetail(float radius, float delta) {
	//TODO: use matrix scale to determine actual on-screen size
	return fminf(11, sqrtf(radius / 4)) / QUARTER_PI * fabsf(delta) + 1;
}

void Imm::arcJoin(float x, float y, float start, float delta) {
	//XXX: should we use constant step + remainder? it would eliminate apparent jiggling
	int segments = circleDetail(r, delta);
	float step = delta / segments;

	float x1 = x + sinf(start) * r;
	float y1 = y + cosf(start) * r;
	for (int i = 0; i < segments; ++i) {
		start += step;
		float x2 = x + sinf(start) * r;
		float y2 = y + cosf(start) * r;

		triangle(x, y, x1, y1, x2, y2, stroke);

		x1 = x2;
		y1 = y2;
	}

	//TODO: fix the bug where pixels are sometimes not filled at the seams of arc joins
}

void Imm::beginLine() {
	lineVertexCount = 0;
	incrementDepth();
}

void Imm::lineVertex(float x, float y) {
	//disallow adding consecutive duplicate totalVerts,
	//as it is pointless and just creates an extra edge case
	if (lineVertexCount > 0 && x == lx && y == ly) {
		return;
	}

	totalLineVerts += 1;

	if (lineVertexCount == 0) {
		fx = x;
		fy = y;
	} else if (lineVertexCount == 1) {
		sx = x;
		sy = y;
	} else {
		//find leg angles
		float angle1 = atan2f(lx - px, ly - py);
		float angle2 = atan2f(lx -  x, ly -  y);

		//find minimum absolute angle between the two legs
		//FROM: https://stackoverflow.com/a/7869457/3064745
		//NOTE: this only works for angles that are in range [-180, 180] !!!
		float diff = angle1 - angle2;
		diff += diff > PI? -TWO_PI : diff < -PI? TWO_PI : 0;

		if (join == BEVEL || join == ROUND || fabsf(diff) < PI/15 || fabsf(diff) > PI - 0.001f) {
			float dx = lx - px;
			float dy = ly - py;
			float d = sqrtf(dx*dx + dy*dy);
			float tx =  dy / d * r;
			float ty = -dx / d * r;

			if (lineVertexCount == 2) {
				sdx = tx;
				sdy = ty;
			} else {
				triangle(px - pdx, py - pdy, px + pdx, py + pdy, lx - tx, ly - ty, stroke);
				triangle(px + pdx, py + pdy, lx - tx, ly - ty, lx + tx, ly + ty, stroke);
			}

			dx = x - lx;
			dy = y - ly;
			d = sqrtf(dx*dx + dy*dy);
			float nx =  dy / d * r;
			float ny = -dx / d * r;

			if (join == ROUND) {
				float theta1 = diff < 0? atan2f(tx, ty) : atan2f(-tx, -ty);
				float theta2 = diff < 0? atan2f(nx, ny) : atan2f(-nx, -ny);
				float delta = theta2 - theta1;
				delta += delta > PI? -TWO_PI : delta < -PI? TWO_PI : 0;
				arcJoin(lx, ly, theta1, delta);
			} else if (diff < 0) {
				triangle(lx, ly, lx + tx, ly + ty, lx + nx, ly + ny, stroke);
			} else {
				triangle(lx, ly, lx - tx, ly - ty, lx - nx, ly - ny, stroke);
			}

			pdx = nx;
			pdy = ny;
		} else {
			//find offset (hypotenuse) of miter joint using trig
			float theta = HALF_PI - diff/2;
			float offset = r / cosf(theta);

			//find bisecting vector
			float angle = (angle1 + angle2)/2;
			float bx = sinf(angle) * offset;
			float by = cosf(angle) * offset;
			if (fabsf(angle1 - angle2) < PI) {
				bx *= -1;
				by *= -1;
			}

			if (lineVertexCount == 2) {
				sdx = bx;
				sdy = by;
			} else {
				triangle(px - pdx, py - pdy, px + pdx, py + pdy, lx - bx, ly - by, stroke);
				triangle(px + pdx, py + pdy, lx - bx, ly - by, lx + bx, ly + by, stroke);
			}

			pdx = bx;
			pdy = by;
		}
	}

	px = lx;
	py = ly;
	lx = x;
	ly = y;

	lineVertexCount += 1;
}

void Imm::lineCap(float x, float y, float angle) {
	arcJoin(x, y, angle - HALF_PI, PI); //TODO: optimize this
}

void Imm::endLine() {
	if (lineVertexCount < 3) {
		return;
	}

	//draw last line (with cap)
	float dx = lx - px;
	float dy = ly - py;
	float d = sqrtf(dx*dx + dy*dy);
	float tx =  dy / d * r;
	float ty = -dx / d * r;

	if (cap == PROJECT) {
		lx -= ty;
		ly += tx;
	}

	triangle(px - pdx, py - pdy, px + pdx, py + pdy, lx - tx, ly - ty, stroke);
	triangle(px + pdx, py + pdy, lx - tx, ly - ty, lx + tx, ly + ty, stroke);

	if (cap == ROUND) {
		lineCap(lx, ly, atan2f(dx, dy));
	}

	//draw first line (with cap)
	dx = fx - sx;
	dy = fy - sy;
	d = sqrtf(dx*dx + dy*dy);
	tx =  dy / d * r;
	ty = -dx / d * r;

	if (cap == PROJECT) {
		fx -= ty;
		fy += tx;
	}

	triangle(sx - sdx, sy - sdy, sx + sdx, sy + sdy, fx + tx, fy + ty, stroke);
	triangle(sx + sdx, sy + sdy, fx + tx, fy + ty, fx - tx, fy - ty, stroke);

	if (cap == ROUND) {
		lineCap(fx, fy, atan2f(dx, dy));
	}
}

void Imm::endLine(Constants close) {
	if (lineVertexCount < 3) {
		return;
	}

	if (close != CLOSE) {
		//why would you call this with the wrong argument? uh-oh... ignore for now!
		endLine();
		return;
	}

	//draw the last two legs
	lineVertex(fx, fy);
	lineVertex(sx, sy);

	//connect first and second vertices
	triangle(px - pdx, py - pdy, px + pdx, py + pdy, sx - sdx, sy - sdy, stroke);
	triangle(px + pdx, py + pdy, sx - sdx, sy - sdy, sx + sdx, sy + sdy, stroke);
}



void Imm::circle(float x, float y, float radius) {
	if (!filling && !stroking) {
		return;
	}

	//TODO: profile and figure out whether it's faster to calculate sine/cosine
	//values twice vs. storing them in a small local lookup table
	if (filling) {
		incrementDepth();

		int segments = circleDetail(radius);
		float step = QUARTER_PI / segments;

		float x1 = 0;
		float y1 = radius;
		float angle = 0;
		for (int i = 0; i < segments; ++i) {
			angle += step;
			float x2, y2;
			//this is not just for performance
			//it also ensures the circle is drawn with no diagonal gaps
			if (i < segments - 1) {
				x2 = sinf(angle) * radius;
				y2 = cosf(angle) * radius;
			} else {
				x2 = y2 = sinf(QUARTER_PI) * radius;
			}

			triangle(x, y, x + x1, y + y1, x + x2, y + y2, fill);
			triangle(x, y, x + x1, y - y1, x + x2, y - y2, fill);
			triangle(x, y, x - x1, y + y1, x - x2, y + y2, fill);
			triangle(x, y, x - x1, y - y1, x - x2, y - y2, fill);

			triangle(x, y, x + y1, y + x1, x + y2, y + x2, fill);
			triangle(x, y, x + y1, y - x1, x + y2, y - x2, fill);
			triangle(x, y, x - y1, y + x1, x - y2, y + x2, fill);
			triangle(x, y, x - y1, y - x1, x - y2, y - x2, fill);

			x1 = x2;
			y1 = y2;
		}
	}

	if (stroking) {
		incrementDepth();

		float r1 = radius - r;
		float r2 = radius + r;

		int segments = circleDetail(r2);
		float step = QUARTER_PI / segments;

		float s1 = 0; //sin
		float c1 = 1; //cos
		float angle = 0;
		for (int i = 0; i < segments; ++i) {
			angle += step;
			float s2, c2;
			//this is not just for performance
			//it also ensures the circle is drawn with no diagonal gaps
			if (i < segments - 1) {
				s2 = sinf(angle);
				c2 = cosf(angle);
			} else {
				s2 = c2 = sinf(QUARTER_PI);
			}

			triangle(x + s1*r1, y + c1*r1, x + s2*r1, y + c2*r1, x + s1*r2, y + c1*r2, stroke);
			triangle(x + s2*r1, y + c2*r1, x + s1*r2, y + c1*r2, x + s2*r2, y + c2*r2, stroke);

			triangle(x + s1*r1, y - c1*r1, x + s2*r1, y - c2*r1, x + s1*r2, y - c1*r2, stroke);
			triangle(x + s2*r1, y - c2*r1, x + s1*r2, y - c1*r2, x + s2*r2, y - c2*r2, stroke);

			triangle(x - s1*r1, y + c1*r1, x - s2*r1, y + c2*r1, x - s1*r2, y + c1*r2, stroke);
			triangle(x - s2*r1, y + c2*r1, x - s1*r2, y + c1*r2, x - s2*r2, y + c2*r2, stroke);

			triangle(x - s1*r1, y - c1*r1, x - s2*r1, y - c2*r1, x - s1*r2, y - c1*r2, stroke);
			triangle(x - s2*r1, y - c2*r1, x - s1*r2, y - c1*r2, x - s2*r2, y - c2*r2, stroke);

			triangle(x + c1*r1, y + s1*r1, x + c2*r1, y + s2*r1, x + c1*r2, y + s1*r2, stroke);
			triangle(x + c2*r1, y + s2*r1, x + c1*r2, y + s1*r2, x + c2*r2, y + s2*r2, stroke);

			triangle(x + c1*r1, y - s1*r1, x + c2*r1, y - s2*r1, x + c1*r2, y - s1*r2, stroke);
			triangle(x + c2*r1, y - s2*r1, x + c1*r2, y - s1*r2, x + c2*r2, y - s2*r2, stroke);

			triangle(x - c1*r1, y + s1*r1, x - c2*r1, y + s2*r1, x - c1*r2, y + s1*r2, stroke);
			triangle(x - c2*r1, y + s2*r1, x - c1*r2, y + s1*r2, x - c2*r2, y + s2*r2, stroke);

			triangle(x - c1*r1, y - s1*r1, x - c2*r1, y - s2*r1, x - c1*r2, y - s1*r2, stroke);
			triangle(x - c2*r1, y - s2*r1, x - c1*r2, y - s1*r2, x - c2*r2, y - s2*r2, stroke);

			s1 = s2;
			c1 = c2;
		}
	}
}

void Imm::rect(float x, float y, float w, float h) {
	if (filling) {
		incrementDepth();

		triangle(x, y, x + w, y, x + w, y + h, fill);
		triangle(x, y, x, y + h, x + w, y + h, fill);
	}

	if (stroking) {
		incrementDepth();

		//suppport negative w and h
		float x0 = w < 0? x + w : x;
		float y0 = h < 0? y + h : y;
		float x1 = w < 0? x : x + w;
		float y1 = h < 0? y : y + h;

		triangle(x0 - r, y0 - r, x0 + r, y0 + r, x1 + r, y0 - r, stroke);
		triangle(x0 + r, y0 + r, x1 + r, y0 - r, x1 - r, y0 + r, stroke);
		triangle(x1 + r, y0 - r, x1 - r, y0 + r, x1 + r, y1 + r, stroke);
		triangle(x1 - r, y0 + r, x1 + r, y1 + r, x1 - r, y1 - r, stroke);
		triangle(x1 + r, y1 + r, x1 - r, y1 - r, x0 - r, y1 + r, stroke);
		triangle(x1 - r, y1 - r, x0 - r, y1 + r, x0 + r, y1 - r, stroke);
		triangle(x0 - r, y1 + r, x0 + r, y1 - r, x0 - r, y0 - r, stroke);
		triangle(x0 + r, y1 - r, x0 - r, y0 - r, x0 + r, y0 + r, stroke);
	}
}

void Imm::rect(float x, float y, float w, float h, Color c) {
	incrementDepth();

	triangle(x, y, x + w, y, x + w, y + h, c);
	triangle(x, y, x, y + h, x + w, y + h, c);
}

void Imm::quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
		Color c) {
	incrementDepth();

	triangle(x1, y1, x2, y2, x3, y3, c);
	triangle(x1, y1, x3, y3, x4, y4, c);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

float text_width(Font font, float scale, const char * text) {
	float width = 0;
	for (const char * c = text; *c != '\0'; ++c) {
		Char * ch = font.chars + *c;
		width += font.kernBias * scale + ch->xadvance * scale;
	}
	return width;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
