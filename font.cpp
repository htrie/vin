#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <fstream>
#include <vector>
#include <Windows.h>

#include "ttf2mesh.h"

struct Vertex {
    float x = 0.0f;
    float y = 0.0f;

    Vertex(float x, float y) : x(x), y(y) {}
};

struct Symbol {
    wchar_t character;
    std::vector<Vertex> vertices;

    Symbol(wchar_t c) : character(c) {}
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
    
    std::vector<Symbol> symbols;
    symbols.emplace_back(L'a');
    symbols.emplace_back(L'b');
    unsigned total_vertex_count = 0;
    // [TODO] Output all ASCII 128 characters.

    for (auto& symbol : symbols) {
        int index = ttf_find_glyph(font, symbol.character);
        if (index < 0) return 1;

        ttf_mesh_t* mesh = nullptr;
        const auto res = ttf_glyph2mesh(&font->glyphs[index], &mesh, TTF_QUALITY_LOW, TTF_FEATURES_DFLT);
        if (res != TTF_DONE) return 1;

        for (int i = 0; i < mesh->nfaces; i++) {
            symbol.vertices.emplace_back(mesh->vert[mesh->faces[i].v1].x, mesh->vert[mesh->faces[i].v1].y);
            symbol.vertices.emplace_back(mesh->vert[mesh->faces[i].v2].x, mesh->vert[mesh->faces[i].v2].y);
            symbol.vertices.emplace_back(mesh->vert[mesh->faces[i].v3].x, mesh->vert[mesh->faces[i].v3].y);
            total_vertex_count += 3;
        }

        ttf_free_mesh(mesh);
    }

    total_vertex_count += 1; // EOV

    ttf_free(font);

    // [TODO] Output vertex_counts in font.h.
    // [TODO] Also output vertex_offsets in font.inc.
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
    }

    return 0;
}
