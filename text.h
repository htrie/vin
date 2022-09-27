#pragma once

enum Glyph {
    BS = 8,
    TAB = 9,
    CR = 13,
    ESC = 27,
    BLOCK = 128,
    LINE = 129,
    RETURN = 130,
};

constexpr bool is_whitespace(char c) { return c == '\t' || c == ' '; }
constexpr bool is_eol(char c) { return c == '\n'; }

struct Character {
    uint8_t index;
    Color color = Color::rgba(255, 0, 0, 255);
    unsigned row = 0;
    unsigned col = 0;

    Character(uint8_t index, Color color, unsigned row, unsigned col)
        : index(index), color(color), row(row), col(col) {}
};

typedef std::vector<Character> Characters;

class Line {
    const std::string_view buffer;
    size_t start = 0;
    size_t finish = 0;

public:
    Line(std::string_view buffer, size_t pos) {
        if (buffer.size() > 0) {
            verify(pos < buffer.size());
            const auto pn = buffer.rfind('\n', pos > 0 && buffer[pos] == '\n' ? pos - 1 : pos);
            const auto nn = buffer.find('\n', pos);
            start = pn != std::string::npos ? (pn < pos ? pn + 1 : pn) : 0;
            finish = nn != std::string::npos ? nn : buffer.size() - 1;
            verify(start <= finish);
        }
    }

    size_t to_relative(size_t pos) const {
        verify(pos != std::string::npos);
        verify(pos >= start && pos <= finish);
        return pos - start;
    }

    size_t to_absolute(size_t pos) const {
        verify(pos != std::string::npos);
        return std::min(start + pos, finish);
    }

    size_t begin() const { return start; }
    size_t end() const { return finish; }
};

class Text {
    std::string buffer = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "abcdefghijklmnopqrstuvwxyz\n"
        "\n"
        "\tlorep ipsum\n"
        "`1234567890-=[]\\;',./\n"
        "~!@#$%^&*()_+{}|:\"<>?\n" };

    Color cursor_color = Color::rgba(255, 255, 0, 255);
    Color whitespace_color = Color::rgba(75, 100, 93, 255);
    Color text_color = Color::rgba(205, 226, 239, 255);

    size_t cursor = 0;

    bool space_mode = false;
    bool insert_mode = false;

    void insert(std::string_view s) {
        buffer.insert(cursor, s);
        cursor = std::min(cursor + s.length(), buffer.size() - 1);
    }

    void erase_back() {
        if (cursor > 0) {
            buffer.erase(cursor - 1, 1);
            cursor = cursor > 0 ? cursor - 1 : cursor;
        }
    }

    void erase() {
        if (buffer.size() > 0) {
            buffer.erase(cursor, 1);
            cursor = cursor == buffer.size() ? cursor - 1 : cursor;
        }
    }

    void next_char() {
        cursor = cursor < buffer.size() - 1 ? cursor + 1 : 0;
    }

    void prev_char() {
        cursor = cursor > 0 ? cursor - 1 : buffer.size() - 1;
    }

    void line_start() {
        Line current(buffer, cursor);
        cursor = current.begin();
    }

    void line_end() {
        Line current(buffer, cursor);
        cursor = current.end();
    }

    void line_start_whitespace() {
        Line current(buffer, cursor);
        cursor = current.begin();
        while (cursor <= current.end()) {
            if (!is_whitespace(buffer[cursor]))
                break;
            next_char();
        }
    }

    void next_line() {
        Line current(buffer, cursor);
        Line next(buffer, current.end() < buffer.size() - 1 ? current.end() + 1 : 0);
        cursor = next.to_absolute(current.to_relative(cursor));
    }

    void prev_line() {
        Line current(buffer, cursor);
        Line prev(buffer, current.begin() > 0 ? current.begin() - 1 : buffer.size() - 1);
        cursor = prev.to_absolute(current.to_relative(cursor));
    }

    bool process_insert(WPARAM key) {
        if (key == Glyph::ESC) { insert_mode = false; }
        else if (key == Glyph::BS) { erase_back(); }
        else if (key == Glyph::TAB) { insert("\t"); }
        else if (key == Glyph::CR) { insert("\n"); }
        else { insert(std::string(1, (char)key)); }
        verify(cursor < buffer.size());
        return false;
    }

    bool process_normal(WPARAM key) {
        if (key == ' ') { space_mode = true; }
        else if (key == 'i') { insert_mode = true; }
        else if (key == 'I') { line_start_whitespace(); insert_mode = true; }
        else if (key == 'a') { next_char(); insert_mode = true; }
        else if (key == 'A') { line_end(); insert_mode = true; }
        else if (key == 'o') { next_line(); line_start(); insert("\n"); prev_char(); insert_mode = true; }
        else if (key == 'O') { line_start(); insert("\n"); prev_char(); insert_mode = true; }
        else if (key == 'x') { erase(); }
        else if (key == 'u') { } // [TODO]
        else if (key == 'G') { } // [TODO]
        else if (key == '0') { line_start(); }
        else if (key == '_') { line_start_whitespace(); }
        else if (key == '$') { line_end(); }
        else if (key == 'h') { prev_char(); }
        else if (key == 'j') { next_line(); }
        else if (key == 'k') { prev_line(); }
        else if (key == 'l') { next_char(); }
        else if (key == 'w') { } // [TODO]
        else if (key == 'e') { } // [TODO]
        else if (key == 'b') { } // [TODO]
        verify(cursor < buffer.size());
        return false;
    }

    bool process_space(WPARAM key) {
        if (key == 'q') { return true; }
        else { space_mode = false; }
        return false;
    }

public:
    bool process(WPARAM key) {
        // [TODO] Open test.cpp file and move buffer init there.
        // [TODO] Save test.cpp file using <space-s>.
        // [TODO] zz.
        // [TODO] gg.
        // [TODO] dd.
        return space_mode ? process_space(key) : 
            insert_mode ? process_insert(key) :
            process_normal(key);
    }

    Characters cull() {
        // [TODO] Display cursor line in transparent yellow.
        // [TODO] Clip lines to fit screen.
        Characters characters;
        characters.reserve(256);

        unsigned index = 0;
        unsigned row = 0;
        unsigned col = 0;
        for (auto& character : buffer)
        {
            if (index == cursor)
                characters.emplace_back(insert_mode ? Glyph::LINE : Glyph::BLOCK, cursor_color, row, col);

            if (character == '\n') {
                characters.emplace_back(Glyph::RETURN, whitespace_color, row, col);
                row++;
                col = 0;
            }
            else if (character == '\t') {
                characters.emplace_back(Glyph::TAB, whitespace_color, row, col);
                col += 4;
            }
            else
            {
                characters.emplace_back((uint8_t)character, text_color, row, col);
                col++;
            }
            index++;
        }

        return characters;
    }
};

