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
    const std::string_view text;
    size_t start = 0;
    size_t finish = 0;

public:
    Line(std::string_view text, size_t pos) {
        if (text.size() > 0) {
            verify(pos < text.size());
            const auto pn = text.rfind('\n', pos > 0 && text[pos] == '\n' ? pos - 1 : pos);
            const auto nn = text.find('\n', pos);
            start = pn != std::string::npos ? (pn < pos ? pn + 1 : pn) : 0;
            finish = nn != std::string::npos ? nn : text.size() - 1;
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

class Buffer {
    std::string text = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "abcdefghijklmnopqrstuvwxyz\n"
        "\n"
        "\tlorep ipsum\n"
        "`1234567890-=[]\\;',./\n"
        "~!@#$%^&*()_+{}|:\"<>?\n"
        "\n" };

    Color cursor_color = Color::rgba(255, 255, 0, 255);
    Color cursor_line_color = Color::rgba(65, 80, 29, 255);
    Color whitespace_color = Color::rgba(75, 100, 93, 255);
    Color text_color = Color::rgba(205, 226, 239, 255);
    Color text_cursor_color = Color::rgba(5, 5, 5, 255);
    Color line_number_color = Color::rgba(75, 100, 121, 255);

    size_t cursor = 0;

    bool space_mode = false;
    bool insert_mode = false;

    void insert(std::string_view s) {
        text.insert(cursor, s);
        cursor = std::min(cursor + s.length(), text.size() - 1);
    }

    void erase_back() {
        if (cursor > 0) {
            text.erase(cursor - 1, 1);
            cursor = cursor > 0 ? cursor - 1 : cursor;
        }
    }

    void erase() {
        if (text.size() > 0) {
            text.erase(cursor, 1);
            cursor = cursor == text.size() ? cursor - 1 : cursor;
        }
    }

    void next_char() {
        cursor = cursor < text.size() - 1 ? cursor + 1 : 0;
    }

    void prev_char() {
        cursor = cursor > 0 ? cursor - 1 : text.size() - 1;
    }

    void line_start() {
        Line current(text, cursor);
        cursor = current.begin();
    }

    void line_end() {
        Line current(text, cursor);
        cursor = current.end();
    }

    void line_start_whitespace() {
        Line current(text, cursor);
        cursor = current.begin();
        while (cursor <= current.end()) {
            if (!is_whitespace(text[cursor]))
                break;
            next_char();
        }
    }

    void next_line() {
        Line current(text, cursor);
        Line next(text, current.end() < text.size() - 1 ? current.end() + 1 : 0);
        cursor = next.to_absolute(current.to_relative(cursor));
    }

    void prev_line() {
        Line current(text, cursor);
        Line prev(text, current.begin() > 0 ? current.begin() - 1 : text.size() - 1);
        cursor = prev.to_absolute(current.to_relative(cursor));
    }

    bool process_insert(WPARAM key) {
        if (key == Glyph::ESC) { insert_mode = false; }
        else if (key == Glyph::BS) { erase_back(); }
        else if (key == Glyph::TAB) { insert("\t"); }
        else if (key == Glyph::CR) { insert("\n"); }
        else { insert(std::string(1, (char)key)); }
        verify(cursor < text.size());
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
        else if (key == '0') { line_start(); }
        else if (key == '_') { line_start_whitespace(); }
        else if (key == '$') { line_end(); }
        else if (key == 'h') { prev_char(); }
        else if (key == 'j') { next_line(); }
        else if (key == 'k') { prev_line(); }
        else if (key == 'l') { next_char(); }
        verify(cursor < text.size());
        return false;
    }

    bool process_space(WPARAM key) {
        if (key == 'q') { return true; }
        else { space_mode = false; }
        return false;
    }

    void push_digit(Characters& characters, unsigned row, unsigned col, unsigned digit) {
        characters.emplace_back((uint8_t)(48 + digit), line_number_color, row, col);
    }

    void push_line_number(Characters& characters, unsigned row, unsigned col, unsigned line) {
        line = std::min(line, 9999u); // [TODO] Support more?
        if (line > 999) { push_digit(characters, row, col + 0, line / 1000); }
        if (line > 99) { push_digit(characters, row, col + 1, line / 100); }
        if (line > 9) { push_digit(characters, row, col + 2, line / 10); }
        push_digit(characters, row, col + 3, line % 10);
    }

    void push_cursor_line(Characters& characters, unsigned row, unsigned col, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            characters.emplace_back(Glyph::BLOCK, cursor_line_color, row, col + i);
        }
    };

    void push_cursor(Characters& characters, unsigned row, unsigned col) {
        characters.emplace_back(insert_mode ? Glyph::LINE : Glyph::BLOCK, cursor_color, row, col);
    };

    void push_return(Characters& characters, unsigned row, unsigned col) {
        characters.emplace_back(Glyph::RETURN, whitespace_color, row, col);
    };

    void push_tab(Characters& characters, unsigned row, unsigned col) {
        characters.emplace_back(Glyph::TAB, whitespace_color, row, col);
    };

    void push_char(Characters& characters, unsigned row, unsigned col, char character, bool block_cursor) {
        characters.emplace_back((uint8_t)character, block_cursor ? text_cursor_color : text_color, row, col);
    };


public:
    bool process(WPARAM key) {
        // [TODO] Open test file using <space-e>.
        // [TODO] Save test file using <space-s>.
        // [TODO] Unit tests.
        // [TODO] zz/zt/zb.
        // [TODO] gg/G.
        // [TODO] dd.
        // [TODO] u.
        return space_mode ? process_space(key) : 
            insert_mode ? process_insert(key) :
            process_normal(key);
    }

    Characters cull() {
        // [TODO] Relative line numbers.
        // [TODO] Vertical scrolling.
        // [TODO] Clip lines to fit screen.
        Line cursor_line(text, cursor);
        Characters characters;
        characters.reserve(256);
        unsigned index = 0;
        unsigned row = 0;
        unsigned col = 0;
        bool new_row = true;
        for (auto& character : text) {
            if (new_row) { push_line_number(characters, row, col, row); col += 5; new_row = false; }
            if (index >= cursor_line.begin() && index <= cursor_line.end()) { push_cursor_line(characters, row, col, character == '\t' ? 4 : 1); }
            if (index == cursor) { push_cursor(characters, row, col); }
            if (character == '\n') { push_return(characters, row, col); row++; col = 0; new_row = true; }
            else if (character == '\t') { push_tab(characters, row, col); col += 4; }
            else { push_char(characters, row, col, character, index == cursor && !insert_mode); col++; }
            index++;
        }
        return characters;
    }
};

