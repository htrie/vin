/* This file is part of libschrift.
 *
 * © 2019-2022 Thomas Oltmann and contributors
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#pragma once

namespace font {

	#define SFT_DOWNWARD_Y 0x01

	#define SCHRIFT_VERSION "0.10.2"

	#define FILE_MAGIC_ONE             0x00010000
	#define FILE_MAGIC_TWO             0x74727565

	#define HORIZONTAL_KERNING         0x01
	#define MINIMUM_KERNING            0x02
	#define CROSS_STREAM_KERNING       0x04
	#define OVERRIDE_KERNING           0x08

	#define POINT_IS_ON_CURVE          0x01
	#define X_CHANGE_IS_SMALL          0x02
	#define Y_CHANGE_IS_SMALL          0x04
	#define REPEAT_FLAG                0x08
	#define X_CHANGE_IS_ZERO           0x10
	#define X_CHANGE_IS_POSITIVE       0x10
	#define Y_CHANGE_IS_ZERO           0x20
	#define Y_CHANGE_IS_POSITIVE       0x20

	#define OFFSETS_ARE_LARGE          0x001
	#define ACTUAL_XY_OFFSETS          0x002
	#define GOT_A_SINGLE_SCALE         0x008
	#define THERE_ARE_MORE_COMPONENTS  0x020
	#define GOT_AN_X_AND_Y_SCALE       0x040
	#define GOT_A_SCALE_MATRIX         0x080

	#define SIGN(x)   (((x) > 0) - ((x) < 0))

	#define STACK_ALLOC(var, type, thresh, count) \
		type var##_stack_[thresh]; \
		var = (count) <= (thresh) ? var##_stack_ : (type*)calloc(sizeof(type), count);
	#define STACK_FREE(var) \
		if (var != var##_stack_) free(var);

	static inline int fast_floor(double x) {
		int i = (int)x;
		return i - (i > x);
	}

	static inline int fast_ceil(double x) {
		int i = (int)x;
		return i + (i < x);
	}


	struct SFT_Font {
		const uint8_t* memory = nullptr;
		uint_fast32_t size = 0;
		HANDLE mapping = nullptr;
		bool mapped = false;
		uint_least16_t unitsPerEm = 0;
		int_least16_t  locaFormat = 0;
		uint_least16_t numLongHmtx = 0;
	};

	struct SFT {
		SFT_Font* font = nullptr;
		double xScale = 0.0;
		double yScale = 0.0;
		double xOffset = 0.0;
		double yOffset = 0.0;
		int flags = 0;
	};

	struct SFT_LMetrics {
		double ascender = 0.0;
		double descender = 0.0;
		double lineGap = 0.0;
	};

	struct SFT_GMetrics {
		double advanceWidth = 0.0;
		double leftSideBearing = 0.0;
		int yOffset = 0;
		int minWidth = 0;
		int minHeight = 0;
	};

	struct SFT_Kerning {
		double xShift = 0.0;
		double yShift = 0.0;
	};

	struct SFT_Image {
		void* pixels = nullptr;
		int width = 0;
		int height = 0;
	};

	typedef uint_least32_t SFT_UChar; /* Guaranteed to be compatible with char32_t. */
	typedef uint_fast32_t SFT_Glyph;

	struct Point {
		double x = 0.0, y = 0.0;
	};

	static Point midpoint(Point a, Point b) {
		return Point(
			0.5 * (a.x + b.x),
			0.5 * (a.y + b.y)
		);
	}


	struct Line {
		uint_least16_t beg = 0, end = 0;
	};

	struct Curve {
		uint_least16_t beg = 0, end = 0, ctrl = 0;
	};

	struct Cell {
		double area = 0.0, cover = 0.0;
	};

	struct Raster {
		Cell* cells = nullptr;
		int width = 0;
		int height = 0;
	};

	struct Outline {
		std::vector<Point> points;
		std::vector<Curve> curves;
		std::vector<Line> lines;

		Outline() {
			points.reserve(64);
			curves.reserve(64);
			lines.reserve(64);
		}

		/* A heuristic to tell whether a given curve can be approximated closely enough by a line. */
		int is_flat(const Curve& curve) {
			const double maxArea2 = 2.0;
			Point a = points[curve.beg];
			Point b = points[curve.ctrl];
			Point c = points[curve.end];
			Point g = { b.x - a.x, b.y - a.y };
			Point h = { c.x - a.x, c.y - a.y };
			double area2 = fabs(g.x * h.y - h.x * g.y);
			return area2 <= maxArea2;
		}

		int tesselate_curve(Curve curve) {
			/* From my tests I can conclude that this stack barely reaches a top height
			 * of 4 elements even for the largest font sizes I'm willing to support. And
			 * as space requirements should only grow logarithmically, I think 10 is
			 * more than enough. */
			std::array<Curve, 10> stack;
			unsigned int top = 0;
			for (;;) {
				if (is_flat(curve) || top >= stack.size()) {
					lines.emplace_back(curve.beg, curve.end);
					if (top == 0) break;
					curve = stack[--top];
				}
				else {
					uint_least16_t ctrl0 = (uint_least16_t)points.size();
					points.push_back(midpoint(points[curve.beg], points[curve.ctrl]));

					uint_least16_t ctrl1 = (uint_least16_t)points.size();
					points.push_back(midpoint(points[curve.ctrl], points[curve.end]));

					uint_least16_t pivot = (uint_least16_t)points.size();
					points.push_back(midpoint(points[ctrl0], points[ctrl1]));

					stack[top++] = Curve(curve.beg, pivot, ctrl0);
					curve = Curve(pivot, curve.end, ctrl1);
				}
			}
			return 0;
		}

		int tesselate_curves() {
			unsigned int i;
			for (i = 0; i < curves.size(); ++i) {
				if (tesselate_curve(curves[i]) < 0)
					return -1;
			}
			return 0;
		}

		/* Draws a line into the buffer. Uses a custom 2D raycasting algorithm to do so. */
		static void draw_line(Raster buf, Point origin, Point goal) {
			Point delta;
			Point nextCrossing;
			Point crossingIncr;
			double halfDeltaX;
			double prevDistance = 0.0, nextDistance;
			double xAverage, yDifference;
			struct { int x, y; } pixel;
			struct { int x, y; } dir;
			int step, numSteps = 0;
			Cell* cptr, cell;

			delta.x = goal.x - origin.x;
			delta.y = goal.y - origin.y;
			dir.x = SIGN(delta.x);
			dir.y = SIGN(delta.y);

			if (!dir.y) {
				return;
			}

			crossingIncr.x = dir.x ? fabs(1.0 / delta.x) : 1.0;
			crossingIncr.y = fabs(1.0 / delta.y);

			if (!dir.x) {
				pixel.x = fast_floor(origin.x);
				nextCrossing.x = 100.0;
			}
			else {
				if (dir.x > 0) {
					pixel.x = fast_floor(origin.x);
					nextCrossing.x = (origin.x - pixel.x) * crossingIncr.x;
					nextCrossing.x = crossingIncr.x - nextCrossing.x;
					numSteps += fast_ceil(goal.x) - fast_floor(origin.x) - 1;
				}
				else {
					pixel.x = fast_ceil(origin.x) - 1;
					nextCrossing.x = (origin.x - pixel.x) * crossingIncr.x;
					numSteps += fast_ceil(origin.x) - fast_floor(goal.x) - 1;
				}
			}

			if (dir.y > 0) {
				pixel.y = fast_floor(origin.y);
				nextCrossing.y = (origin.y - pixel.y) * crossingIncr.y;
				nextCrossing.y = crossingIncr.y - nextCrossing.y;
				numSteps += fast_ceil(goal.y) - fast_floor(origin.y) - 1;
			}
			else {
				pixel.y = fast_ceil(origin.y) - 1;
				nextCrossing.y = (origin.y - pixel.y) * crossingIncr.y;
				numSteps += fast_ceil(origin.y) - fast_floor(goal.y) - 1;
			}

			nextDistance = std::min(nextCrossing.x, nextCrossing.y);
			halfDeltaX = 0.5 * delta.x;

			for (step = 0; step < numSteps; ++step) {
				xAverage = origin.x + (prevDistance + nextDistance) * halfDeltaX;
				yDifference = (nextDistance - prevDistance) * delta.y;
				cptr = &buf.cells[pixel.y * buf.width + pixel.x];
				cell = *cptr;
				cell.cover += yDifference;
				xAverage -= (double)pixel.x;
				cell.area += (1.0 - xAverage) * yDifference;
				*cptr = cell;
				prevDistance = nextDistance;
				int alongX = nextCrossing.x < nextCrossing.y;
				pixel.x += alongX ? dir.x : 0;
				pixel.y += alongX ? 0 : dir.y;
				nextCrossing.x += alongX ? crossingIncr.x : 0.0;
				nextCrossing.y += alongX ? 0.0 : crossingIncr.y;
				nextDistance = std::min(nextCrossing.x, nextCrossing.y);
			}

			xAverage = origin.x + (prevDistance + 1.0) * halfDeltaX;
			yDifference = (1.0 - prevDistance) * delta.y;
			cptr = &buf.cells[pixel.y * buf.width + pixel.x];
			cell = *cptr;
			cell.cover += yDifference;
			xAverage -= (double)pixel.x;
			cell.area += (1.0 - xAverage) * yDifference;
			*cptr = cell;
		}

		void draw_lines(Raster buf) const {
			unsigned int i;
			for (i = 0; i < lines.size(); ++i) {
				const Line& line = lines[i];
				const Point& origin = points[line.beg];
				const Point& goal = points[line.end];
				draw_line(buf, origin, goal);
			}
		}
	};

	int map_file(SFT_Font* font, const char* filename) {
		HANDLE file;
		DWORD high, low;

		font->mapping = NULL;
		font->memory = NULL;

		file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (file == INVALID_HANDLE_VALUE) {
			return -1;
		}

		low = GetFileSize(file, &high);
		if (low == INVALID_FILE_SIZE) {
			CloseHandle(file);
			return -1;
		}

		font->size = (size_t)high << (8 * sizeof(DWORD)) | low;

		font->mapping = CreateFileMapping(file, NULL, PAGE_READONLY, high, low, NULL);
		if (!font->mapping) {
			CloseHandle(file);
			return -1;
		}

		CloseHandle(file);

		font->memory = (uint8_t*)MapViewOfFile(font->mapping, FILE_MAP_READ, 0, 0, 0);
		if (!font->memory) {
			CloseHandle(font->mapping);
			font->mapping = NULL;
			return -1;
		}

		return 0;
	}

	static void unmap_file(SFT_Font* font) {
		if (font->memory) {
			UnmapViewOfFile(font->memory);
			font->memory = NULL;
		}
		if (font->mapping) {
			CloseHandle(font->mapping);
			font->mapping = NULL;
		}
	}

	static int init_font(SFT_Font* font);

	static void transform_points(unsigned int numPts, Point* points, double trf[6]);
	static void clip_points(unsigned int numPts, Point* points, int width, int height);

	static inline int is_safe_offset(SFT_Font* font, uint_fast32_t offset, uint_fast32_t margin);
	static void* csearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
	static int cmpu16(const void* a, const void* b);
	static int cmpu32(const void* a, const void* b);
	static inline uint_least8_t getu8(SFT_Font* font, uint_fast32_t offset);
	static inline int_least8_t geti8(SFT_Font* font, uint_fast32_t offset);
	static inline uint_least16_t getu16(SFT_Font* font, uint_fast32_t offset);
	static inline int_least16_t geti16(SFT_Font* font, uint_fast32_t offset);
	static inline uint_least32_t getu32(SFT_Font* font, uint_fast32_t offset);
	static int gettable(SFT_Font* font, char tag[4], uint_fast32_t* offset);

	static int cmap_fmt4(SFT_Font* font, uint_fast32_t table, SFT_UChar charCode, uint_fast32_t* glyph);
	static int cmap_fmt6(SFT_Font* font, uint_fast32_t table, SFT_UChar charCode, uint_fast32_t* glyph);
	static int glyph_id(SFT_Font* font, SFT_UChar charCode, uint_fast32_t* glyph);

	static int hor_metrics(SFT_Font* font, uint_fast32_t glyph, int* advanceWidth, int* leftSideBearing);
	static int glyph_bbox(const SFT* sft, uint_fast32_t outline, int box[4]);

	static int outline_offset(SFT_Font* font, uint_fast32_t glyph, uint_fast32_t* offset);
	static int simple_flags(SFT_Font* font, uint_fast32_t* offset, uint_fast16_t numPts, uint8_t* flags);
	static int simple_points(SFT_Font* font, uint_fast32_t offset, uint_fast16_t numPts, uint8_t* flags, Point* points);
	static int decode_contour(uint8_t* flags, uint_fast16_t basePoint, uint_fast16_t count, Outline* outl);
	static int simple_outline(SFT_Font* font, uint_fast32_t offset, unsigned int numContours, Outline* outl);
	static int compound_outline(SFT_Font* font, uint_fast32_t offset, int recDepth, Outline* outl);
	static int decode_outline(SFT_Font* font, uint_fast32_t offset, int recDepth, Outline* outl);

	static void post_process(Raster buf, uint8_t* image);

	static int render_outline(Outline* outl, double transform[6], SFT_Image image);

	const char* sft_version(void) { return SCHRIFT_VERSION; }

	void sft_freefont(SFT_Font* font) {
		if (!font) return;
		if (font->mapped)
			unmap_file(font);
		free(font);
	}

	SFT_Font* sft_loadmem(const void* mem, size_t size) {
		SFT_Font* font;
		if (size > UINT32_MAX) {
			return NULL;
		}
		if (!(font = (SFT_Font*)calloc(1, sizeof * font))) {
			return NULL;
		}
		font->memory = (uint8_t*)mem;
		font->size = (uint_fast32_t)size;
		font->mapped = false;
		if (init_font(font) < 0) {
			sft_freefont(font);
			return NULL;
		}
		return font;
	}

	SFT_Font * sft_loadfile(char const* filename) {
		SFT_Font* font;
		if (!(font = (SFT_Font*)calloc(1, sizeof * font))) {
			return NULL;
		}
		if (map_file(font, filename) < 0) {
			free(font);
			return NULL;
		}
		if (init_font(font) < 0) {
			sft_freefont(font);
			return NULL;
		}
		return font;
	}

	int sft_lmetrics(const SFT* sft, SFT_LMetrics* metrics) {
		double factor;
		uint_fast32_t hhea;
		memset(metrics, 0, sizeof * metrics);
		if (gettable(sft->font, (char*)"hhea", &hhea) < 0)
			return -1;
		if (!is_safe_offset(sft->font, hhea, 36))
			return -1;
		factor = sft->yScale / sft->font->unitsPerEm;
		metrics->ascender = geti16(sft->font, hhea + 4) * factor;
		metrics->descender = geti16(sft->font, hhea + 6) * factor;
		metrics->lineGap = geti16(sft->font, hhea + 8) * factor;
		return 0;
	}

	int sft_lookup(const SFT* sft, SFT_UChar codepoint, SFT_Glyph* glyph) {
		return glyph_id(sft->font, codepoint, glyph);
	}

	int sft_gmetrics(const SFT* sft, SFT_Glyph glyph, SFT_GMetrics* metrics) {
		int adv, lsb;
		double xScale = sft->xScale / sft->font->unitsPerEm;
		uint_fast32_t outline;
		int bbox[4];

		memset(metrics, 0, sizeof * metrics);

		if (hor_metrics(sft->font, glyph, &adv, &lsb) < 0)
			return -1;
		metrics->advanceWidth = adv * xScale;
		metrics->leftSideBearing = lsb * xScale + sft->xOffset;

		if (outline_offset(sft->font, glyph, &outline) < 0)
			return -1;
		if (!outline)
			return 0;
		if (glyph_bbox(sft, outline, bbox) < 0)
			return -1;
		metrics->minWidth = bbox[2] - bbox[0] + 1;
		metrics->minHeight = bbox[3] - bbox[1] + 1;
		metrics->yOffset = sft->flags & SFT_DOWNWARD_Y ? -bbox[3] : bbox[1];

		return 0;
	}

	int sft_kerning(const SFT* sft, SFT_Glyph leftGlyph, SFT_Glyph rightGlyph, SFT_Kerning* kerning) {
		void* match;
		uint_fast32_t offset;
		unsigned int numTables, numPairs, length, format, flags;
		int value;
		uint8_t key[4];

		memset(kerning, 0, sizeof * kerning);

		if (gettable(sft->font, (char*)"kern", &offset) < 0)
			return 0;

		/* Read kern table header. */
		if (!is_safe_offset(sft->font, offset, 4))
			return -1;
		if (getu16(sft->font, offset) != 0)
			return 0;
		numTables = getu16(sft->font, offset + 2);
		offset += 4;

		while (numTables > 0) {
			/* Read subtable header. */
			if (!is_safe_offset(sft->font, offset, 6))
				return -1;
			length = getu16(sft->font, offset + 2);
			format = getu8(sft->font, offset + 4);
			flags = getu8(sft->font, offset + 5);
			offset += 6;

			if (format == 0 && (flags & HORIZONTAL_KERNING) && !(flags & MINIMUM_KERNING)) {
				/* Read format 0 header. */
				if (!is_safe_offset(sft->font, offset, 8))
					return -1;
				numPairs = getu16(sft->font, offset);
				offset += 8;
				/* Look up character code pair via binary search. */
				key[0] = (leftGlyph >> 8) & 0xFF;
				key[1] = leftGlyph & 0xFF;
				key[2] = (rightGlyph >> 8) & 0xFF;
				key[3] = rightGlyph & 0xFF;
				if ((match = bsearch(key, sft->font->memory + offset,
					numPairs, 6, cmpu32)) != NULL) {

					value = geti16(sft->font, (uint_fast32_t)((uint8_t*)match - sft->font->memory + 4));
					if (flags & CROSS_STREAM_KERNING) {
						kerning->yShift += value;
					}
					else {
						kerning->xShift += value;
					}
				}

			}

			offset += length;
			--numTables;
		}

		kerning->xShift = kerning->xShift / sft->font->unitsPerEm * sft->xScale;
		kerning->yShift = kerning->yShift / sft->font->unitsPerEm * sft->yScale;

		return 0;
	}

	int sft_render(const SFT* sft, SFT_Glyph glyph, SFT_Image image) {
		uint_fast32_t outline;
		double transform[6];
		int bbox[4];

		if (outline_offset(sft->font, glyph, &outline) < 0)
			return -1;
		if (!outline)
			return 0;
		if (glyph_bbox(sft, outline, bbox) < 0)
			return -1;
		/* Set up the transformation matrix such that
		 * the transformed bounding boxes min corner lines
		 * up with the (0, 0) point. */
		transform[0] = sft->xScale / sft->font->unitsPerEm;
		transform[1] = 0.0;
		transform[2] = 0.0;
		transform[4] = sft->xOffset - bbox[0];
		if (sft->flags & SFT_DOWNWARD_Y) {
			transform[3] = -sft->yScale / sft->font->unitsPerEm;
			transform[5] = bbox[3] - sft->yOffset;
		}
		else {
			transform[3] = +sft->yScale / sft->font->unitsPerEm;
			transform[5] = sft->yOffset - bbox[1];
		}

		Outline outl;
		if (decode_outline(sft->font, outline, 0, &outl) < 0)
			return -1;
		if (render_outline(&outl, transform, image) < 0)
			return -1;

		return 0;
	}

	static int init_font(SFT_Font* font) {
		uint_fast32_t scalerType, head, hhea;

		if (!is_safe_offset(font, 0, 12))
			return -1;
		/* Check for a compatible scalerType (magic number). */
		scalerType = getu32(font, 0);
		if (scalerType != FILE_MAGIC_ONE && scalerType != FILE_MAGIC_TWO)
			return -1;

		if (gettable(font, (char*)"head", &head) < 0)
			return -1;
		if (!is_safe_offset(font, head, 54))
			return -1;
		font->unitsPerEm = getu16(font, head + 18);
		font->locaFormat = geti16(font, head + 50);

		if (gettable(font, (char*)"hhea", &hhea) < 0)
			return -1;
		if (!is_safe_offset(font, hhea, 36))
			return -1;
		font->numLongHmtx = getu16(font, hhea + 34);

		return 0;
	}

	static void transform_points(unsigned int numPts, Point* points, double trf[6]) {
		Point pt;
		unsigned int i;
		for (i = 0; i < numPts; ++i) {
			pt = points[i];
			points[i] = Point(
				pt.x * trf[0] + pt.y * trf[2] + trf[4],
				pt.x * trf[1] + pt.y * trf[3] + trf[5]
			);
		}
	}

	static void clip_points(unsigned int numPts, Point* points, int width, int height) {
		Point pt;
		unsigned int i;

		for (i = 0; i < numPts; ++i) {
			pt = points[i];

			if (pt.x < 0.0) {
				points[i].x = 0.0;
			}
			if (pt.x >= width) {
				points[i].x = nextafter(width, 0.0);
			}
			if (pt.y < 0.0) {
				points[i].y = 0.0;
			}
			if (pt.y >= height) {
				points[i].y = nextafter(height, 0.0);
			}
		}
	}

	static inline int is_safe_offset(SFT_Font* font, uint_fast32_t offset, uint_fast32_t margin) {
		if (offset > font->size) return 0;
		if (font->size - offset < margin) return 0;
		return 1;
	}

	/* Like bsearch(), but returns the next highest element if key could not be found. */
	static void* csearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
		const uint8_t* bytes = (uint8_t*)base, * sample;
		size_t low = 0, high = nmemb - 1, mid;
		if (!nmemb) return NULL;
		while (low != high) {
			mid = low + (high - low) / 2;
			sample = bytes + mid * size;
			if (compar(key, sample) > 0) {
				low = mid + 1;
			}
			else {
				high = mid;
			}
		}
		return (uint8_t*)bytes + low * size;
	}

	static int cmpu16(const void* a, const void* b) {
		return memcmp(a, b, 2);
	}

	static int cmpu32(const void* a, const void* b) {
		return memcmp(a, b, 4);
	}

	static inline uint_least8_t getu8(SFT_Font* font, uint_fast32_t offset) {
		assert(offset + 1 <= font->size);
		return *(font->memory + offset);
	}

	static inline int_least8_t geti8(SFT_Font* font, uint_fast32_t offset) {
		return (int_least8_t)getu8(font, offset);
	}

	static inline uint_least16_t getu16(SFT_Font* font, uint_fast32_t offset) {
		assert(offset + 2 <= font->size);
		const uint8_t* base = font->memory + offset;
		uint_least16_t b1 = base[0], b0 = base[1];
		return (uint_least16_t)(b1 << 8 | b0);
	}

	static inline int16_t geti16(SFT_Font* font, uint_fast32_t offset) {
		return (int_least16_t)getu16(font, offset);
	}

	static inline uint32_t getu32(SFT_Font* font, uint_fast32_t offset) {
		assert(offset + 4 <= font->size);
		const uint8_t* base = font->memory + offset;
		uint_least32_t b3 = base[0], b2 = base[1], b1 = base[2], b0 = base[3];
		return (uint_least32_t)(b3 << 24 | b2 << 16 | b1 << 8 | b0);
	}

	static int gettable(SFT_Font* font, char tag[4], uint_fast32_t* offset) {
		void* match;
		unsigned int numTables;
		/* No need to bounds-check access to the first 12 bytes - this gets already checked by init_font(). */
		numTables = getu16(font, 4);
		if (!is_safe_offset(font, 12, (uint_fast32_t)numTables * 16))
			return -1;
		if (!(match = bsearch(tag, font->memory + 12, numTables, 16, cmpu32)))
			return -1;
		*offset = getu32(font, (uint_fast32_t)((uint8_t*)match - font->memory + 8));
		return 0;
	}

	static int cmap_fmt4(SFT_Font* font, uint_fast32_t table, SFT_UChar charCode, SFT_Glyph* glyph) {
		const uint8_t* segPtr;
		uint_fast32_t segIdxX2;
		uint_fast32_t endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
		uint_fast16_t segCountX2, idRangeOffset, startCode, shortCode, idDelta, id;
		uint8_t key[2] = { (uint8_t)(charCode >> 8), (uint8_t)charCode };
		/* cmap format 4 only supports the Unicode BMP. */
		if (charCode > 0xFFFF) {
			*glyph = 0;
			return 0;
		}
		shortCode = (uint_fast16_t)charCode;
		if (!is_safe_offset(font, table, 8))
			return -1;
		segCountX2 = getu16(font, table);
		if ((segCountX2 & 1) || !segCountX2)
			return -1;
		/* Find starting positions of the relevant arrays. */
		endCodes = table + 8;
		startCodes = endCodes + segCountX2 + 2;
		idDeltas = startCodes + segCountX2;
		idRangeOffsets = idDeltas + segCountX2;
		if (!is_safe_offset(font, idRangeOffsets, segCountX2))
			return -1;
		/* Find the segment that contains shortCode by binary searching over
		 * the highest codes in the segments. */
		segPtr = (uint8_t*)csearch(key, font->memory + endCodes, segCountX2 / 2, 2, cmpu16);
		segIdxX2 = (uint_fast32_t)(segPtr - (font->memory + endCodes));
		/* Look up segment info from the arrays & short circuit if the spec requires. */
		if ((startCode = getu16(font, startCodes + segIdxX2)) > shortCode)
			return 0;
		idDelta = getu16(font, idDeltas + segIdxX2);
		if (!(idRangeOffset = getu16(font, idRangeOffsets + segIdxX2))) {
			/* Intentional integer under- and overflow. */
			*glyph = (shortCode + idDelta) & 0xFFFF;
			return 0;
		}
		/* Calculate offset into glyph array and determine ultimate value. */
		idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2U * (unsigned int)(shortCode - startCode);
		if (!is_safe_offset(font, idOffset, 2))
			return -1;
		id = getu16(font, idOffset);
		/* Intentional integer under- and overflow. */
		*glyph = id ? (id + idDelta) & 0xFFFF : 0;
		return 0;
	}

	static int cmap_fmt6(SFT_Font* font, uint_fast32_t table, SFT_UChar charCode, SFT_Glyph* glyph) {
		unsigned int firstCode, entryCount;
		/* cmap format 6 only supports the Unicode BMP. */
		if (charCode > 0xFFFF) {
			*glyph = 0;
			return 0;
		}
		if (!is_safe_offset(font, table, 4))
			return -1;
		firstCode = getu16(font, table);
		entryCount = getu16(font, table + 2);
		if (!is_safe_offset(font, table, 4 + 2 * entryCount))
			return -1;
		if (charCode < firstCode)
			return -1;
		charCode -= firstCode;
		if (!(charCode < entryCount))
			return -1;
		*glyph = getu16(font, table + 4 + 2 * charCode);
		return 0;
	}

	static int cmap_fmt12_13(SFT_Font* font, uint_fast32_t table, SFT_UChar charCode, SFT_Glyph* glyph, int which) {
		uint32_t len, numEntries;
		uint_fast32_t i;

		*glyph = 0;

		/* check that the entire header is present */
		if (!is_safe_offset(font, table, 16))
			return -1;

		len = getu32(font, table + 4);

		/* A minimal header is 16 bytes */
		if (len < 16)
			return -1;

		if (!is_safe_offset(font, table, len))
			return -1;

		numEntries = getu32(font, table + 12);

		for (i = 0; i < numEntries; ++i) {
			uint32_t firstCode, lastCode, glyphOffset;
			firstCode = getu32(font, table + (i * 12) + 16);
			lastCode = getu32(font, table + (i * 12) + 16 + 4);
			if (charCode < firstCode || charCode > lastCode)
				continue;
			glyphOffset = getu32(font, table + (i * 12) + 16 + 8);
			if (which == 12)
				*glyph = (charCode - firstCode) + glyphOffset;
			else
				*glyph = glyphOffset;
			return 0;
		}

		return 0;
	}

	/* Maps Unicode code points to glyph indices. */
	static int glyph_id(SFT_Font* font, SFT_UChar charCode, SFT_Glyph* glyph) {
		uint_fast32_t cmap, entry, table;
		unsigned int idx, numEntries;
		int type, format;

		*glyph = 0;

		if (gettable(font, (char*)"cmap", &cmap) < 0)
			return -1;

		if (!is_safe_offset(font, cmap, 4))
			return -1;
		numEntries = getu16(font, cmap + 2);

		if (!is_safe_offset(font, cmap, 4 + numEntries * 8))
			return -1;

		/* First look for a 'full repertoire'/non-BMP map. */
		for (idx = 0; idx < numEntries; ++idx) {
			entry = cmap + 4 + idx * 8;
			type = getu16(font, entry) * 0100 + getu16(font, entry + 2);
			/* Complete unicode map */
			if (type == 0004 || type == 0312) {
				table = cmap + getu32(font, entry + 4);
				if (!is_safe_offset(font, table, 8))
					return -1;
				/* Dispatch based on cmap format. */
				format = getu16(font, table);
				switch (format) {
				case 12:
					return cmap_fmt12_13(font, table, charCode, glyph, 12);
				default:
					return -1;
				}
			}
		}

		/* If no 'full repertoire' cmap was found, try looking for a BMP map. */
		for (idx = 0; idx < numEntries; ++idx) {
			entry = cmap + 4 + idx * 8;
			type = getu16(font, entry) * 0100 + getu16(font, entry + 2);
			/* Unicode BMP */
			if (type == 0003 || type == 0301) {
				table = cmap + getu32(font, entry + 4);
				if (!is_safe_offset(font, table, 6))
					return -1;
				/* Dispatch based on cmap format. */
				switch (getu16(font, table)) {
				case 4:
					return cmap_fmt4(font, table + 6, charCode, glyph);
				case 6:
					return cmap_fmt6(font, table + 6, charCode, glyph);
				default:
					return -1;
				}
			}
		}

		return -1;
	}

	static int hor_metrics(SFT_Font* font, SFT_Glyph glyph, int* advanceWidth, int* leftSideBearing) {
		uint_fast32_t hmtx, offset, boundary;
		if (gettable(font, (char*)"hmtx", &hmtx) < 0)
			return -1;
		if (glyph < font->numLongHmtx) {
			/* glyph is inside long metrics segment. */
			offset = hmtx + 4 * glyph;
			if (!is_safe_offset(font, offset, 4))
				return -1;
			*advanceWidth = getu16(font, offset);
			*leftSideBearing = geti16(font, offset + 2);
			return 0;
		}
		else {
			/* glyph is inside short metrics segment. */
			boundary = hmtx + 4U * (uint_fast32_t)font->numLongHmtx;
			if (boundary < 4)
				return -1;

			offset = boundary - 4;
			if (!is_safe_offset(font, offset, 4))
				return -1;
			*advanceWidth = getu16(font, offset);

			offset = boundary + 2 * (glyph - font->numLongHmtx);
			if (!is_safe_offset(font, offset, 2))
				return -1;
			*leftSideBearing = geti16(font, offset);
			return 0;
		}
	}

	static int glyph_bbox(const SFT* sft, uint_fast32_t outline, int box[4]) {
		double xScale, yScale;
		/* Read the bounding box from the font file verbatim. */
		if (!is_safe_offset(sft->font, outline, 10))
			return -1;
		box[0] = geti16(sft->font, outline + 2);
		box[1] = geti16(sft->font, outline + 4);
		box[2] = geti16(sft->font, outline + 6);
		box[3] = geti16(sft->font, outline + 8);
		if (box[2] <= box[0] || box[3] <= box[1])
			return -1;
		/* Transform the bounding box into SFT coordinate space. */
		xScale = sft->xScale / sft->font->unitsPerEm;
		yScale = sft->yScale / sft->font->unitsPerEm;
		box[0] = (int)floor(box[0] * xScale + sft->xOffset);
		box[1] = (int)floor(box[1] * yScale + sft->yOffset);
		box[2] = (int)ceil(box[2] * xScale + sft->xOffset);
		box[3] = (int)ceil(box[3] * yScale + sft->yOffset);
		return 0;
	}

	/* Returns the offset into the font that the glyph's outline is stored at. */
	static int outline_offset(SFT_Font* font, SFT_Glyph glyph, uint_fast32_t* offset) {
		uint_fast32_t loca, glyf;
		uint_fast32_t base, current, next;

		if (gettable(font, (char*)"loca", &loca) < 0)
			return -1;
		if (gettable(font, (char*)"glyf", &glyf) < 0)
			return -1;

		if (font->locaFormat == 0) {
			base = loca + 2 * glyph;

			if (!is_safe_offset(font, base, 4))
				return -1;

			current = 2U * (uint_fast32_t)getu16(font, base);
			next = 2U * (uint_fast32_t)getu16(font, base + 2);
		}
		else {
			base = loca + 4 * glyph;

			if (!is_safe_offset(font, base, 8))
				return -1;

			current = getu32(font, base);
			next = getu32(font, base + 4);
		}

		*offset = current == next ? 0 : glyf + current;
		return 0;
	}

	/* For a 'simple' outline, determines each point of the outline with a set of flags. */
	static int simple_flags(SFT_Font* font, uint_fast32_t* offset, uint_fast16_t numPts, uint8_t* flags) {
		uint_fast32_t off = *offset;
		uint_fast16_t i;
		uint8_t value = 0, repeat = 0;
		for (i = 0; i < numPts; ++i) {
			if (repeat) {
				--repeat;
			}
			else {
				if (!is_safe_offset(font, off, 1))
					return -1;
				value = getu8(font, off++);
				if (value & REPEAT_FLAG) {
					if (!is_safe_offset(font, off, 1))
						return -1;
					repeat = getu8(font, off++);
				}
			}
			flags[i] = value;
		}
		*offset = off;
		return 0;
	}

	/* For a 'simple' outline, decodes both X and Y coordinates for each point of the outline. */
	static int simple_points(SFT_Font* font, uint_fast32_t offset, uint_fast16_t numPts, uint8_t* flags, Point* points) {
		long accum, value, bit;
		uint_fast16_t i;

		accum = 0L;
		for (i = 0; i < numPts; ++i) {
			if (flags[i] & X_CHANGE_IS_SMALL) {
				if (!is_safe_offset(font, offset, 1))
					return -1;
				value = (long)getu8(font, offset++);
				bit = !!(flags[i] & X_CHANGE_IS_POSITIVE);
				accum -= (value ^ -bit) + bit;
			}
			else if (!(flags[i] & X_CHANGE_IS_ZERO)) {
				if (!is_safe_offset(font, offset, 2))
					return -1;
				accum += geti16(font, offset);
				offset += 2;
			}
			points[i].x = (double)accum;
		}

		accum = 0L;
		for (i = 0; i < numPts; ++i) {
			if (flags[i] & Y_CHANGE_IS_SMALL) {
				if (!is_safe_offset(font, offset, 1))
					return -1;
				value = (long)getu8(font, offset++);
				bit = !!(flags[i] & Y_CHANGE_IS_POSITIVE);
				accum -= (value ^ -bit) + bit;
			}
			else if (!(flags[i] & Y_CHANGE_IS_ZERO)) {
				if (!is_safe_offset(font, offset, 2))
					return -1;
				accum += geti16(font, offset);
				offset += 2;
			}
			points[i].y = (double)accum;
		}

		return 0;
	}

	static int decode_contour(uint8_t* flags, uint_fast16_t basePoint, uint_fast16_t count, Outline* outl) {
		uint_fast16_t i;
		uint_least16_t looseEnd, beg, ctrl, center, cur;
		unsigned int gotCtrl;

		/* Skip contours with less than two points, since the following algorithm can't handle them and
		 * they should appear invisible either way (because they don't have any area). */
		if (count < 2) return 0;

		assert(basePoint <= UINT16_MAX - count);

		if (flags[0] & POINT_IS_ON_CURVE) {
			looseEnd = (uint_least16_t)basePoint++;
			++flags;
			--count;
		}
		else if (flags[count - 1] & POINT_IS_ON_CURVE) {
			looseEnd = (uint_least16_t)(basePoint + --count);
		}
		else {
			looseEnd = (uint_least16_t)outl->points.size();
			outl->points.push_back(midpoint(outl->points[basePoint], outl->points[basePoint + count - 1]));
		}
		beg = looseEnd;
		gotCtrl = 0;
		for (i = 0; i < count; ++i) {
			/* cur can't overflow because we ensure that basePoint + count < 0xFFFF before calling decode_contour(). */
			cur = (uint_least16_t)(basePoint + i);
			/* NOTE clang-analyzer will often flag this and another piece of code because it thinks that flags and
			 * outl->points + basePoint don't always get properly initialized -- even when you explicitly loop over both
			 * and set every element to zero (but not when you use memset). This is a known clang-analyzer bug:
			 * http://clang-developers.42468.n3.nabble.com/StaticAnalyzer-False-positive-with-loop-handling-td4053875.html */
			if (flags[i] & POINT_IS_ON_CURVE) {
				if (gotCtrl) {
					outl->curves.emplace_back(beg, cur, ctrl);
				}
				else {
					outl->lines.emplace_back(beg, cur);
				}
				beg = cur;
				gotCtrl = 0;
			}
			else {
				if (gotCtrl) {
					center = (uint_least16_t)outl->points.size();
					outl->points.push_back(midpoint(outl->points[ctrl], outl->points[cur]));
					outl->curves.emplace_back(beg, center, ctrl);
					beg = center;
				}
				ctrl = cur;
				gotCtrl = 1;
			}
		}
		if (gotCtrl) {
			outl->curves.emplace_back(beg, looseEnd, ctrl);
		}
		else {
			outl->lines.emplace_back(beg, looseEnd);
		}

		return 0;
	}

	static int simple_outline(SFT_Font* font, uint_fast32_t offset, unsigned int numContours, Outline* outl) {
		uint_fast16_t* endPts = NULL;
		uint8_t* flags = NULL;
		uint_fast16_t numPts;
		unsigned int i;

		assert(numContours > 0);

		uint_fast16_t basePoint = (uint_fast16_t)outl->points.size();
		uint_fast16_t beg = 0;

		if (!is_safe_offset(font, offset, numContours * 2 + 2))
			goto failure;
		numPts = getu16(font, offset + (numContours - 1) * 2);
		if (numPts >= UINT16_MAX)
			goto failure;
		numPts++;
		if (outl->points.size() > UINT16_MAX - numPts)
			goto failure;
		outl->points.resize(basePoint + numPts);

		STACK_ALLOC(endPts, uint_fast16_t, 16, numContours);
		if (endPts == NULL)
			goto failure;
		STACK_ALLOC(flags, uint8_t, 128, numPts);
		if (flags == NULL)
			goto failure;

		for (i = 0; i < numContours; ++i) {
			endPts[i] = getu16(font, offset);
			offset += 2;
		}
		/* Ensure that endPts are never falling.
		 * Falling endPts have no sensible interpretation and most likely only occur in malicious input.
		 * Therefore, we bail, should we ever encounter such input. */
		for (i = 0; i < numContours - 1; ++i) {
			if (endPts[i + 1] < endPts[i] + 1)
				goto failure;
		}
		offset += 2U + getu16(font, offset);

		if (simple_flags(font, &offset, numPts, flags) < 0)
			goto failure;
		if (simple_points(font, offset, numPts, flags, outl->points.data() + basePoint) < 0)
			goto failure;

		for (i = 0; i < numContours; ++i) {
			uint_fast16_t count = endPts[i] - beg + 1;
			if (decode_contour(flags + beg, basePoint + beg, count, outl) < 0)
				goto failure;
			beg = endPts[i] + 1;
		}

		STACK_FREE(endPts);
		STACK_FREE(flags);
		return 0;
	failure:
		STACK_FREE(endPts);
		STACK_FREE(flags);
		return -1;
	}

	static int compound_outline(SFT_Font* font, uint_fast32_t offset, int recDepth, Outline* outl) {
		double local[6];
		uint_fast32_t outline;
		unsigned int flags, glyph, basePoint;
		/* Guard against infinite recursion (compound glyphs that have themselves as component). */
		if (recDepth >= 4)
			return -1;
		do {
			memset(local, 0, sizeof local);
			if (!is_safe_offset(font, offset, 4))
				return -1;
			flags = getu16(font, offset);
			glyph = getu16(font, offset + 2);
			offset += 4;
			/* We don't implement point matching, and neither does stb_truetype for that matter. */
			if (!(flags & ACTUAL_XY_OFFSETS))
				return -1;
			/* Read additional X and Y offsets (in FUnits) of this component. */
			if (flags & OFFSETS_ARE_LARGE) {
				if (!is_safe_offset(font, offset, 4))
					return -1;
				local[4] = geti16(font, offset);
				local[5] = geti16(font, offset + 2);
				offset += 4;
			}
			else {
				if (!is_safe_offset(font, offset, 2))
					return -1;
				local[4] = geti8(font, offset);
				local[5] = geti8(font, offset + 1);
				offset += 2;
			}
			if (flags & GOT_A_SINGLE_SCALE) {
				if (!is_safe_offset(font, offset, 2))
					return -1;
				local[0] = geti16(font, offset) / 16384.0;
				local[3] = local[0];
				offset += 2;
			}
			else if (flags & GOT_AN_X_AND_Y_SCALE) {
				if (!is_safe_offset(font, offset, 4))
					return -1;
				local[0] = geti16(font, offset + 0) / 16384.0;
				local[3] = geti16(font, offset + 2) / 16384.0;
				offset += 4;
			}
			else if (flags & GOT_A_SCALE_MATRIX) {
				if (!is_safe_offset(font, offset, 8))
					return -1;
				local[0] = geti16(font, offset + 0) / 16384.0;
				local[1] = geti16(font, offset + 2) / 16384.0;
				local[2] = geti16(font, offset + 4) / 16384.0;
				local[3] = geti16(font, offset + 6) / 16384.0;
				offset += 8;
			}
			else {
				local[0] = 1.0;
				local[3] = 1.0;
			}
			/* At this point, Apple's spec more or less tells you to scale the matrix by its own L1 norm.
			 * But stb_truetype scales by the L2 norm. And FreeType2 doesn't scale at all.
			 * Furthermore, Microsoft's spec doesn't even mention anything like this.
			 * It's almost as if nobody ever uses this feature anyway. */
			if (outline_offset(font, glyph, &outline) < 0)
				return -1;
			if (outline) {
				basePoint = (unsigned)outl->points.size();
				if (decode_outline(font, outline, recDepth + 1, outl) < 0)
					return -1;
				transform_points((unsigned)outl->points.size() - basePoint, outl->points.data() + basePoint, local);
			}
		} while (flags & THERE_ARE_MORE_COMPONENTS);

		return 0;
	}

	static int decode_outline(SFT_Font* font, uint_fast32_t offset, int recDepth, Outline* outl) {
		int numContours;
		if (!is_safe_offset(font, offset, 10))
			return -1;
		numContours = geti16(font, offset);
		if (numContours > 0) {
			/* Glyph has a 'simple' outline consisting of a number of contours. */
			return simple_outline(font, offset + 10, (unsigned int)numContours, outl);
		}
		else if (numContours < 0) {
			/* Glyph has a compound outline combined from mutiple other outlines. */
			return compound_outline(font, offset + 10, recDepth, outl);
		}
		else {
			return 0;
		}
	}

	/* Integrate the values in the buffer to arrive at the final grayscale image. */
	static void post_process(Raster buf, uint8_t* image) {
		Cell cell;
		double accum = 0.0, value;
		unsigned int i, num;
		num = (unsigned int)buf.width * (unsigned int)buf.height;
		for (i = 0; i < num; ++i) {
			cell = buf.cells[i];
			value = fabs(accum + cell.area);
			value = std::min(value, 1.0);
			value = value * 255.0 + 0.5;
			image[i] = (uint8_t)value;
			accum += cell.cover;
		}
	}

	static int render_outline(Outline* outl, double transform[6], SFT_Image image) {
		Cell* cells = NULL;
		Raster buf;
		unsigned int numPixels;

		numPixels = (unsigned int)image.width * (unsigned int)image.height;

		STACK_ALLOC(cells, Cell, 128 * 128, numPixels);
		if (!cells) {
			return -1;
		}
		memset(cells, 0, numPixels * sizeof * cells);
		buf.cells = cells;
		buf.width = image.width;
		buf.height = image.height;

		transform_points((unsigned)outl->points.size(), outl->points.data(), transform);

		clip_points((unsigned)outl->points.size(), outl->points.data(), image.width, image.height);

		if (outl->tesselate_curves() < 0) {
			STACK_FREE(cells);
			return -1;
		}

		outl->draw_lines(buf);

		post_process(buf, (uint8_t*)image.pixels);

		STACK_FREE(cells);
		return 0;
	}

}
