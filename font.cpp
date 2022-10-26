#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <string>
#include <Windows.h>

#if 0
#include "ttf2mesh.h"

struct Vertex {
	float x = 0.0f;
	float y = 0.0f;

	Vertex(float x, float y) : x(x), y(y) {}
};

struct Symbol {
	unsigned vertex_offset = 0;
	std::vector<Vertex> vertices;
};

typedef std::vector<Symbol> Symbols;

void output_include(const Symbols& symbols, unsigned total_vertex_count) {
	std::ofstream out("font.inc", std::ios::trunc | std::ios::out);

	out << "const vec2 vertices[" << total_vertex_count << "] = vec2[" << total_vertex_count << "](\n";
	for (const auto& symbol : symbols) {
		for (const auto& vertex : symbol.vertices) {
			out << "\tvec2(" << vertex.x << ", " << vertex.y << "),\n";
		}
	}
	out << "\tvec2(0.0, 0.0)"; // EOV
	out << ");\n";
	out << "\n";

	out << "const uint vertex_offsets[" << (symbols.size() + 1) << "] = uint[" << (symbols.size() + 1) << "](\n";
	for (auto& symbol : symbols) {
		out << "\t" << symbol.vertex_offset << ",\n";
	}
	out << "\t0\n";
	out << ");\n";
}

void output_header(const Symbols& symbols) {
	std::ofstream out("font.h", std::ios::trunc | std::ios::out);

	out << "const std::array<unsigned, " << (symbols.size() + 1) << "> vertex_counts = {\n";
	for (auto& symbol : symbols) {
		out << "\t" << symbol.vertices.size() << ",\n";
	}
	out << "\t0\n";
	out << "};\n";
}

std::vector<Vertex> generate_vertices(ttf_t* font, wchar_t c) {
	std::vector<Vertex> out;
	const int index = ttf_find_glyph(font, c);
	if (index >= 0) {
		ttf_mesh_t* mesh = nullptr;
		const auto res = ttf_glyph2mesh(&font->glyphs[index], &mesh, TTF_QUALITY_LOW, TTF_FEATURES_DFLT);
		if (res == TTF_DONE) {
			if (mesh->nfaces > 0) {
				out.reserve(mesh->nfaces * 3);
				for (int i = 0; i < mesh->nfaces; i++) {
					out.emplace_back(mesh->vert[mesh->faces[i].v1].x, mesh->vert[mesh->faces[i].v1].y);
					out.emplace_back(mesh->vert[mesh->faces[i].v2].x, mesh->vert[mesh->faces[i].v2].y);
					out.emplace_back(mesh->vert[mesh->faces[i].v3].x, mesh->vert[mesh->faces[i].v3].y);
				}
			}
			ttf_free_mesh(mesh);
		}
	}
	return out;
}
#endif

unsigned parse_unsigned(const std::string& s, const std::string& name) {
	unsigned u = 0;
	std::sscanf(s.c_str(), std::string(name + "=%u").c_str(), &u);
	return u;
}

std::string parse_string(const std::string& s, const std::string& name) {
	const size_t offset = name.size() + 1;
	return s.substr(offset, s.size() - offset);
}

struct Char {
	unsigned id = 0;
	unsigned x = 0;
	unsigned y = 0;
	unsigned width = 0;
	unsigned height = 0;
	unsigned xoffset = 0;
	unsigned yoffset = 0;
	unsigned xadvance = 0;
	unsigned page = 0;
	unsigned chnl = 0;
};

Char parse_char(std::istringstream& iss) {
	Char chr;
	while (iss) {
		std::string field;
		iss >> field;
		if (field.starts_with("id=")) { chr.id = parse_unsigned(field, "id"); }
		else if (field.starts_with("x=")) { chr.x = parse_unsigned(field, "x"); }
		else if (field.starts_with("y=")) { chr.y = parse_unsigned(field, "y"); }
		else if (field.starts_with("width=")) { chr.width = parse_unsigned(field, "width"); }
		else if (field.starts_with("height=")) { chr.height = parse_unsigned(field, "height"); }
		else if (field.starts_with("xoffset=")) { chr.xoffset = parse_unsigned(field, "xoffset"); }
		else if (field.starts_with("yoffset=")) { chr.yoffset = parse_unsigned(field, "yoffset"); }
		else if (field.starts_with("xadvance=")) { chr.xadvance = parse_unsigned(field, "xadvance"); }
		else if (field.starts_with("page=")) { chr.page = parse_unsigned(field, "page"); }
		else if (field.starts_with("chnl=")) { chr.chnl = parse_unsigned(field, "chnl"); }
	}
	return chr;
}

struct Chars {
	unsigned count = 0;
	std::vector<Char> values;
};

