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
#include <array>
#include <string>
#include <Windows.h>

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
	std::ifstream in("font_0.tga", std::ios::binary);

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

void output(const Tga& tga, const Fnt& fnt) {
	std::ofstream out("font.h", std::ios::trunc | std::ios::out);

	out << "const uint32_t font_width = " << tga.header.width << ";" << std::endl;
	out << "const uint32_t font_height = " << tga.header.height << ";" << std::endl;
	out << std::endl;

	out << "const std::array<uint8_t, " << tga.content.size() << "> font_pixels  = {" << std::endl;
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

	out << "struct FontGlyph {" << std::endl;
	out << "\tfloat x = 0.0f;" << std::endl;
	out << "\tfloat y = 0.0f;" << std::endl;
	out << "\tfloat w = 0.0f;" << std::endl;
	out << "\tfloat h = 0.0f;" << std::endl;
	out << "\tfloat x_off = 0.0f;" << std::endl;
	out << "\tfloat y_off = 0.0f;" << std::endl;
	out << "\tfloat x_adv = 0.0f;" << std::endl;
	out << std::endl;
	out << "\tFontGlyph(float x, float y, float w, float h, float x_off, float y_off, float x_adv)" << std::endl;
	out << "\t\t: x(x), y(y), w(w), h(h), x_off(x_off), y_off(y_off), x_adv(x_adv) {}" << std::endl;
	out << "};" << std::endl;
	out << std::endl;

	out << "const std::unordered_map<uint16_t, FontGlyph> font_glyphs  = {" << std::endl;
	for (unsigned i = 0; i < fnt.chars.values.size(); i++) {
		const auto& c = fnt.chars.values[i];
		out << "\t{ " << c.id << 
			", FontGlyph(" << 
			std::to_string((float)c.x / (float)tga.header.width) << "f, " << 
			std::to_string((float)c.y / (float)tga.header.height) << "f, " << 
			std::to_string((float)c.width / (float)tga.header.width) << "f, " << 
			std::to_string((float)c.height / (float)tga.header.height) << "f, " << 
			std::to_string((float)c.xoffset / (float)tga.header.width) << "f, " << 
			std::to_string((float)c.yoffset / (float)tga.header.height) << "f, " << 
			std::to_string((float)c.xadvance / (float)tga.header.width) << "f) }";
		if (i < (unsigned)fnt.chars.values.size() - 1)
			out << ",";
		out << std::endl;
	}
	out << "};" << std::endl;

}

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow) {
	const auto tga = parse_tga();
	const auto fnt = parse_fnt();
	output(tga, fnt);
	return 0;
}
