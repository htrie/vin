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

struct Character {
    uint8_t index;
    Color color = { 1.0f, 0.0f, 0.0f, 1.0f }; // [TODO] Use simd::color.
    unsigned row = 0;
    unsigned col = 0;

    Character(uint8_t index, unsigned row, unsigned col)
        : index(index), row(row), col(col) {}
};

typedef std::vector<Character> Characters;

class Text {
    std::string buffer = {
        "abcdefghijklmnopqrstuvwxyz\n"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "`1234567890-=[]\\;',./\n"
        "~!@#$%^&*()_+{}|:\"<>?\n" };

    bool insert_mode = false;

    void process_insert(WPARAM key) {
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
            insert_mode = false;
        }
        else {
            buffer += (char)key;
            //buffer += (char)Glyph::BLOCK;
            //buffer += (char)Glyph::LINE;
        }
    }

    void process_normal(WPARAM key) {
        if (key == 'i') {
            insert_mode = true;
        }
        else {
        }
    }

public:
    void process(WPARAM key) {
        // [TODO] display cursor
        // [TODO] block/line cursor
        // [TODO] h/j/k/l to move
        // [TODO] space+Q to quit
        return insert_mode ? process_insert(key) : process_normal(key);
    }

    Characters cull() {
        // [TODO] Clip lines to fit screen.
        Characters characters;
        characters.reserve(256);

        unsigned row = 0;
        unsigned col = 0;
        for (auto& character : buffer)
        {
            if (character == '\n') {
                row++;
                col = 0;
            }
            else
            {
                characters.emplace_back((uint8_t)character, row, col);
                col++;
            }
        }

        return characters;
    }
};

