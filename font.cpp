#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <fstream>
#include <Windows.h>

#include "ttf2mesh.h"

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow)
{
    const char* dir = ".";
    ttf_t** list = ttf_list_fonts(&dir, 1, "PragmataPro*");
    if (list == nullptr) return 1;
    if (list[0] == nullptr) return 1;

    ttf_t* font = nullptr;
    ttf_load_from_file(list[0]->filename, &font, false);
    ttf_free_list(list);
    if (font == nullptr) return 1;

    wchar_t symbol = L'a';
    int index = ttf_find_glyph(font, symbol);
    if (index < 0) return 1;

    ttf_mesh_t *mesh = nullptr;
    const auto res = ttf_glyph2mesh(&font->glyphs[index], &mesh, TTF_QUALITY_LOW, TTF_FEATURES_DFLT);
    if (res != TTF_DONE) return 1;

    // [TODO] Output all ASCII 128 characters.
    // [TODO] Also output vertex_offsets in font.inc.
    {
        std::ofstream out("font.inc", std::ios::trunc | std::ios::out);
        out << "const vec2 vertices[" << mesh->nfaces * 3 << "] = vec2[" << mesh->nfaces * 3 << "](\n";
        for (int i = 0; i < mesh->nfaces; i++)
        {
            out << "\tvec2(" << mesh->vert[mesh->faces[i].v1].x << ", " << mesh->vert[mesh->faces[i].v1].y << ")"; out << ",\n";
            out << "\tvec2(" << mesh->vert[mesh->faces[i].v2].x << ", " << mesh->vert[mesh->faces[i].v2].y << ")"; out << ",\n";
            out << "\tvec2(" << mesh->vert[mesh->faces[i].v3].x << ", " << mesh->vert[mesh->faces[i].v3].y << ")";
            if (i < mesh->nfaces - 1)
                out << ",\n";
            else
                out << "\n";
        }
        out << ");\n";
    }

    // [TODO] Output vertex_counts in font.h.

    ttf_free_mesh(mesh);
    ttf_free(font);

    return 0;
}
