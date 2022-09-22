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

static const unsigned ascii_count = 256;

struct Vertex {
    float x = 0.0f;
    float y = 0.0f;

    Vertex(float x, float y) : x(x), y(y) {}
};

struct Symbol {
    unsigned vertex_offset = 0;
    std::vector<Vertex> vertices;
};

void output_include(const std::array<Symbol, ascii_count>& symbols, unsigned total_vertex_count) {
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

    out << "const uint vertex_offsets[" << (ascii_count + 1) << "] = uint[" << (ascii_count + 1) << "](\n";
    for (unsigned c = 0; c < ascii_count; ++c) {
        out << "\t" << symbols[c].vertex_offset << ",\n";
    }
    out << "\t0\n";
    out << ");\n";
}

void output_header(const std::array<Symbol, ascii_count>& symbols) {
    std::ofstream out("font.h", std::ios::trunc | std::ios::out);

    out << "const std::array<unsigned, " << (ascii_count + 1) << "> vertex_counts = {\n";
    for (unsigned c = 0; c < ascii_count; ++c) {
        out << "\t" << symbols[c].vertices.size() << ",\n";
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
    
    std::array<Symbol, ascii_count> symbols;
    unsigned total_vertex_count = 0;
    for (unsigned c = 0; c < ascii_count; ++c) {
        symbols[c].vertex_offset = total_vertex_count;
        symbols[c].vertices = generate_vertices(font, c);
        total_vertex_count += (unsigned)symbols[c].vertices.size();
    }
    total_vertex_count += 1; // EOV

    ttf_free(font);
    ttf_free_list(list);

    output_include(symbols, total_vertex_count);
    output_header(symbols);

    return 0;
}
