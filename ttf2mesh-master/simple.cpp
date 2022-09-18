#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "ttf2mesh.h"
#include "glwindow.h"

static ttf_mesh_t *mesh = nullptr;

void on_render()
{
    int width, height;
    glwindow_get_size(&width, &height);

    glwindow_begin_draw();

    glViewport(0, 0, width, height);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, width, 0.0, height, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (mesh != nullptr)
    {
        glTranslatef((float)width / 4.0f, (float)height / 10.0f, 0.0f);
        glScalef((float)height, (float)height, 1.0f);

        glColor3f(0.0, 0.0, 0.0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, &mesh->vert->x);
        glDrawElements(GL_TRIANGLES, mesh->nfaces * 3, GL_UNSIGNED_INT, &mesh->faces->v1);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    glwindow_end_draw();
};

int app_main()
{
    const char* dir = ".";
    ttf_t** list = ttf_list_fonts(&dir, 1, "PragmataPro*");
    if (list == nullptr) return 1; // no memory in system
    if (list[0] == nullptr) return 1; // no fonts were found

    static ttf_t* font = nullptr;
    ttf_load_from_file(list[0]->filename, &font, false);
    ttf_free_list(list);
    if (font == nullptr) return 1;

    wchar_t symbol = L'a';
    int index = ttf_find_glyph(font, symbol);
    if (index < 0) return 1;

    const auto res = ttf_glyph2mesh(&font->glyphs[index], &mesh, TTF_QUALITY_HIGH, TTF_FEATURES_DFLT);
    if (res != TTF_DONE) return 1;

    // [TODO] Output font.inc file.
    // [TODO] Output font.h file.
    // [TODO] Remove glwindow.
    // [TODO] Rename project to font.
    // [TODO] Move to main folder.

    if (!glwindow_create(400, 400, "Press [space] or [A...Z] to select view"))
    {
        fprintf(stderr, "unable to create opengl window\n");
        return 2;
    }

    glwindow_render_cb = on_render;
    glwindow_eventloop();

    ttf_free_mesh(mesh);
    ttf_free(font);

    return 0;
}
