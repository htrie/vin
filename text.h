#pragma once

enum class Mode {
    normal,
    normal_number,
    normal_d,
    normal_z,
    insert,
    replace,
    space
};

enum Glyph {
    BS = 8,
    TAB = 9,
    CR = 13,
    ESC = 27,
    BLOCK = 128,
    LINE = 129,
    RETURN = 130,
    BOTTOM_BLOCK = 131,
};

constexpr std::string_view mode_letter(Mode mode) {
    switch (mode) {
    case Mode::normal: return "N";
    case Mode::normal_number: return "0";
    case Mode::normal_d: return "D";
    case Mode::normal_z: return "Z";
    case Mode::insert: return "I";
    case Mode::replace: return "R";
    case Mode::space: return " ";
    }
    return "";
}

constexpr bool is_whitespace(char c) { return c == '\n' || c == '\t' || c == ' '; }
constexpr bool is_line_whitespace(char c) { return c == '\t' || c == ' '; }

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

    void fix_eof() {
        const auto size = get_text().size();
        if (size == 0 || (size > 0 && get_text()[size - 1] != '\n')) {
            get_text() += '\n';
        }
    }

public:
    Stack() {
        states.emplace_back();
        fix_eof();
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
        fix_eof();
        modified = false;
    }
};

class Buffer {
    Stack stack;
    Timer timer;

    Color status_line_color = Color::rgba(1, 22, 39, 255);
    Color status_text_color = Color::rgba(155, 155, 155, 255);
    Color notification_line_color = Color::rgba(1, 22, 39, 255);
    Color notification_text_color = Color::rgba(24, 112, 139, 255);
    Color cursor_color = Color::rgba(255, 255, 0, 255);
    Color cursor_line_color = Color::rgba(65, 80, 29, 255);
    Color whitespace_color = Color::rgba(75, 100, 93, 255);
    Color text_color = Color::rgba(205, 226, 239, 255);
    Color text_cursor_color = Color::rgba(5, 5, 5, 255);
    Color line_number_color = Color::rgba(75, 100, 121, 255);

    Mode mode = Mode::normal;

    std::string filename;
    std::string notification;
    std::string clipboard;

    unsigned accu = 0;
    unsigned begin_row = 0;

    bool quit = false;

    void load() {
        const auto start = timer.now();
        filename = "todo.diff"; // [TODO] File picker.
        if (auto in = std::ifstream(filename)) {
            stack.set_cursor(0);
            stack.get_text() = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            stack.set_modified();
            const auto time = timer.duration(start);
            notification = std::string("load " + filename + " in ") + std::to_string((unsigned)(time * 1000.0f)) + "us";
        }
    }

    void save() {
        const auto start = timer.now();
        if (auto out = std::ofstream(filename)) {
            out << stack.get_text();
            const auto time = timer.duration(start);
            notification = std::string("save " + filename + " in ") + std::to_string((unsigned)(time * 1000.0f)) + "us";
        }
    }

