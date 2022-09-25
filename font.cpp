#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <fstream>
#include <vector>
#include <array>
#include <Windows.h>

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
        for(const auto& vertex : symbol.vertices) {
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

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow) {
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
    total_vertex_count += 1; // EOV

    ttf_free(font);
    ttf_free_list(list);

    output_include(symbols, total_vertex_count);
    output_header(symbols);

    return 0;
}
