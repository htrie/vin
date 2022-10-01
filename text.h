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

class Stack {
    struct State {
        std::string text;
        size_t cursor = 0;
    };

    std::vector<State> states;
    bool modified = false;
    bool undo = false;

public:
    Stack() {
        states.emplace_back(); // [TODO] Remove when file loading.
        states.back().text = {
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
            "abcdefghijklmnopqrstuvwxyz\n"
            "\n"
            "\tlorep ipsum\n"
            "`1234567890-=[]\\;',./\n"
            "~!@#$%^&*()_+{}|:\"<>?\n"
            "abcdefghijklmnopqrstuvwxyz\n"
            "abcdefghijklmnopqrstuvwxy\n"
            "abcdefghijklmnopqrstuvwx\n"
            "abcdefghijklmnopqrstuvw\n"
            "abcdefghijklmnopqrstuv\n"
            "abcdefghijklmnopqrstu\n"
            "abcdefghijklmnopqrst\n"
            "abcdefghijklmnopqrs\n"
            "abcdefghijklmnopqr\n"
            "abcdefghijklmnopq\n"
            "abcdefghijklmnop\n"
            "abcdefghijklmno\n"
            "abcdefghijklmn\n"
            "abcdefghijklm\n"
            "abcdefghijkl\n"
            "abcdefghijk\n"
            "abcdefghij\n"
            "abcdefghi\n"
            "abcdefgh\n"
            "abcdefg\n"
            "abcdef\n"
            "abcde\n"
            "abcd\n"
            "abc\n"
            "ab\n"
            "a\n"
            "\n" };
    }

    std::string& get_text() { return states.back().text; }

    size_t get_cursor() const { return states.back().cursor; }
    void set_cursor(size_t u) { states.back().cursor = u; }

    void set_modified() { modified = true; };
    void set_undo() { undo = true; }

    void push() {
        if (states.size() > 100) { states.erase(states.begin()); }
        if (states.size() > 0) { states.push_back(states.back()); }
    }

    void pop() {
        if (!modified && states.size() > 1) { std::swap(states[states.size() - 2], states[states.size() - 1]); states.pop_back(); }
        if (undo && states.size() > 1) { states.pop_back(); undo = false; }
        modified = false;
    }
};

class Buffer {
    Stack stack;

    Color status_line_color = Color::rgba(41, 62, 79, 255);
    Color status_text_color = Color::rgba(215, 236, 249, 255);
    Color cursor_color = Color::rgba(255, 255, 0, 255);
    Color cursor_line_color = Color::rgba(65, 80, 29, 255);
    Color whitespace_color = Color::rgba(75, 100, 93, 255);
    Color text_color = Color::rgba(205, 226, 239, 255);
    Color text_cursor_color = Color::rgba(5, 5, 5, 255);
    Color line_number_color = Color::rgba(75, 100, 121, 255);

    unsigned begin_row = 0;

    bool quit = false;
    bool space_mode = false;
    bool insert_mode = false;

    size_t find_line_number(std::string_view text, size_t cursor) const {
        size_t number = 0;
        size_t index = 0;
        while (index < text.size() && index < cursor) {
            if (text[index++] == '\n')
                number++;
        }
        return number;
    }

    void insert(std::string_view s) {
        stack.get_text().insert(stack.get_cursor(), s);
        stack.set_cursor(std::min(stack.get_cursor() + s.length(), stack.get_text().size() - 1));
        stack.set_modified();
    }

    void erase_back() {
        if (stack.get_cursor() > 0) {
            stack.get_text().erase(stack.get_cursor() - 1, 1);
            stack.set_cursor(stack.get_cursor() > 0 ? stack.get_cursor() - 1 : stack.get_cursor());
            stack.set_modified();
        }
    }

    void erase() {
        if (stack.get_text().size() > 0) {
            stack.get_text().erase(stack.get_cursor(), 1);
            stack.set_cursor(stack.get_cursor() == stack.get_text().size() ? stack.get_cursor() - 1 : stack.get_cursor());
            stack.set_modified();
        }
    }

