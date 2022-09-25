#pragma once

enum Glyph {
    BS = 8,
    TAB = 9,
    CR = 13,
    ESC = 27,
    BLOCK = 128,
    LINE = 129,
};

class Color
{
public:
    uint8_t b = 0, g = 0, r = 0, a = 0;

    uint32_t& c() { return ( uint32_t& )*this; }
    const uint32_t& c() const { return ( uint32_t& )*this; }

    Color() noexcept {}
    Color(const Color& o) { *this = o; }

    explicit Color(uint32_t C) { c() = C; }
    explicit Color(float r, float g, float b, float a) : r((uint8_t)(r * 255.0f)), g((uint8_t)(g * 255.0f)), b((uint8_t)(b * 255.0f)), a((uint8_t)(a * 255.0f)) {}
    explicit Color(const std::array<float, 4>& v) : Color(v[0], v[1], v[2], v[3]) {}

    static Color argb(unsigned a, unsigned r, unsigned g, unsigned b) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
    static Color rgba(unsigned r, unsigned g, unsigned b, unsigned a) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }

    std::array<float, 4> rgba() const { return { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f }; }

    bool operator==(const Color& o) const { return b == o.b && g == o.g && r == o.r && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }
};

struct Character {
    uint8_t index;
    Color color = Color::rgba(255, 0, 0, 255);
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
        // [TODO] <h/j/k/l> to move.
        // [TODO] <space+Q> to quit.
        return insert_mode ? process_insert(key) : process_normal(key);
    }

    Characters cull() {
        // [TODO] Display cursor line in transparent yellow.
        // [TODO] Clip lines to fit screen.
        Characters characters;
        characters.reserve(256);

        unsigned cursor_row = 0; // [TODO] Place cursor.
        unsigned cursor_col = 0;

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
                if (row == cursor_row && col == cursor_col)
                    characters.emplace_back(insert_mode ? Glyph::LINE : Glyph::BLOCK, row, col); // [TODO] Display cursor in yellow.
                characters.emplace_back((uint8_t)character, row, col);
                col++;
            }
        }

        return characters;
    }
};

