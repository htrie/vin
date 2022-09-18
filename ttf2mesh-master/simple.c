#define _CRT_SECURE_NO_WARNINGS

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "ttf2mesh.h"
#include "glwindow.h"

static ttf_t *font = NULL;
static ttf_mesh_t *mesh = NULL;

static bool load_system_font()
{
    const char* dir = ".";
    ttf_t **list = ttf_list_fonts(&dir, 1, "PragmataPro*");
    if (list == NULL) return false; // no memory in system
    if (list[0] == NULL) return false; // no fonts were found

    ttf_load_from_file(list[0]->filename, &font, false);
    ttf_free_list(list);
    if (font == NULL) return false;

    printf("font \"%s\" loaded\n", font->names.full_name);
    return true;
}

static void choose_glyph(wchar_t symbol)
{
    int index = ttf_find_glyph(font, symbol);
    if (index < 0) return;

    ttf_mesh_t *out;
    if (ttf_glyph2mesh(&font->glyphs[index], &out, TTF_QUALITY_HIGH, TTF_FEATURES_DFLT) != TTF_DONE)
        return;

    ttf_free_mesh(mesh);
    mesh = out;
}

static void on_render()
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

    if (mesh != NULL)
    {
        glTranslatef(width / 4, height / 10, 0);
        glScalef(height, height, 1.0f);

        glColor3f(0.0, 0.0, 0.0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, &mesh->vert->x);
        glDrawElements(GL_TRIANGLES, mesh->nfaces * 3,  GL_UNSIGNED_INT, &mesh->faces->v1);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    glwindow_end_draw();
}

static void on_key_event(wchar_t key, const bool ctrl_alt_shift[3], bool pressed)
{
    (void)ctrl_alt_shift;

    if (key == 27 && !pressed) // escape
    {
        glwindow_destroy();
        return;
    }

    choose_glyph(key + 32);
    glwindow_repaint();
}

int app_main()
{
    if (!load_system_font())
    {
        fprintf(stderr, "unable to load system font\n");
        return 1;
    }

    choose_glyph(L'A');

    // [TODO] Output font.inc file.
    // [TODO] Output font.h file.
    // [TODO] Rename project to font.
    // [TODO] Move to main folder.
    // [TODO] Remove glwindow.

    if (!glwindow_create(400, 400, "Press [space] or [A...Z] to select view"))
    {
        fprintf(stderr, "unable to create opengl window\n");
        return 2;
    }

    // init window callbacks
    // and go to event loop

    glwindow_key_cb = on_key_event;
    glwindow_render_cb = on_render;
    glwindow_eventloop();

    return 0;
}