Chars parse_chars(std::istringstream& iss) {
	Chars chars;
	while (iss) {
		std::string field;
		iss >> field;
		if (field.starts_with("count=")) { chars.count = parse_unsigned(field, "count"); }
	}
	return chars;
}

struct Common {
	unsigned scaleW = 0;
	unsigned scaleH = 0;
};

Common parse_common(std::istringstream& iss) {
	Common common;
	while (iss) {
		std::string field;
		iss >> field;
		if (field.starts_with("scaleW=")) { common.scaleW = parse_unsigned(field, "scaleW"); }
		else if (field.starts_with("scaleH=")) { common.scaleH = parse_unsigned(field, "scaleH"); }
	}
	return common;
}

struct Info {
	unsigned bold = 0;
};

Info parse_info(std::istringstream& iss) {
	Info info;
	while (iss) {
		std::string field;
		iss >> field;
		if (field.starts_with("bold")) { info.bold = parse_unsigned(field, "bold"); }
	}
	return info;
}

struct Page {
	std::string file;
};

Page parse_page(std::istringstream& iss) {
	Page page;
	while (iss) {
		std::string field;
		iss >> field;
		if (field.starts_with("file=")) { page.file = parse_string(field, "file"); }
	}
	return page;
}

struct Fnt {
	Info info;
	Common common;
	Page page;
	Chars chars;
};

Fnt parse_fnt() {
	Fnt fnt;
	std::ifstream in("font.fnt");
	std::string line;
	unsigned char_count = 0;
	while (std::getline(in, line)) {
		std::istringstream iss(line);
		std::string keyword;
		iss >> keyword;
		if (keyword == "info") { fnt.info = parse_info(iss); }
		else if (keyword == "common") { fnt.common = parse_common(iss); }
		else if (keyword == "page") { fnt.page = parse_page(iss); }
		else if (keyword == "chars") { fnt.chars = parse_chars(iss); }
		else if (keyword == "char") { fnt.chars.values.push_back(parse_char(iss)); }
	}
	return fnt;
}

struct Tga {
  #pragma pack(push, 1)
    struct Header {
        uint8_t idlength;
        uint8_t colourmaptype;
        uint8_t datatypecode;
        uint16_t colourmaporigin;
        uint16_t colourmaplength;
        uint8_t colourmapdepth;
        uint16_t x_origin;
        uint16_t y_origin;
        uint16_t width;
        uint16_t height;
        uint8_t bitsperpixel;
        uint8_t imagedescriptor;
    };
    #pragma pack(pop)

	Header header;
	std::vector<uint8_t> content;
};

Tga parse_tga() {
	Tga tga;
	std::ifstream in("font_0.tga");
	in.seekg(0, std::ios_base::end);
	const size_t size = in.tellg();
	in.seekg(0, std::ios_base::beg);
	const size_t header_size = sizeof(Tga::Header);
	in.read((char*)&tga.header, header_size);
	const auto content_size = size - header_size;
	tga.content.resize(content_size);
	in.read((char*)tga.content.data(), content_size);
	return tga;
}

void output_include(const Tga& tga) {
	std::ofstream out("font.inc", std::ios::trunc | std::ios::out);
}

void output_header(const Fnt& fnt) {
	std::ofstream out("font.h", std::ios::trunc | std::ios::out);
}

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow) {
#if 1
	output_include(parse_tga());
	output_header(parse_fnt());
#else
	const char* dir = ".";
	ttf_t** list = ttf_list_fonts(&dir, 1, "PragmataPro*");
	if (list == nullptr) return 1;
	if (list[0] == nullptr) return 1;

	ttf_t* font = nullptr;
	ttf_load_from_file(list[0]->filename, &font, false);
	if (font == nullptr) return 1;

	Symbols symbols;
	symbols.reserve(256);
	unsigned total_vertex_count = 0;

	const auto add_char = [&](wchar_t c) {
		symbols.emplace_back();
		auto& symbol = symbols.back();
		symbol.vertex_offset = total_vertex_count;
		symbol.vertices = generate_vertices(font, c);
		total_vertex_count += (unsigned)symbol.vertices.size();
	};

	for (unsigned c = 0; c < 128; ++c) { add_char(c); } // ASCII table.
	add_char(0x2588); // 128: block
	add_char(0x258F); // 129: left vertical line
	add_char(0x23CE); // 130: return
	add_char(0x2581); // 131: bottom block
	add_char(0x23F5); // 132: tab sign
	add_char(0x2024); // 133: space sign
	total_vertex_count += 1; // EOV

	ttf_free(font);
	ttf_free_list(list);

	output_include(symbols, total_vertex_count);
	output_header(symbols);
#endif

	return 0;
}