    void next_char() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(std::clamp(stack.get_cursor() + 1, current.begin(), current.end()));
    }

    void prev_char() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(std::clamp(stack.get_cursor() - 1, current.begin(), current.end()));
    }

    void line_start() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(current.begin());
    }

    void line_end() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(current.end());
    }

    void line_start_whitespace() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(current.begin());
        while (stack.get_cursor() <= current.end()) {
            if (!is_whitespace(stack.get_text()[stack.get_cursor()]))
                break;
            next_char();
        }
    }

    void next_line() {
        Line current(stack.get_text(), stack.get_cursor());
        Line next(stack.get_text(), current.end() < stack.get_text().size() - 1 ? current.end() + 1 : current.end());
        stack.set_cursor(next.to_absolute(current.to_relative(stack.get_cursor())));
    }

    void prev_line() {
        Line current(stack.get_text(), stack.get_cursor());
        Line prev(stack.get_text(), current.begin() > 0 ? current.begin() - 1 : 0);
        stack.set_cursor(prev.to_absolute(current.to_relative(stack.get_cursor())));
    }

    void buffer_end() {
        Line current(stack.get_text(), stack.get_cursor());
        Line last(stack.get_text(), stack.get_text().size() - 1);
        stack.set_cursor(last.to_absolute(current.to_relative(stack.get_cursor())));
    }

    void buffer_start() {
        Line current(stack.get_text(), stack.get_cursor());
        Line first(stack.get_text(), 0);
        stack.set_cursor(first.to_absolute(current.to_relative(stack.get_cursor())));
    }

    void process_insert(WPARAM key) {
        if (key == Glyph::ESC) { insert_mode = false; }
        else if (key == Glyph::BS) { erase_back(); }
        else if (key == Glyph::TAB) { insert("\t"); }
        else if (key == Glyph::CR) { insert("\n"); }
        else { insert(std::string(1, (char)key)); }
    }

    void process_normal(WPARAM key) {
        if (key == ' ') { space_mode = true; }
        else if (key == 'u') { stack.set_undo(); }
        else if (key == 'i') { insert_mode = true; }
        else if (key == 'I') { line_start_whitespace(); insert_mode = true; }
        else if (key == 'a') { next_char(); insert_mode = true; }
        else if (key == 'A') { line_end(); insert_mode = true; }
        else if (key == 'o') { line_end(); insert("\n"); insert_mode = true; }
        else if (key == 'O') { line_start(); insert("\n"); prev_line(); insert_mode = true; }
        else if (key == 'x') { erase(); }
        else if (key == '0') { line_start(); }
        else if (key == '_') { line_start_whitespace(); }
        else if (key == '$') { line_end(); }
        else if (key == 'h') { prev_char(); }
        else if (key == 'j') { next_line(); }
        else if (key == 'k') { prev_line(); }
        else if (key == 'l') { next_char(); }
        else if (key == 'g') { buffer_start(); }
        else if (key == 'G') { buffer_end(); }
    }

    void process_space(WPARAM key) {
        if (key == 'q') { quit = true; }
        else { space_mode = false; }
    }

    void push_digit(Characters& characters, unsigned row, unsigned col, unsigned digit) {
        characters.emplace_back((uint8_t)(48 + digit), line_number_color, row, col);
    }

    void push_line_number(Characters& characters, unsigned row, unsigned col, unsigned line) {
        if (line > 999) { push_digit(characters, row, col + 0, (line % 10000) / 1000); }
        if (line > 99) { push_digit(characters, row, col + 1, (line % 1000) / 100); }
        if (line > 9) { push_digit(characters, row, col + 2, (line % 100)  / 10); }
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

    void push_text(Characters& characters, unsigned row_count) {
        Line cursor_line(stack.get_text(), stack.get_cursor());
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        begin_row = std::clamp(begin_row, cursor_row > row_count ? cursor_row - row_count : 0, cursor_row);
        const unsigned end_row = begin_row + row_count;
        unsigned absolute_row = 0;
        unsigned index = 0;
        unsigned row = 1;
        unsigned col = 0;
        bool new_row = true;
        for (auto& character : stack.get_text()) {
            if (absolute_row >= begin_row && absolute_row <= end_row) {
                if (new_row) { 
                    unsigned line = absolute_row;
                    unsigned column = line == cursor_row ? col : col + 1;
                    line = line == cursor_row ? line :
                        line < cursor_row ? cursor_row - line :
                        line - cursor_row;
                    push_line_number(characters, row, column, line); col += 6; new_row = false; }
                if (index >= cursor_line.begin() && index <= cursor_line.end()) { push_cursor_line(characters, row, col, character == '\t' ? 4 : 1); }
                if (index == stack.get_cursor()) { push_cursor(characters, row, col); }
                if (character == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; new_row = true; }
                else if (character == '\t') { push_tab(characters, row, col); col += 4; }
                else { push_char(characters, row, col, character, index == stack.get_cursor() && !insert_mode); col++; }
            }
            else {
                if (character == '\n') { absolute_row++; }
            }
            index++;
        }
    }

    void push_status_text(Characters& characters, const std::string_view text) {
        unsigned col = 0;
        for (auto& character : text) {
            characters.emplace_back((uint8_t)character, status_text_color, 0, col++);
        }
    }

    void push_status_line(Characters& characters, unsigned col_count) {
        for (unsigned i = 0; i < col_count; ++i) {
            characters.emplace_back(Glyph::BLOCK, status_line_color, 0, i);
        }
    }

    void push_status_bar(Characters& characters, float process_time, float redraw_time, unsigned col_count) {
        push_status_line(characters, col_count);
        push_status_text(characters, build_status_text(process_time, redraw_time));
    }

    std::string build_status_text(float process_time, float redraw_time) {
        Line cursor_line(stack.get_text(), stack.get_cursor());
        const auto text_size = std::to_string(stack.get_text().size()) + " bytes";
        const auto cursor_col = std::string("col ") + std::to_string(cursor_line.to_relative(stack.get_cursor()));
        const auto cursor_row = std::string("row ") + std::to_string(find_line_number(stack.get_text(), stack.get_cursor()));
        const auto process_duration = std::string("proc ") + std::to_string((unsigned)(process_time * 1000.0f)) + "us";
        const auto redraw_duration = std::string("draw ") + std::to_string((unsigned)(redraw_time * 1000.0f)) + "us";
        return std::string("test.cpp") + 
            " [" + text_size + ", " + cursor_col + ", " + cursor_row +  "]" +
            " (" + process_duration + ", " + redraw_duration + ")";
    }

public:
    bool process(WPARAM key) {
        stack.push();
        if (space_mode) { process_space(key); }
        else if (insert_mode) { process_insert(key); }
        else { process_normal(key); }
        stack.pop();
        verify(stack.get_cursor() < stack.get_text().size());
        return quit;
    }

    Characters cull(float process_time, float redraw_time, unsigned col_count, unsigned row_count) {
        Characters characters;
        characters.reserve(256);
        push_status_bar(characters, process_time, redraw_time, col_count);
        push_text(characters, row_count);
        return characters;
    }
};

