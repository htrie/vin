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

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow) {
    const char* dir = ".";
    ttf_t** list = ttf_list_fonts(&dir, 1, "PragmataPro*");
    if (list == nullptr) return 1;
    if (list[0] == nullptr) return 1;

    ttf_t* font = nullptr;
    ttf_load_from_file(list[0]->filename, &font, false);
    ttf_free_list(list);
    if (font == nullptr) return 1;
    
    std::array<Symbol, 128> symbols; // ASCII table.
    unsigned total_vertex_count = 0;

    for (unsigned c = 0; c < 128; ++c) {
        int index = ttf_find_glyph(font, c);
        if (index < 0) continue;

        ttf_mesh_t* mesh = nullptr;
        const auto res = ttf_glyph2mesh(&font->glyphs[index], &mesh, TTF_QUALITY_LOW, TTF_FEATURES_DFLT);
        if (res != TTF_DONE) continue;

        if (mesh->nfaces > 0) {
            symbols[c].vertex_offset = total_vertex_count;
            for (int i = 0; i < mesh->nfaces; i++) {
                symbols[c].vertices.emplace_back(mesh->vert[mesh->faces[i].v1].x, mesh->vert[mesh->faces[i].v1].y);
                symbols[c].vertices.emplace_back(mesh->vert[mesh->faces[i].v2].x, mesh->vert[mesh->faces[i].v2].y);
                symbols[c].vertices.emplace_back(mesh->vert[mesh->faces[i].v3].x, mesh->vert[mesh->faces[i].v3].y);
                total_vertex_count += 3;
            }
        }

        ttf_free_mesh(mesh);
    }

    total_vertex_count += 1; // EOV

    ttf_free(font);

    {
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

        out << "const uint vertex_offsets[129] = uint[129](\n";
        for (unsigned c = 0; c < 128; ++c) {
            out << "\t" << symbols[c].vertex_offset << ",\n";
        }
        out << "\t0\n";
        out << ");\n";
    }

    {
        std::ofstream out("font.h", std::ios::trunc | std::ios::out);

        out << "const std::array<unsigned, 129> vertex_counts = {\n";
        for (unsigned c = 0; c < 128; ++c) {
            out << "\t" << symbols[c].vertices.size() << ",\n";
        }
        out << "\t0\n";
        out << "};\n";
    }

    return 0;
}
