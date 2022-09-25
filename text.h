#pragma once

enum Glyph {
    BS = 8,
    TAB = 9,
    CR = 13,
    ESC = 27,
    DEL = 127,
    BLOCK = 128,
};

typedef std::array<float, 4> Color;

class Text {
    std::string buffer = {
        "abcdefghijklmnopqrstuvwxyz\n"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "`1234567890-=[]\\;',./\n"
        "~!@#$%^&*()_+{}|:\"<>?\n" };

public:
    void process(WPARAM key) {
        // [TODO] block cursor.
        // [TODO] space+Q to quit.
        // [TODO] h/j/k/l to move.
        // [TODO] insert/normal mode.
        if (key == Glyph::BS) {
            if (!buffer.empty())
                buffer.pop_back();
        }
        else if (key == Glyph::TAB) {
            buffer += "    ";
        }
        else if (key == Glyph::CR) {
            buffer += '\n';
        }
        else if (key == Glyph::ESC) {
            // [TODO]
            buffer += (char)Glyph::BLOCK;
        }
        else if (key == Glyph::DEL) {
            // [TODO]
        }
        else {
            buffer += (char)key;
        }
    }

    std::string cull() {
        // [TODO] Clip lines to fit screen.
        return buffer;
    }
};

