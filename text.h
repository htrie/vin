#pragma once

enum Glyph {
    BS = 8,
    TAB = 9,
    CR = 13,
    ESC = 27,
    BLOCK = 128,
    LINE = 129,
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
        // [TODO] display cursor.
        // [TODO] insert/normal mode.
        // [TODO] block/line cursor.
        // [TODO] h/j/k/l to move.
        // [TODO] space+Q to quit.
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
            //buffer += (char)Glyph::BLOCK;
            buffer += (char)Glyph::LINE;
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

