#pragma once

// [TODO] u.
// [TODO] Open test file using <space-e>.
// [TODO] Save test file using <space-s>.
// [TODO] Unit tests.
// [TODO] zz/zt/zb.
// [TODO] gg/G.
// [TODO] dd.
// [TODO] Relative line numbers.
// [TODO] Vertical scrolling.
// [TODO] Clip lines to fit screen.

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

struct State {
    std::string text;
    size_t cursor = 0;
};

class Buffer {
    std::vector<State> states;

    Color cursor_color = Color::rgba(255, 255, 0, 255);
    Color cursor_line_color = Color::rgba(65, 80, 29, 255);
    Color whitespace_color = Color::rgba(75, 100, 93, 255);
    Color text_color = Color::rgba(205, 226, 239, 255);
    Color text_cursor_color = Color::rgba(5, 5, 5, 255);
    Color line_number_color = Color::rgba(75, 100, 121, 255);

    bool quit = false;
    bool undo = false;
    bool space_mode = false;
    bool insert_mode = false;

    std::string& get_text() { return states.back().text; }
    size_t get_cursor() const { return states.back().cursor; }
    void set_cursor(size_t u) { states.back().cursor = u; }

    void insert(std::string_view s) {
        get_text().insert(get_cursor(), s);
        set_cursor(std::min(get_cursor() + s.length(), get_text().size() - 1));
    }

    void erase_back() {
        if (get_cursor() > 0) {
            get_text().erase(get_cursor() - 1, 1);
            set_cursor(get_cursor() > 0 ? get_cursor() - 1 : get_cursor());
        }
    }

    void erase() {
        if (get_text().size() > 0) {
            get_text().erase(get_cursor(), 1);
            set_cursor(get_cursor() == get_text().size() ? get_cursor() - 1 : get_cursor());
        }
    }

    void next_char() {
        set_cursor(get_cursor() < get_text().size() - 1 ? get_cursor() + 1 : 0);
    }

    void prev_char() {
        set_cursor(get_cursor() > 0 ? get_cursor() - 1 : get_text().size() - 1);
    }

    void line_start() {
        Line current(get_text(), get_cursor());
        set_cursor(current.begin());
    }

    void line_end() {
        Line current(get_text(), get_cursor());
        set_cursor(current.end());
    }

    void line_start_whitespace() {
        Line current(get_text(), get_cursor());
        set_cursor(current.begin());
        while (get_cursor() <= current.end()) {
            if (!is_whitespace(get_text()[get_cursor()]))
                break;
            next_char();
        }
    }

    void next_line() {
        Line current(get_text(), get_cursor());
        Line next(get_text(), current.end() < get_text().size() - 1 ? current.end() + 1 : 0);
        set_cursor(next.to_absolute(current.to_relative(get_cursor())));
    }

    void prev_line() {
        Line current(get_text(), get_cursor());
        Line prev(get_text(), current.begin() > 0 ? current.begin() - 1 : get_text().size() - 1);
        set_cursor(prev.to_absolute(current.to_relative(get_cursor())));
    }

    bool process_insert(WPARAM key) {
        if (key == Glyph::ESC) { insert_mode = false; return false; }
        else if (key == Glyph::BS) { erase_back(); return true; }
        else if (key == Glyph::TAB) { insert("\t"); return true; }
        else if (key == Glyph::CR) { insert("\n"); return true; }
        else { insert(std::string(1, (char)key)); return true; }
        return false;
    }

    bool process_normal(WPARAM key) {
        if (key == ' ') { space_mode = true; return false; }
        else if (key == 'u') { undo = true; return false; }
        else if (key == 'i') { insert_mode = true; return false; }
        else if (key == 'I') { line_start_whitespace(); insert_mode = true; return false; }
        else if (key == 'a') { next_char(); insert_mode = true; return false; }
        else if (key == 'A') { line_end(); insert_mode = true; return false; }
        else if (key == 'o') { next_line(); line_start(); insert("\n"); prev_char(); insert_mode = true; return true; }
        else if (key == 'O') { line_start(); insert("\n"); prev_char(); insert_mode = true; return true; }
        else if (key == 'x') { erase(); return true; }
        else if (key == '0') { line_start(); return false; }
        else if (key == '_') { line_start_whitespace(); return false; }
        else if (key == '$') { line_end(); return false; }
        else if (key == 'h') { prev_char(); return false; }
        else if (key == 'j') { next_line(); return false; }
        else if (key == 'k') { prev_line(); return false; }
        else if (key == 'l') { next_char(); return false; }
        return false;
    }

    bool process_space(WPARAM key) {
        if (key == 'q') { quit = true; return false; }
        else { space_mode = false; return false; }
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
    Buffer() {
        states.emplace_back();
        states.back().text = {
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
            "abcdefghijklmnopqrstuvwxyz\n"
            "\n"
            "\tlorep ipsum\n"
            "`1234567890-=[]\\;',./\n"
            "~!@#$%^&*()_+{}|:\"<>?\n"
            "\n" };
    }

    bool process(WPARAM key) {
        if (states.size() > 100) { states.erase(states.begin()); } // [TODO] State stack class.
        if (states.size() > 0) { states.push_back(states.back()); }
        const bool modified = space_mode ? process_space(key) : 
            insert_mode ? process_insert(key) :
            process_normal(key);
        if (!modified && states.size() > 1) { std::swap(states[states.size() - 2], states[states.size() - 1]); states.pop_back(); }
        if (undo && states.size() > 1) { states.pop_back(); undo = false; }
        verify(get_cursor() < get_text().size());
        return quit;
    }

    Characters cull() {
        Line cursor_line(get_text(), get_cursor());
        Characters characters;
        characters.reserve(256);
        unsigned index = 0;
        unsigned row = 0;
        unsigned col = 0;
        bool new_row = true;
        for (auto& character : get_text()) {
            if (new_row) { push_line_number(characters, row, col, row); col += 5; new_row = false; }
            if (index >= cursor_line.begin() && index <= cursor_line.end()) { push_cursor_line(characters, row, col, character == '\t' ? 4 : 1); }
            if (index == get_cursor()) { push_cursor(characters, row, col); }
            if (character == '\n') { push_return(characters, row, col); row++; col = 0; new_row = true; }
            else if (character == '\t') { push_tab(characters, row, col); col += 4; }
            else { push_char(characters, row, col, character, index == get_cursor() && !insert_mode); col++; }
            index++;
        }
        return characters;
    }
};