    void accumulate_number(WPARAM key) {
        verify(key >= '0' && key <= '9');
        const auto digit = (unsigned)key - (unsigned)'0';
        accu *= 10;
        accu += digit;
    }

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
            clipboard = stack.get_text().substr(stack.get_cursor(), 1);
            stack.get_text().erase(stack.get_cursor(), 1);
            stack.set_cursor(stack.get_text().size() > 0 && stack.get_cursor() == stack.get_text().size() ? stack.get_cursor() - 1 : stack.get_cursor());
            stack.set_modified();
        }
    }

    void erase_line() {
        if (stack.get_text().size() > 0) {
            Line current(stack.get_text(), stack.get_cursor());
            clipboard = stack.get_text().substr(current.begin(), current.end() - current.begin() + 1);
            stack.get_text().erase(current.begin(), current.end() - current.begin() + 1);
            stack.set_cursor(std::min(current.begin(), stack.get_text().size() - 1));
            stack.set_modified();
        }
    }

    void next_char() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(std::clamp(stack.get_cursor() + 1, current.begin(), current.end()));
    }

    void prev_char() {
        Line current(stack.get_text(), stack.get_cursor());
        stack.set_cursor(std::clamp(stack.get_cursor() > 0 ? stack.get_cursor() - 1 : 0, current.begin(), current.end()));
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
            if (!is_line_whitespace(stack.get_text()[stack.get_cursor()]))
                break;
            next_char();
        }
    }

    void next_word() {
        while (stack.get_cursor() < stack.get_text().size() - 1) {
            if (is_whitespace(stack.get_text()[stack.get_cursor()])) break;
            stack.set_cursor(stack.get_cursor() + 1);
        }
        while (stack.get_cursor() < stack.get_text().size() - 1) {
            if (!is_whitespace(stack.get_text()[stack.get_cursor()])) break;
            stack.set_cursor(stack.get_cursor() + 1);
        }
    }

    void prev_word() {
        while (stack.get_cursor() > 0) {
            if (is_whitespace(stack.get_text()[stack.get_cursor()])) break;
            stack.set_cursor(stack.get_cursor() - 1);
        }
        while (stack.get_cursor() > 0) {
            if (!is_whitespace(stack.get_text()[stack.get_cursor()])) break;
            stack.set_cursor(stack.get_cursor() - 1);
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

    void jump_down(unsigned skip) {
        for (unsigned i = 0; i < skip; i++) { next_line(); }
    }

    void jump_up(unsigned skip) {
        for (unsigned i = 0; i < skip; i++) { prev_line(); }
    }

    void window_down(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        const unsigned down_row = cursor_row + row_count / 2;
        const unsigned skip = down_row > cursor_row ? down_row - cursor_row : 0;
        for (unsigned i = 0; i < skip; i++) { next_line(); }
    }

    void window_up(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        const unsigned up_row = cursor_row > row_count / 2 ? cursor_row - row_count / 2 : 0;
        const unsigned skip = cursor_row - up_row;
        for (unsigned i = 0; i < skip; i++) { prev_line(); }
    }

    void window_top(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        const unsigned top_row = begin_row;
        const unsigned skip = cursor_row - top_row;
        for (unsigned i = 0; i < skip; i++) { prev_line(); }
    }

    void window_center(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        const unsigned middle_row = begin_row + row_count / 2;
        const unsigned skip = middle_row > cursor_row ? middle_row - cursor_row : cursor_row - middle_row;
        for (unsigned i = 0; i < skip; i++) { middle_row > cursor_row ? next_line() : prev_line(); }
    }

    void window_bottom(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        const unsigned bottom_row = begin_row + row_count;
        const unsigned skip = bottom_row > cursor_row ? bottom_row - cursor_row : 0;
        for (unsigned i = 0; i < skip; i++) { next_line(); }
    }

    unsigned cursor_clamp(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        begin_row = std::clamp(begin_row, cursor_row > row_count ? cursor_row - row_count : 0, cursor_row);
        return cursor_row;
    }

    void cursor_center(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        begin_row = cursor_row - row_count / 2;
    }

    void cursor_top(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        begin_row = cursor_row;
    }

    void cursor_bottom(unsigned row_count) {
        const unsigned cursor_row = (unsigned)find_line_number(stack.get_text(), stack.get_cursor());
        begin_row = cursor_row > row_count ? cursor_row - row_count : 0;
    }

    void process_replace(WPARAM key) {
        erase(); insert(std::string(1, (char)key)); prev_char(); mode = Mode::normal;
    }

    void process_insert(WPARAM key, bool released) {
        if (key == Glyph::ESC) { mode = Mode::normal; }
        else if (key == Glyph::BS) { erase_back(); }
        else if (key == Glyph::TAB) { insert("\t"); }
        else if (key == Glyph::CR) { insert("\n"); }
        else if (key == ' ') { if (!released) { insert(std::string(1, (char)key)); } }
        else { insert(std::string(1, (char)key)); }
    }

    void process_normal(WPARAM key, bool released, unsigned row_count) {
        if (key == ' ' && !released) { mode = Mode::space; }
        else if (key == 'u') { stack.set_undo(); }
        else if (key >= '0' && key <= '9') { accumulate_number(key); mode = Mode::normal_number; }
        else if (key == 'd') { mode = Mode::normal_d; }
        else if (key == 'z') { mode = Mode::normal_z; }
        else if (key == 'i') { mode = Mode::insert; }
        else if (key == 'I') { line_start_whitespace(); mode = Mode::insert; }
        else if (key == 'a') { next_char(); mode = Mode::insert; }
        else if (key == 'A') { line_end(); mode = Mode::insert; }
        else if (key == 'o') { line_end(); insert("\n"); mode = Mode::insert; }
        else if (key == 'O') { line_start(); insert("\n"); prev_line(); mode = Mode::insert; }
        else if (key == 'r') { mode = Mode::replace; }
        else if (key == 'x') { erase(); }
        else if (key == 'P') { insert(clipboard); prev_line(); }
        else if (key == 'p') { next_line(); insert(clipboard); prev_line(); }
        else if (key == '0') { line_start(); }
        else if (key == '_') { line_start_whitespace(); }
        else if (key == '$') { line_end(); }
        else if (key == 'h') { prev_char(); }
        else if (key == 'j') { next_line(); }
        else if (key == 'k') { prev_line(); }
        else if (key == 'l') { next_char(); }
        else if (key == 'b') { prev_word(); }
        else if (key == 'w') { next_word(); }
        else if (key == 'g') { buffer_start(); }
        else if (key == 'G') { buffer_end(); }
        else if (key == 'H') { window_top(row_count); }
        else if (key == 'M') { window_center(row_count); }
        else if (key == 'L') { window_bottom(row_count); }
    }

    void process_normal_number(WPARAM key) {
        if (key >= '0' && key <= '9') { accumulate_number(key); }
        else if (key == 'j') { jump_down(accu); accu = 0; mode = Mode::normal; }
        else if (key == 'k') { jump_up(accu); accu = 0; mode = Mode::normal; }
        else if (key == 'g') { buffer_start(); jump_down(accu); accu = 0; mode = Mode::normal; }
        else { accu = 0; mode = Mode::normal; }
    }

    void process_normal_z(WPARAM key, unsigned row_count) {
        if (key == 'z') { cursor_center(row_count); mode = Mode::normal; }
        else if (key == 't') { cursor_top(row_count); mode = Mode::normal; }
        else if (key == 'b') { cursor_bottom(row_count); mode = Mode::normal; }
        else { mode = Mode::normal; }
    }

    void process_normal_d(WPARAM key) {
        if (key == 'd') { erase_line(); mode = Mode::normal; }
        else { mode = Mode::normal; }
    }

    void process_space(WPARAM key, bool released, unsigned row_count) {
        if (key == 'q') { quit = true; }
        else if (key == 'e') { load(); mode = Mode::normal; }
        else if (key == 's') { save();  mode = Mode::normal; }
        else if (key == 'o') { window_up(row_count); }
        else if (key == 'i') { window_down(row_count); }
        else if (key == ' ') { if (released) { mode = Mode::normal; } }
        else { mode = Mode::normal; }
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

    void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) {
        for (unsigned i = 0; i < col_count - 6; ++i) {
            characters.emplace_back(Glyph::BLOCK, cursor_line_color, row, 6 + i);
        }
    }

    void push_cursor(Characters& characters, unsigned row, unsigned col) {
        characters.emplace_back(
            mode == Mode::insert ? Glyph::LINE :
            mode == Mode::replace ? Glyph::BOTTOM_BLOCK :
            Glyph::BLOCK,
            cursor_color, row, col);
    };

    void push_return(Characters& characters, unsigned row, unsigned col) {
        characters.emplace_back(Glyph::RETURN, whitespace_color, row, col);
    };

    void push_tab(Characters& characters, unsigned row, unsigned col) {
        characters.emplace_back(Glyph::TAB, whitespace_color, row, col);
    };

    void push_char(Characters& characters, unsigned row, unsigned col, char character, bool block_cursor) {
        characters.emplace_back((uint8_t)character, block_cursor && mode == Mode::normal ? text_cursor_color : text_color, row, col);
    };

    void push_text(Characters& characters, unsigned col_count, unsigned row_count) { // [TODO] Clean.
        Line cursor_line(stack.get_text(), stack.get_cursor());
        const unsigned cursor_row = cursor_clamp(row_count);
        const unsigned end_row = begin_row + row_count;
        unsigned absolute_row = 0;
        unsigned index = 0;
        unsigned row = 2;
        unsigned col = 0;
        bool new_row = true;
        for (auto& character : stack.get_text()) {
            if (absolute_row >= begin_row && absolute_row <= end_row) {
                if (index == cursor_line.begin()) { push_cursor_line(characters, row, col_count); }
                if (col == col_count) { row++; col = 0; new_row = true; }
                if (new_row) { 
                    unsigned column = absolute_row == cursor_row ? col : col + 1;
                    const unsigned line = absolute_row == cursor_row ? absolute_row :
                        absolute_row < cursor_row ? cursor_row - absolute_row :
                        absolute_row - cursor_row;
                    push_line_number(characters, row, column, line); col += 6; new_row = false; }
                if (index == stack.get_cursor()) { push_cursor(characters, row, col); }
                if (character == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; new_row = true; }
                else if (character == '\t') { push_tab(characters, row, col); col += 4; }
                else { push_char(characters, row, col, character, index == stack.get_cursor()); col++; }
            }
            else {
                if (character == '\n') { absolute_row++; }
            }
            index++;
        }
    }

    void push_special_text(Characters& characters, unsigned row, const Color& color, const std::string_view text) {
        unsigned col = 0;
        for (auto& character : text) {
            characters.emplace_back((uint8_t)character, color, row, col++);
        }
    }

    void push_special_line(Characters& characters, unsigned row, const Color& color, unsigned col_count) {
        for (unsigned i = 0; i < col_count; ++i) {
            characters.emplace_back(Glyph::BLOCK, color, row, i);
        }
    }

    void push_status_bar(Characters& characters, float process_time, float cull_time, float redraw_time, unsigned col_count) {
        push_special_line(characters, 0, status_line_color, col_count);
        push_special_text(characters, 0, status_text_color, build_status_text(process_time, cull_time, redraw_time));
    }

    void push_notification_bar(Characters& characters, unsigned col_count) {
        push_special_line(characters, 1, notification_line_color, col_count);
        push_special_text(characters, 1, notification_text_color, notification);
    }

    std::string build_status_text(float process_time, float cull_time, float redraw_time) {
        Line cursor_line(stack.get_text(), stack.get_cursor());
        const auto text_perc = std::to_string(1 + unsigned(stack.get_cursor() * 100 / stack.get_text().size())) + "%";
        const auto text_size = std::to_string(stack.get_text().size()) + " bytes";
        const auto cursor_col = std::string("col ") + std::to_string(cursor_line.to_relative(stack.get_cursor()));
        const auto cursor_row = std::string("row ") + std::to_string(find_line_number(stack.get_text(), stack.get_cursor()));
        const auto process_duration = std::string("proc ") + std::to_string((unsigned)(process_time * 1000.0f)) + "us";
        const auto cull_duration = std::string("cull ") + std::to_string((unsigned)(cull_time * 1000.0f)) + "us";
        const auto redraw_duration = std::string("draw ") + std::to_string((unsigned)(redraw_time * 1000.0f)) + "us";
        return std::string(mode_letter(mode)) + " " + filename + 
            " [" + text_perc + " " + text_size + ", " + cursor_col + ", " + cursor_row +  "]" +
            " (" + process_duration + ", " + cull_duration + ", " + redraw_duration + ")";
    }

public:
    void run(float init_time) {
        notification = std::string("init in ") + std::to_string((unsigned)(init_time * 1000.0f)) + "us";
    }

    bool process(WPARAM key, bool released, unsigned col_count, unsigned row_count) { // [TODO] Unit tests.
        stack.push();
        switch (mode) {
            case Mode::normal: process_normal(key, released, row_count); break;
            case Mode::normal_number: process_normal_number(key); break;
            case Mode::normal_d: process_normal_d(key); break;
            case Mode::normal_z: process_normal_z(key, row_count); break;
            case Mode::insert: process_insert(key, released); break;
            case Mode::replace: process_replace(key); break;
            case Mode::space: process_space(key, released, row_count); break;
        };
        stack.pop();
        verify(stack.get_cursor() <= stack.get_text().size());
        return quit;
    }

    Characters cull(float process_time, float cull_time, float redraw_time, unsigned col_count, unsigned row_count) {
        Characters characters;
        characters.reserve(256);
        push_status_bar(characters, process_time, cull_time, redraw_time, col_count);
        push_notification_bar(characters, col_count);
        push_text(characters, col_count, row_count);
        return characters;
    }
};

