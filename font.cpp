#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <Windows.h>

unsigned parse_unsigned(const std::string_view s, const std::string_view name) {
	unsigned u = 0;
	std::sscanf(s.data(), (std::string(name) + "=%u").c_str(), &u);
	return u;
}

std::string parse_string(const std::string_view s, const std::string_view name) {
	const size_t offset = name.size() + 1;
	return std::string(s.substr(offset, s.size() - offset));
}

struct Char {
	unsigned id = 0;
	unsigned x = 0;
	unsigned y = 0;
	unsigned width = 0;
	unsigned height = 0;
	int xoffset = 0;
	int yoffset = 0;
	int xadvance = 0;
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

Fnt parse_fnt(const std::string_view font_filename) {
	Fnt fnt;
	std::ifstream in(std::string(font_filename) + ".fnt");
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

Tga parse_tga(const std::string_view font_filename) {
	Tga tga;
	std::ifstream in(std::string(font_filename) + "_0.tga", std::ios::binary);

	in.seekg(0, std::ios_base::end);
	const size_t size = in.tellg();
	in.seekg(0, std::ios_base::beg);

	const size_t header_size = sizeof(Tga::Header);
	in.read((char*)&tga.header, header_size);

	const size_t footer_size = 26;
	const auto content_size = size - header_size - footer_size;
	tga.content.resize(content_size);
	in.read((char*)tga.content.data(), content_size);

	return tga;
}

void output(const std::string_view font_filename, const Tga& tga, const Fnt& fnt) {
	std::ofstream out(std::string(font_filename) + ".h", std::ios::trunc | std::ios::out);

	out << "const uint32_t " << font_filename << "_width = " << tga.header.width << ";" << std::endl;
	out << "const uint32_t " << font_filename << "_height = " << tga.header.height << ";" << std::endl;
	out << std::endl;

	out << "const uint8_t " << font_filename << "_pixels[" << tga.content.size() << "] = {" << std::endl;
	for (unsigned j = 0; j < tga.header.height; j++) {
		for (unsigned i = 0; i < tga.header.width; i++) {
			out << (unsigned)tga.content[i + tga.header.width * j];
			if ( (i < (unsigned)(tga.header.width - 1)) || (j < (unsigned)(tga.header.height - 1)))
				out << ",";
		}
		out << std::endl;
	}
	out << "};" << std::endl;
	out << std::endl;

	out << "#ifndef FONT_GLYPH" << std::endl;
	out << "#define FONT_GLYPH" << std::endl;
	out << "struct FontGlyph {" << std::endl;
	out << "	uint16_t id = 0;" << std::endl;
	out << "	uint16_t x = 0;" << std::endl;
	out << "	uint16_t y = 0;" << std::endl;
	out << "	uint16_t w = 0;" << std::endl;
	out << "	uint16_t h = 0;" << std::endl;
	out << "	uint16_t x_off = 0;" << std::endl;
	out << "	uint16_t y_off = 0;" << std::endl;
	out << "	uint16_t x_adv = 0;" << std::endl;
	out << std::endl;
	out << "	FontGlyph() {}" << std::endl;
	out << "	FontGlyph(uint16_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t x_off, uint16_t y_off, uint16_t x_adv)" << std::endl;
	out << "		: id(id), x(x), y(y), w(w), h(h), x_off(x_off), y_off(y_off), x_adv(x_adv) {}" << std::endl;
	out << "};" << std::endl;
	out << std::endl;
	out << "typedef std::vector<FontGlyph> FontGlyphs;" << std::endl;
	out << "#endif" << std::endl;
	out << std::endl;

	out << "const FontGlyphs " << font_filename << "_glyphs  = {" << std::endl;
	for (unsigned i = 0; i < fnt.chars.values.size(); i++) {
		const auto& c = fnt.chars.values[i];
		out << "\t{ " << c.id << ", " <<
			std::to_string(c.x) << ", " << 
			std::to_string(c.y) << ", " << 
			std::to_string(c.width) << ", " << 
			std::to_string(c.height) << ", " << 
			std::to_string(c.xoffset) << ", " << 
			std::to_string(c.yoffset ) << ", " << 
			std::to_string(c.xadvance) << " }";
		if (i < (unsigned)fnt.chars.values.size() - 1)
			out << ",";
		out << std::endl;
	}
	out << "};" << std::endl;
}

void process(const std::string_view font_filename) {
	const auto tga = parse_tga(font_filename);
	const auto fnt = parse_fnt(font_filename);
	output(font_filename, tga, fnt);
}

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow) {
	process("font");
	return 0;
}
