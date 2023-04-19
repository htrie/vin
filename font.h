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

namespace font { // TODO remove namespace

	#define DOWNWARD_Y 0x01

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

	static inline int fast_floor(double x) {
		int i = (int)x;
		return i - (i > x);
	}

	static inline int fast_ceil(double x) {
		int i = (int)x;
		return i + (i < x);
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


	struct Point {
		double x = 0.0, y = 0.0;
	};

	static Point midpoint(Point a, Point b) {
		return Point(
			0.5 * (a.x + b.x),
			0.5 * (a.y + b.y)
		);
	}

	static void transform_points(unsigned int numPts, Point* points, double trf[6]) {
		Point pt;
		for (unsigned i = 0; i < numPts; ++i) {
			pt = points[i];
			points[i] = Point(
				pt.x * trf[0] + pt.y * trf[2] + trf[4],
				pt.x * trf[1] + pt.y * trf[3] + trf[5]
			);
		}
	}

	static void clip_points(unsigned int numPts, Point* points, int width, int height) {
		Point pt;
		for (unsigned i = 0; i < numPts; ++i) {
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

	/* Integrate the values in the buffer to arrive at the final grayscale image. */
	static void post_process(const Raster& buf, uint8_t* image) {
		Cell cell;
		double accum = 0.0, value;
		unsigned int num = (unsigned int)buf.width * (unsigned int)buf.height;
		for (unsigned i = 0; i < num; ++i) {
			cell = buf.cells[i];
			value = fabs(accum + cell.area);
			value = std::min(value, 1.0);
			value = value * 255.0 + 0.5;
			image[i] = (uint8_t)value;
			accum += cell.cover;
		}
	}

	/* Draws a line into the buffer. Uses a custom 2D raycasting algorithm to do so. */
	static void draw_line(Raster& buf, const Point& origin, const Point& goal) {
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


	typedef uint_least32_t UChar; /* Guaranteed to be compatible with char32_t. */
	typedef uint_fast32_t uint_fast32_t;

	struct Font { // TODO make class
		const uint8_t* memory = nullptr;
		uint_fast32_t size = 0;
		HANDLE mapping = nullptr;
		bool mapped = false;
		uint_least16_t unitsPerEm = 0;
		int_least16_t  locaFormat = 0;
		uint_least16_t numLongHmtx = 0;

		uint_least8_t getu8(uint_fast32_t offset) const {
			assert(offset + 1 <= size);
			return *(memory + offset);
		}

		int_least8_t geti8(uint_fast32_t offset) const {
			return (int_least8_t)getu8(offset);
		}

		uint_least16_t getu16(uint_fast32_t offset) const {
			assert(offset + 2 <= size);
			const uint8_t* base = memory + offset;
			uint_least16_t b1 = base[0], b0 = base[1];
			return (uint_least16_t)(b1 << 8 | b0);
		}

		int16_t geti16(uint_fast32_t offset) const {
			return (int_least16_t)getu16(offset);
		}

		uint32_t getu32(uint_fast32_t offset) const {
			assert(offset + 4 <= size);
			const uint8_t* base = memory + offset;
			uint_least32_t b3 = base[0], b2 = base[1], b1 = base[2], b0 = base[3];
			return (uint_least32_t)(b3 << 24 | b2 << 16 | b1 << 8 | b0);
		}

		int is_safe_offset(uint_fast32_t offset, uint_fast32_t margin) const {
			if (offset > size) return 0;
			if (size - offset < margin) return 0;
			return 1;
		}

		int gettable(char tag[4], uint_fast32_t* offset) const {
			void* match;
			unsigned int numTables;
			/* No need to bounds-check access to the first 12 bytes - this gets already checked by init_font(). */
			numTables = getu16(4);
			if (!is_safe_offset(12, (uint_fast32_t)numTables * 16))
				return -1;
			if (!(match = bsearch(tag, memory + 12, numTables, 16, cmpu32)))
				return -1;
			*offset = getu32((uint_fast32_t)((uint8_t*)match - memory + 8));
			return 0;
		}

		Font() {}

		Font(const void* mem, size_t size) {
			if (size > UINT32_MAX) {
				return;
			}
			memory = (uint8_t*)mem;
			size = (uint_fast32_t)size;
			mapped = false;
			init();
		}

		Font(const std::string_view filename) {
			if (map_file(filename) < 0) {
				return;
			}
			init();
		}

		~Font() {
			if (mapped)
				unmap_file();
		}

		bool is_valid() const { return memory != nullptr; }

		int init() {
			uint_fast32_t scalerType, head, hhea;

			if (!is_safe_offset(0, 12))
				return -1;
			/* Check for a compatible scalerType (magic number). */
			scalerType = getu32(0);
			if (scalerType != FILE_MAGIC_ONE && scalerType != FILE_MAGIC_TWO)
				return -1;

			if (gettable((char*)"head", &head) < 0)
				return -1;
			if (!is_safe_offset(head, 54))
				return -1;
			unitsPerEm = getu16(head + 18);
			locaFormat = geti16(head + 50);

			if (gettable((char*)"hhea", &hhea) < 0)
				return -1;
			if (!is_safe_offset(hhea, 36))
				return -1;
			numLongHmtx = getu16(hhea + 34);

			return 0;
		}

		int map_file(const std::string_view filename) {
			HANDLE file;
			DWORD high, low;

			mapping = NULL;
			memory = NULL;

			file = CreateFileA(filename.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			if (file == INVALID_HANDLE_VALUE) {
				return -1;
			}

			low = GetFileSize(file, &high);
			if (low == INVALID_FILE_SIZE) {
				CloseHandle(file);
				return -1;
			}

			size = (size_t)high << (8 * sizeof(DWORD)) | low;

			mapping = CreateFileMapping(file, NULL, PAGE_READONLY, high, low, NULL);
			if (!mapping) {
				CloseHandle(file);
				return -1;
			}

			CloseHandle(file);

			memory = (uint8_t*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
			if (!memory) {
				CloseHandle(mapping);
				mapping = NULL;
				return -1;
			}

			return 0;
		}

		void unmap_file() {
			if (memory) {
				UnmapViewOfFile(memory);
				memory = NULL;
			}
			if (mapping) {
				CloseHandle(mapping);
				mapping = NULL;
			}
		}

		int hor_metrics(uint_fast32_t glyph_id, int* advanceWidth, int* leftSideBearing) const {
			uint_fast32_t hmtx, offset, boundary;
			if (gettable((char*)"hmtx", &hmtx) < 0)
				return -1;
			if (glyph_id < numLongHmtx) {
				/* glyph is inside long metrics segment. */
				offset = hmtx + 4 * glyph_id;
				if (!is_safe_offset(offset, 4))
					return -1;
				*advanceWidth = getu16(offset);
				*leftSideBearing = geti16(offset + 2);
				return 0;
			}
			else {
				/* glyph is inside short metrics segment. */
				boundary = hmtx + 4U * (uint_fast32_t)numLongHmtx;
				if (boundary < 4)
					return -1;

				offset = boundary - 4;
				if (!is_safe_offset(offset, 4))
					return -1;
				*advanceWidth = getu16(offset);

				offset = boundary + 2 * (glyph_id - numLongHmtx);
				if (!is_safe_offset(offset, 2))
					return -1;
				*leftSideBearing = geti16(offset);
				return 0;
			}
		}

		/* Returns the offset into the font that the glyph's outline is stored at. */
		int outline_offset(uint_fast32_t glyph_id, uint_fast32_t* offset) const {
			uint_fast32_t loca, glyf;
			uint_fast32_t base, current, next;

			if (gettable((char*)"loca", &loca) < 0)
				return -1;
			if (gettable((char*)"glyf", &glyf) < 0)
				return -1;

			if (locaFormat == 0) {
				base = loca + 2 * glyph_id;

				if (!is_safe_offset(base, 4))
					return -1;

				current = 2U * (uint_fast32_t)getu16(base);
				next = 2U * (uint_fast32_t)getu16(base + 2);
			}
			else {
				base = loca + 4 * glyph_id;

				if (!is_safe_offset(base, 8))
					return -1;

				current = getu32(base);
				next = getu32(base + 4);
			}

			*offset = current == next ? 0 : glyf + current;
			return 0;
		}

		/* For a 'simple' outline, determines each point of the outline with a set of flags. */
		int simple_flags(uint_fast32_t* offset, uint_fast16_t numPts, uint8_t* flags) const {
			uint_fast32_t off = *offset;
			uint_fast16_t i;
			uint8_t value = 0, repeat = 0;
			for (i = 0; i < numPts; ++i) {
				if (repeat) {
					--repeat;
				}
				else {
					if (!is_safe_offset(off, 1))
						return -1;
					value = getu8(off++);
					if (value & REPEAT_FLAG) {
						if (!is_safe_offset(off, 1))
							return -1;
						repeat = getu8(off++);
					}
				}
				flags[i] = value;
			}
			*offset = off;
			return 0;
		}

		/* For a 'simple' outline, decodes both X and Y coordinates for each point of the outline. */
		int simple_points(uint_fast32_t offset, uint_fast16_t numPts, uint8_t* flags, Point* points) const {
			long accum, value, bit;
			uint_fast16_t i;

			accum = 0L;
			for (i = 0; i < numPts; ++i) {
				if (flags[i] & X_CHANGE_IS_SMALL) {
					if (!is_safe_offset(offset, 1))
						return -1;
					value = (long)getu8(offset++);
					bit = !!(flags[i] & X_CHANGE_IS_POSITIVE);
					accum -= (value ^ -bit) + bit;
				}
				else if (!(flags[i] & X_CHANGE_IS_ZERO)) {
					if (!is_safe_offset(offset, 2))
						return -1;
					accum += geti16(offset);
					offset += 2;
				}
				points[i].x = (double)accum;
			}

			accum = 0L;
			for (i = 0; i < numPts; ++i) {
				if (flags[i] & Y_CHANGE_IS_SMALL) {
					if (!is_safe_offset(offset, 1))
						return -1;
					value = (long)getu8(offset++);
					bit = !!(flags[i] & Y_CHANGE_IS_POSITIVE);
					accum -= (value ^ -bit) + bit;
				}
				else if (!(flags[i] & Y_CHANGE_IS_ZERO)) {
					if (!is_safe_offset(offset, 2))
						return -1;
					accum += geti16(offset);
					offset += 2;
				}
				points[i].y = (double)accum;
			}

			return 0;
		}

		int cmap_fmt4(uint_fast32_t table, UChar charCode, uint_fast32_t* glyph_id) const {
			const uint8_t* segPtr;
			uint_fast32_t segIdxX2;
			uint_fast32_t endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
			uint_fast16_t segCountX2, idRangeOffset, startCode, shortCode, idDelta, id;
			uint8_t key[2] = { (uint8_t)(charCode >> 8), (uint8_t)charCode };
			/* cmap format 4 only supports the Unicode BMP. */
			if (charCode > 0xFFFF) {
				*glyph_id = 0;
				return 0;
			}
			shortCode = (uint_fast16_t)charCode;
			if (!is_safe_offset(table, 8))
				return -1;
			segCountX2 = getu16(table);
			if ((segCountX2 & 1) || !segCountX2)
				return -1;
			/* Find starting positions of the relevant arrays. */
			endCodes = table + 8;
			startCodes = endCodes + segCountX2 + 2;
			idDeltas = startCodes + segCountX2;
			idRangeOffsets = idDeltas + segCountX2;
			if (!is_safe_offset(idRangeOffsets, segCountX2))
				return -1;
			/* Find the segment that contains shortCode by binary searching over
			 * the highest codes in the segments. */
			segPtr = (uint8_t*)csearch(key, memory + endCodes, segCountX2 / 2, 2, cmpu16);
			segIdxX2 = (uint_fast32_t)(segPtr - (memory + endCodes));
			/* Look up segment info from the arrays & short circuit if the spec requires. */
			if ((startCode = getu16(startCodes + segIdxX2)) > shortCode)
				return 0;
			idDelta = getu16(idDeltas + segIdxX2);
			if (!(idRangeOffset = getu16(idRangeOffsets + segIdxX2))) {
				/* Intentional integer under- and overflow. */
				*glyph_id = (shortCode + idDelta) & 0xFFFF;
				return 0;
			}
			/* Calculate offset into glyph array and determine ultimate value. */
			idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2U * (unsigned int)(shortCode - startCode);
			if (!is_safe_offset(idOffset, 2))
				return -1;
			id = getu16(idOffset);
			/* Intentional integer under- and overflow. */
			*glyph_id = id ? (id + idDelta) & 0xFFFF : 0;
			return 0;
		}

		int cmap_fmt6(uint_fast32_t table, UChar charCode, uint_fast32_t* glyph_id) const {
			unsigned int firstCode, entryCount;
			/* cmap format 6 only supports the Unicode BMP. */
			if (charCode > 0xFFFF) {
				*glyph_id = 0;
				return 0;
			}
			if (!is_safe_offset(table, 4))
				return -1;
			firstCode = getu16(table);
			entryCount = getu16(table + 2);
			if (!is_safe_offset(table, 4 + 2 * entryCount))
				return -1;
			if (charCode < firstCode)
				return -1;
			charCode -= firstCode;
			if (!(charCode < entryCount))
				return -1;
			*glyph_id = getu16(table + 4 + 2 * charCode);
			return 0;
		}

		int cmap_fmt12_13(uint_fast32_t table, UChar charCode, uint_fast32_t* glyph_id, int which) const {
			uint32_t len, numEntries;
			uint_fast32_t i;

			*glyph_id = 0;

			/* check that the entire header is present */
			if (!is_safe_offset(table, 16))
				return -1;

			len = getu32(table + 4);

			/* A minimal header is 16 bytes */
			if (len < 16)
				return -1;

			if (!is_safe_offset(table, len))
				return -1;

			numEntries = getu32(table + 12);

			for (i = 0; i < numEntries; ++i) {
				uint32_t firstCode, lastCode, glyphOffset;
				firstCode = getu32(table + (i * 12) + 16);
				lastCode = getu32(table + (i * 12) + 16 + 4);
				if (charCode < firstCode || charCode > lastCode)
					continue;
				glyphOffset = getu32(table + (i * 12) + 16 + 8);
				if (which == 12)
					*glyph_id = (charCode - firstCode) + glyphOffset;
				else
					*glyph_id = glyphOffset;
				return 0;
			}

			return 0;
		}

		/* Maps Unicode code points to glyph indices. */
		int glyph_id(UChar charCode, uint_fast32_t* glyph_id) const {
			uint_fast32_t cmap, entry, table;
			unsigned int idx, numEntries;
			int type, format;

			*glyph_id = 0;

			if (gettable((char*)"cmap", &cmap) < 0)
				return -1;

			if (!is_safe_offset(cmap, 4))
				return -1;
			numEntries = getu16(cmap + 2);

			if (!is_safe_offset(cmap, 4 + numEntries * 8))
				return -1;

			/* First look for a 'full repertoire'/non-BMP map. */
			for (idx = 0; idx < numEntries; ++idx) {
				entry = cmap + 4 + idx * 8;
				type = getu16(entry) * 0100 + getu16(entry + 2);
				/* Complete unicode map */
				if (type == 0004 || type == 0312) {
					table = cmap + getu32(entry + 4);
					if (!is_safe_offset(table, 8))
						return -1;
					/* Dispatch based on cmap format. */
					format = getu16(table);
					switch (format) {
					case 12:
						return cmap_fmt12_13(table, charCode, glyph_id, 12);
					default:
						return -1;
					}
				}
			}

			/* If no 'full repertoire' cmap was found, try looking for a BMP map. */
			for (idx = 0; idx < numEntries; ++idx) {
				entry = cmap + 4 + idx * 8;
				type = getu16(entry) * 0100 + getu16(entry + 2);
				/* Unicode BMP */
				if (type == 0003 || type == 0301) {
					table = cmap + getu32(entry + 4);
					if (!is_safe_offset(table, 6))
						return -1;
					/* Dispatch based on cmap format. */
					switch (getu16(table)) {
					case 4:
						return cmap_fmt4(table + 6, charCode, glyph_id);
					case 6:
						return cmap_fmt6(table + 6, charCode, glyph_id);
					default:
						return -1;
					}
				}
			}

			return -1;
		}
	};

	struct Image {
		void* pixels = nullptr;
		int width = 0;
		int height = 0;
	};

	class Outline {
		std::vector<Point> points;
		std::vector<Curve> curves;
		std::vector<Line> lines;

		/* A heuristic to tell whether a given curve can be approximated closely enough by a line. */
		int is_flat(const Curve& curve) const {
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
			for (unsigned i = 0; i < curves.size(); ++i) {
				if (tesselate_curve(curves[i]) < 0)
					return -1;
			}
			return 0;
		}

		void draw_lines(Raster& buf) const {
			for (unsigned i = 0; i < lines.size(); ++i) {
				const Line& line = lines[i];
				const Point& origin = points[line.beg];
				const Point& goal = points[line.end];
				draw_line(buf, origin, goal);
			}
		}

		int decode_contour(uint8_t* flags, uint_fast16_t basePoint, uint_fast16_t count) {
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
				looseEnd = (uint_least16_t)points.size();
				points.push_back(midpoint(points[basePoint], points[basePoint + count - 1]));
			}
			beg = looseEnd;
			gotCtrl = 0;
			for (i = 0; i < count; ++i) {
				/* cur can't overflow because we ensure that basePoint + count < 0xFFFF before calling decode_contour(). */
				cur = (uint_least16_t)(basePoint + i);
				/* NOTE clang-analyzer will often flag this and another piece of code because it thinks that flags and
				 * points + basePoint don't always get properly initialized -- even when you explicitly loop over both
				 * and set every element to zero (but not when you use memset). This is a known clang-analyzer bug:
				 * http://clang-developers.42468.n3.nabble.com/StaticAnalyzer-False-positive-with-loop-handling-td4053875.html */
				if (flags[i] & POINT_IS_ON_CURVE) {
					if (gotCtrl) {
						curves.emplace_back(beg, cur, ctrl);
					}
					else {
						lines.emplace_back(beg, cur);
					}
					beg = cur;
					gotCtrl = 0;
				}
				else {
					if (gotCtrl) {
						center = (uint_least16_t)points.size();
						points.push_back(midpoint(points[ctrl], points[cur]));
						curves.emplace_back(beg, center, ctrl);
						beg = center;
					}
					ctrl = cur;
					gotCtrl = 1;
				}
			}
			if (gotCtrl) {
				curves.emplace_back(beg, looseEnd, ctrl);
			}
			else {
				lines.emplace_back(beg, looseEnd);
			}

			return 0;
		}

		int simple_outline(const Font& font, uint_fast32_t offset, unsigned int numContours) {
			assert(numContours > 0);

			uint_fast16_t basePoint = (uint_fast16_t)points.size();
			uint_fast16_t beg = 0;

			if (!font.is_safe_offset(offset, numContours * 2 + 2))
				return -1;
			uint_fast16_t numPts = font.getu16(offset + (numContours - 1) * 2);
			if (numPts >= UINT16_MAX)
				return -1;
			numPts++;
			if (points.size() > UINT16_MAX - numPts)
				return -1;
			points.resize(basePoint + numPts);

			std::vector<uint_fast16_t> endPts;
			endPts.resize(numContours);

			std::vector<uint8_t> flags;
			flags.resize(numPts);

			for (unsigned i = 0; i < numContours; ++i) {
				endPts[i] = font.getu16(offset);
				offset += 2;
			}
			/* Ensure that endPts are never falling.
			 * Falling endPts have no sensible interpretation and most likely only occur in malicious input.
			 * Therefore, we bail, should we ever encounter such input. */
			for (unsigned i = 0; i < numContours - 1; ++i) {
				if (endPts[i + 1] < endPts[i] + 1)
					return -1;
			}
			offset += 2U + font.getu16(offset);

			if (font.simple_flags(&offset, numPts, flags.data()) < 0)
				return -1;
			if (font.simple_points(offset, numPts, flags.data(), points.data() + basePoint) < 0)
				return -1;

			for (unsigned i = 0; i < numContours; ++i) {
				uint_fast16_t count = endPts[i] - beg + 1;
				if (decode_contour(flags.data() + beg, basePoint + beg, count) < 0)
					return -1;
				beg = endPts[i] + 1;
			}

			return 0;
		}

		int compound_outline(const Font& font, uint_fast32_t offset, int recDepth) {
			double local[6];
			unsigned int flags, glyph_id, basePoint;
			/* Guard against infinite recursion (compound glyphs that have themselves as component). */
			if (recDepth >= 4)
				return -1;
			do {
				memset(local, 0, sizeof local);
				if (!font.is_safe_offset(offset, 4))
					return -1;
				flags = font.getu16(offset);
				glyph_id = font.getu16(offset + 2);
				offset += 4;
				/* We don't implement point matching, and neither does stb_truetype for that matter. */
				if (!(flags & ACTUAL_XY_OFFSETS))
					return -1;
				/* Read additional X and Y offsets (in FUnits) of this component. */
				if (flags & OFFSETS_ARE_LARGE) {
					if (!font.is_safe_offset(offset, 4))
						return -1;
					local[4] = font.geti16(offset);
					local[5] = font.geti16(offset + 2);
					offset += 4;
				}
				else {
					if (!font.is_safe_offset(offset, 2))
						return -1;
					local[4] = font.geti8(offset);
					local[5] = font.geti8(offset + 1);
					offset += 2;
				}
				if (flags & GOT_A_SINGLE_SCALE) {
					if (!font.is_safe_offset(offset, 2))
						return -1;
					local[0] = font.geti16(offset) / 16384.0;
					local[3] = local[0];
					offset += 2;
				}
				else if (flags & GOT_AN_X_AND_Y_SCALE) {
					if (!font.is_safe_offset(offset, 4))
						return -1;
					local[0] = font.geti16(offset + 0) / 16384.0;
					local[3] = font.geti16(offset + 2) / 16384.0;
					offset += 4;
				}
				else if (flags & GOT_A_SCALE_MATRIX) {
					if (!font.is_safe_offset(offset, 8))
						return -1;
					local[0] = font.geti16(offset + 0) / 16384.0;
					local[1] = font.geti16(offset + 2) / 16384.0;
					local[2] = font.geti16(offset + 4) / 16384.0;
					local[3] = font.geti16(offset + 6) / 16384.0;
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
				uint_fast32_t outline;
				if (font.outline_offset(glyph_id, &outline) < 0)
					return -1;
				if (outline) {
					basePoint = (unsigned)points.size();
					if (decode_outline(font, outline, recDepth + 1) < 0)
						return -1;
					transform_points((unsigned)points.size() - basePoint, points.data() + basePoint, local);
				}
			} while (flags & THERE_ARE_MORE_COMPONENTS);

			return 0;
		}

	public:
		Outline() {
			points.reserve(64);
			curves.reserve(64);
			lines.reserve(64);
		}

		int decode_outline(const Font& font, uint_fast32_t offset, int recDepth) {
			if (!font.is_safe_offset(offset, 10))
				return -1;
			const int numContours = font.geti16(offset);
			if (numContours > 0) {
				/* Glyph has a 'simple' outline consisting of a number of contours. */
				return simple_outline(font, offset + 10, (unsigned int)numContours);
			}
			else if (numContours < 0) {
				/* Glyph has a compound outline combined from mutiple other outlines. */
				return compound_outline(font, offset + 10, recDepth);
			}
			else {
				return 0;
			}
		}

		int render_outline(double transform[6], Image image) {
			transform_points((unsigned)points.size(), points.data(), transform);
			clip_points((unsigned)points.size(), points.data(), image.width, image.height);

			if (tesselate_curves() < 0)
				return -1;

			std::vector<Cell> cells;
			cells.resize((unsigned int)image.width * (unsigned int)image.height);

			Raster buf;
			buf.cells = cells.data();
			buf.width = image.width;
			buf.height = image.height;
			draw_lines(buf);
			post_process(buf, (uint8_t*)image.pixels);

			return 0;
		}
	};


	struct Metrics { // TODO merge with Glyph
		double advanceWidth = 0.0;
		double leftSideBearing = 0.0;
		int yOffset = 0;
		int minWidth = 0;
		int minHeight = 0;

		bool is_valid() const { return advanceWidth > 0; }
	};

	struct Renderer { // TODO make class // TODO rename
		double xScale = 0.0;
		double yScale = 0.0;
		double xOffset = 0.0;
		double yOffset = 0.0;
		int flags = 0;

		double ascender = 0.0;
		double descender = 0.0;
		double lineGap = 0.0;

		int lmetrics(const Font& font) {
			uint_fast32_t hhea;
			if (font.gettable((char*)"hhea", &hhea) < 0)
				return -1;
			if (!font.is_safe_offset(hhea, 36))
				return -1;
			const double factor = yScale / font.unitsPerEm;
			ascender = font.geti16(hhea + 4) * factor;
			descender = font.geti16(hhea + 6) * factor;
			lineGap = font.geti16(hhea + 8) * factor;
			return 0;
		}

		Metrics get_metrics(const Font& font, const uint_fast32_t glyph_id) const {
			int adv, lsb;
			if (font.hor_metrics(glyph_id, &adv, &lsb) < 0)
				return {};

			Metrics metrics;
			const double xscale = xScale / font.unitsPerEm;
			metrics.advanceWidth = adv * xscale;
			metrics.leftSideBearing = lsb * xscale + xOffset;

			uint_fast32_t outline;
			if (font.outline_offset(glyph_id, &outline) < 0)
				return {};
			if (!outline)
				return metrics;

			int bbox[4];
			if (glyph_bbox(font, outline, bbox) < 0)
				return {};

			metrics.minWidth = bbox[2] - bbox[0] + 1;
			metrics.minHeight = bbox[3] - bbox[1] + 1;
			metrics.yOffset = flags & DOWNWARD_Y ? -bbox[3] : bbox[1];
			return metrics;
		}

		int glyph_bbox(const Font& font, uint_fast32_t outline, int box[4]) const {
			/* Read the bounding box from the font file verbatim. */
			if (!font.is_safe_offset(outline, 10))
				return -1;
			box[0] = font.geti16(outline + 2);
			box[1] = font.geti16(outline + 4);
			box[2] = font.geti16(outline + 6);
			box[3] = font.geti16(outline + 8);
			if (box[2] <= box[0] || box[3] <= box[1])
				return -1;

			/* Transform the bounding box into SFT coordinate space. */
			const double xscale = xScale / font.unitsPerEm;
			const double yscale = yScale / font.unitsPerEm;
			box[0] = (int)floor(box[0] * xscale + xOffset);
			box[1] = (int)floor(box[1] * yscale + yOffset);
			box[2] = (int)ceil(box[2] * xscale + xOffset);
			box[3] = (int)ceil(box[3] * yscale + yOffset);
			return 0;
		}

		int render(const Font& font, uint_fast32_t glyph_id, Image image) const {
			uint_fast32_t outline;
			if (font.outline_offset(glyph_id, &outline) < 0)
				return -1;
			if (!outline)
				return 0;

			int bbox[4];
			if (glyph_bbox(font, outline, bbox) < 0)
				return -1;

			/* Set up the transformation matrix such that
			 * the transformed bounding boxes min corner lines
			 * up with the (0, 0) point. */
			double transform[6];
			transform[0] = xScale / font.unitsPerEm;
			transform[1] = 0.0;
			transform[2] = 0.0;
			transform[4] = xOffset - bbox[0];
			if (flags & DOWNWARD_Y) {
				transform[3] = -yScale / font.unitsPerEm;
				transform[5] = bbox[3] - yOffset;
			}
			else {
				transform[3] = +yScale / font.unitsPerEm;
				transform[5] = yOffset - bbox[1];
			}

			Outline outl;
			if (outl.decode_outline(font, outline, 0) < 0)
				return -1;
			if (outl.render_outline(transform, image) < 0)
				return -1;

			return 0;
		}

	};

}
