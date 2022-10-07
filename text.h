#pragma once

enum class Mode {
	normal,
	normal_number,
	normal_d,
	normal_f,
	normal_F,
	normal_r,
	normal_z,
	insert,
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
	TABSIGN = 132,
};

constexpr std::string_view mode_letter(Mode mode) {
	switch (mode) {
	case Mode::normal: return "n";
	case Mode::normal_number: return "0";
	case Mode::normal_d: return "d";
	case Mode::normal_f: return "f";
	case Mode::normal_F: return "F";
	case Mode::normal_r: return "r";
	case Mode::normal_z: return "z";
	case Mode::insert: return "i";
	case Mode::space: return " ";
	}
	return "";
}

std::string timestamp() {
	const auto now = std::chrono::system_clock::now();
	const auto start_of_day = std::chrono::floor<std::chrono::days>(now);
	const auto time_since_start_of_day = std::chrono::round<std::chrono::seconds>(now - start_of_day);
	const std::chrono::hh_mm_ss hms{ time_since_start_of_day };
	return std::format("{}", hms);
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

class State {
	std::string text;
	size_t cursor = 0;
	unsigned begin_row = 0;
	bool modified = true;

public:
	const std::string& get_text() { return text; }
	void set_text(const std::string& t) { text = t; }

	size_t get_cursor() const { return cursor; }
	void set_cursor(size_t u) { cursor = u; }

	unsigned get_begin_row() const { return begin_row; }

	bool is_modified() const { return modified; }
	void set_modified(bool b) { modified = b; };

	bool test(size_t index, const std::string_view s) {
		if (index + s.size() <= text.size())
			return text.substr(index, s.size()) == s;
		return false;
	}

	size_t find_line_number() const {
		size_t number = 0;
		size_t index = 0;
		while (index < text.size() && index < cursor) {
			if (text[index++] == '\n')
				number++;
		}
		return number;
	}

	void line_find(WPARAM key) {
		Line current(text, cursor);
		size_t pos = text[cursor] == key ? cursor + 1 : cursor;
		bool found = false;
		while (pos < current.end()) {
			if (text[pos] == key) { found = true; break; }
			pos++;
		}
		if (found) { cursor = pos; }
	}

	void line_rfind(WPARAM key) {
		Line current(text, cursor);
		size_t pos = text[cursor] == key && cursor > current.begin() ? cursor - 1 : cursor;
		bool found = false;
		while (pos >= current.begin()) {
			if (text[pos] == key) { found = true; break; }
			if (pos > current.begin()) { pos--; }
			else { break; }
		}
		if (found) { cursor = pos; }
	}

	void next_char() {
		Line current(text, cursor);
		cursor = std::clamp(cursor + 1, current.begin(), current.end());
	}

	void prev_char() {
		Line current(text, cursor);
		cursor = std::clamp(cursor > 0 ? cursor - 1 : 0, current.begin(), current.end());
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
			if (!is_line_whitespace(text[cursor]))
				break;
			next_char();
		}
	}

	void skip_whitespace_forward() {
		while (cursor < text.size() - 1) {
			if (!is_whitespace(text[cursor])) break;
			cursor = cursor + 1;
		}
	}

	void skip_non_whitespace_forward() {
		while (cursor < text.size() - 1) {
			if (is_whitespace(text[cursor])) break;
			cursor = cursor + 1;
		}
	}

	void skip_whitespace_backward() {
		while (cursor > 0) {
			if (!is_whitespace(text[cursor])) break;
			cursor = cursor - 1;
		}
	}

	void skip_non_whitespace_backward() {
		while (cursor > 0) {
			if (is_whitespace(text[cursor])) break;
			cursor = cursor - 1;
		}
	}

	void next_word() {
		if (is_whitespace(text[cursor])) {
			skip_whitespace_forward();
		}
		else {
			skip_non_whitespace_forward();
			skip_whitespace_forward();
		}
	}

	void prev_word() {
		if (is_whitespace(text[cursor])) {
			skip_whitespace_backward();
			skip_non_whitespace_backward();
		}
		else {
			skip_non_whitespace_backward();
		}
	}

	bool is_first_line() {
		Line current(text, cursor);
		return current.begin() == 0;
	}

	void next_line() {
		Line current(text, cursor);
		Line next(text, current.end() < text.size() - 1 ? current.end() + 1 : current.end());
		cursor = next.to_absolute(current.to_relative(cursor));
	}

	void prev_line() {
		Line current(text, cursor);
		Line prev(text, current.begin() > 0 ? current.begin() - 1 : 0);
		cursor = prev.to_absolute(current.to_relative(cursor));
	}

	void buffer_end() {
		Line current(text, cursor);
		Line last(text, text.size() - 1);
		cursor = last.to_absolute(current.to_relative(cursor));
	}

	void buffer_start() {
		Line current(text, cursor);
		Line first(text, 0);
		cursor = first.to_absolute(current.to_relative(cursor));
	}

	void jump_down(unsigned skip) {
		for (unsigned i = 0; i < skip; i++) { next_line(); }
	}

	void jump_up(unsigned skip) {
		for (unsigned i = 0; i < skip; i++) { prev_line(); }
	}

	void window_down(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		const unsigned down_row = cursor_row + row_count / 2;
		const unsigned skip = down_row > cursor_row ? down_row - cursor_row : 0;
		for (unsigned i = 0; i < skip; i++) { next_line(); }
	}

	void window_up(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		const unsigned up_row = cursor_row > row_count / 2 ? cursor_row - row_count / 2 : 0;
		const unsigned skip = cursor_row - up_row;
		for (unsigned i = 0; i < skip; i++) { prev_line(); }
	}

	void window_top(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		const unsigned top_row = begin_row;
		const unsigned skip = cursor_row - top_row;
		for (unsigned i = 0; i < skip; i++) { prev_line(); }
	}

	void window_center(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		const unsigned middle_row = begin_row + row_count / 2;
		const unsigned skip = middle_row > cursor_row ? middle_row - cursor_row : cursor_row - middle_row;
		for (unsigned i = 0; i < skip; i++) { middle_row > cursor_row ? next_line() : prev_line(); }
	}

	void window_bottom(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		const unsigned bottom_row = begin_row + row_count;
		const unsigned skip = bottom_row > cursor_row ? bottom_row - cursor_row : 0;
		for (unsigned i = 0; i < skip; i++) { next_line(); }
	}

	unsigned cursor_clamp(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		begin_row = std::clamp(begin_row, cursor_row > row_count ? cursor_row - row_count : 0, cursor_row);
		return cursor_row;
	}

	void cursor_center(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		begin_row = cursor_row - row_count / 2;
	}

	void cursor_top(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		begin_row = cursor_row;
	}

	void cursor_bottom(unsigned row_count) {
		const unsigned cursor_row = (unsigned)find_line_number();
		begin_row = cursor_row > row_count ? cursor_row - row_count : 0;
	}

	void remove_line_whitespace() {
		if (text.size() > 0) {
			while (is_line_whitespace(text[cursor])) {
				text.erase(cursor, 1);
			}
		}
	}

	void insert(std::string_view s) {
		text.insert(cursor, s);
		cursor = std::min(cursor + s.length(), text.size() - 1);
		modified = true;
	}

	void erase_back() {
		if (cursor > 0) {
			text.erase(cursor - 1, 1);
			cursor = cursor > 0 ? cursor - 1 : cursor;
			modified = true;
		}
	}

	std::string erase() {
		if (text.size() > 0) {
			const auto s = text.substr(cursor, 1);
			text.erase(cursor, 1);
			cursor = text.size() > 0 && cursor == text.size() ? cursor - 1 : cursor;
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_if(char c) {
		if (text.size() > 0 && text[cursor] == c) {
			const auto s = text.substr(cursor, 1);
			text.erase(cursor, 1);
			cursor = text.size() > 0 && cursor == text.size() ? cursor - 1 : cursor;
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_all_up() {
		if (text.size() > 0) {
			const auto s = text.substr(0, cursor);
			text.erase(0, cursor);
			cursor = 0;
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_all_down() {
		if (text.size() > 0) {
			const auto s = text.substr(cursor, text.size() - cursor);
			text.erase(cursor, text.size() - cursor);
			cursor = std::min(cursor, text.size() - 1);
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_line() {
		if (text.size() > 0) {
			Line current(text, cursor);
			const auto s = text.substr(current.begin(), current.end() - current.begin() + 1);
			text.erase(current.begin(), current.end() - current.begin() + 1);
			cursor = std::min(current.begin(), text.size() - 1);
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_line_contents() {
		if (text.size() > 0) {
			Line current(text, cursor);
			const auto s = text.substr(current.begin(), current.end() - current.begin());
			text.erase(current.begin(), current.end() - current.begin());
			cursor = std::min(current.begin(), text.size() - 1);
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_to_line_end() {
		if (text.size() > 0) {
			Line current(text, cursor);
			const auto s = text.substr(cursor, current.end() - cursor);
			text.erase(cursor, current.end() - cursor);
			cursor = std::min(cursor, text.size() - 1);
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_lines_down(unsigned count) {
		std::string s;
		for (unsigned i = 0; i <= count; i++) {
			s += erase_line();
		}
		return s;
	}

	std::string erase_lines_up(unsigned count) {
		std::string s;
		bool first_line = false;
		for (unsigned i = 0; i <= count; i++) {
			if (first_line) break; // Don't erase twice.
			first_line = is_first_line();
			s.insert(0, erase_line());
			prev_line();
		}
		return s;
	}

	std::string erase_word() {
		if (text.size() > 0) {
			Line current(text, cursor);
			const size_t begin = cursor;
			size_t end = begin;
			while (end < current.end()) {
				if (is_whitespace(text[end])) break;
				end++;
			}
			while (end < current.end()) {
				if (!is_whitespace(text[end])) break;
				end++;
			}
			const auto s = text.substr(begin, end - begin);
			text.erase(begin, end - begin);
			modified = true;
			return s;
		}
		return {};
	}

	std::string erase_words(unsigned count) {
		std::string s;
		for (unsigned i = 0; i < count; i++) {
			s += erase_word();
		}
		return s;
	}

	void fix_eof() {
		const auto size = text.size();
		if (size == 0 || (size > 0 && text[size - 1] != '\n')) {
			text += '\n';
		}
	}
};

class Stack {
	std::vector<State> states;
	bool undo = false;

public:
	Stack() {
		reset();
	}

	void reset() {
		states.clear();
		states.emplace_back();
		states.back().fix_eof();
	}

	State& state() { return states.back(); }

	const std::string& get_text() { return states.back().get_text(); }
	void set_text(const std::string& t) { states.back().set_text(t); }

	size_t get_cursor() const { return states.back().get_cursor(); }
	void set_cursor(size_t u) { states.back().set_cursor(u); }

	void set_modified(bool b) { states.back().set_modified(b); };
	void set_undo() { undo = true; }

	void push() {
		if (states.size() > 100) { states.erase(states.begin()); }
		if (states.size() > 0) { states.push_back(states.back()); }
	}

	void pop() {
		if (!states.back().is_modified() && states.size() > 1) { std::swap(states[states.size() - 2], states[states.size() - 1]); states.pop_back(); }
		if (undo && states.size() > 1) { states.pop_back(); undo = false; }
		states.back().fix_eof();
		states.back().set_modified(false);
	}
};

class Buffer {
	Stack stack;
	Timer timer;

	Mode mode = Mode::normal;

	std::string filename;
	std::string notification;
	std::string clipboard;

	unsigned accu = 0;

	bool quit = false;

	std::string close() {
		if (!filename.empty()) {
			stack.reset();
			const auto res = std::string("close ") + filename;
			return res;
		}
		return {};
	}

	std::string load() {
		if (!filename.empty()) {
			const auto start = timer.now();
			if (auto in = std::ifstream(filename)) {
				stack.set_cursor(0);
				stack.set_text(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
				stack.set_modified(true);
				const auto time = timer.duration(start);
				return std::string("load " + filename + " in ") + std::to_string((unsigned)(time * 1000.0f)) + "us";
			}
		}
		return {};
	}

	std::string save() {
		if (!filename.empty()) {
			const auto start = timer.now();
			if (auto out = std::ofstream(filename)) {
				out << stack.get_text();
				const auto time = timer.duration(start);
				return std::string("save " + filename + " in ") + std::to_string((unsigned)(time * 1000.0f)) + "us";
			}
		}
		return {};
	}

	void accumulate(WPARAM key) {
		verify(key >= '0' && key <= '9');
		const auto digit = (unsigned)key - (unsigned)'0';
		accu *= 10;
		accu += digit;
	}

	std::string clip(const std::string& s) {
		clipboard = s;
		notify(std::string("clipboard: ") + s);
		return s;
	}

	void paste_before() {
		if (clipboard.find('\n') != std::string::npos) {
			state().insert(clipboard); state().prev_line();
		}
		else {
			state().insert(clipboard);
		}
	}

	void paste_after() {
		if (clipboard.find('\n') != std::string::npos) {
			state().next_line(); state().insert(clipboard); state().prev_line();
		}
		else {
			state().next_char(); state().insert(clipboard);
		}
	}

	void process_normal_r(WPARAM key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { clip(state().erase()); state().insert(std::string(1, (char)key)); state().prev_char(); mode = Mode::normal; }
	}

	void process_insert(WPARAM key, bool released) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else if (key == Glyph::BS) { state().erase_back(); }
		else if (key == Glyph::TAB) { state().insert("\t"); }
		else if (key == Glyph::CR) { state().insert("\n"); }
		else if (key == ' ') { if (!released) { state().insert(std::string(1, (char)key)); } }
		else { state().insert(std::string(1, (char)key)); }
	}

	void process_normal(WPARAM key, bool released, unsigned row_count) {
		if (key == ' ' && !released) { mode = Mode::space; }
		else if (key == 'u') { stack.set_undo(); }
		else if (key >= '0' && key <= '9') { accumulate(key); mode = Mode::normal_number; }
		else if (key == 'd') { mode = Mode::normal_d; }
		else if (key == 'c') {} // [TODO] Change mode (cc, cw, cb, cNj/cNk).
		else if (key == 'y') {} // [TODO] Yank mode (yy, yw, yb, yNj/yNk).
		else if (key == 'z') { mode = Mode::normal_z; }
		else if (key == 'i') { mode = Mode::insert; }
		else if (key == 'I') { state().line_start_whitespace(); mode = Mode::insert; }
		else if (key == 'a') { state().next_char(); mode = Mode::insert; }
		else if (key == 'A') { state().line_end(); mode = Mode::insert; }
		else if (key == 'o') { state().line_end(); state().insert("\n"); mode = Mode::insert; }
		else if (key == 'O') { state().line_start(); state().insert("\n"); state().prev_line(); mode = Mode::insert; }
		else if (key == 'r') { mode = Mode::normal_r; }
		else if (key == 'f') { mode = Mode::normal_f; }
		else if (key == 'F') { mode = Mode::normal_F; }
		else if (key == 'x') { clip(state().erase()); }
		else if (key == 's') { state().erase(); mode = Mode::insert; }
		else if (key == 'S') { state().erase_line_contents(); mode = Mode::insert; }
		else if (key == 'C') { state().erase_to_line_end(); mode = Mode::insert; }
		else if (key == 'D') { state().erase_to_line_end(); }
		else if (key == 'P') { paste_before(); }
		else if (key == 'p') { paste_after(); }
		else if (key == '0') { state().line_start(); }
		else if (key == '_') { state().line_start_whitespace(); }
		else if (key == '$') { state().line_end(); }
		else if (key == 'h') { state().prev_char(); }
		else if (key == 'j') { state().next_line(); }
		else if (key == 'k') { state().prev_line(); }
		else if (key == 'l') { state().next_char(); }
		else if (key == 'b') { state().prev_word(); }
		else if (key == 'w') { state().next_word(); }
		else if (key == 'g') { state().buffer_start(); }
		else if (key == 'G') { state().buffer_end(); }
		else if (key == 'H') { state().window_top(row_count); }
		else if (key == 'M') { state().window_center(row_count); }
		else if (key == 'L') { state().window_bottom(row_count); }
		else if (key == 'J') { state().line_end(); state().erase(); state().remove_line_whitespace(); state().insert(" "); state().prev_char(); }
		else if (key == '<') { state().line_start(); state().erase_if('\t'); state().line_start_whitespace(); }
		else if (key == '>') { state().line_start_whitespace(); state().insert("\t"); }
		else if (key == ';') {} // [TODO] Re-find.
		else if (key == '/') {} // [TODO] Find.
		else if (key == '?') {} // [TODO] Reverse find.
		else if (key == '*') {} // [TODO] Find under cursor.
		else if (key == '.') {} // [TODO] Repeat command.
	}

	void process_normal_number(WPARAM key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'j') { state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { state().jump_up(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { state().buffer_start(); state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_f(WPARAM key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { state().line_find(key); mode = Mode::normal; }
	}

	void process_normal_F(WPARAM key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { state().line_rfind(key); mode = Mode::normal; }
	}

	void process_normal_z(WPARAM key, unsigned row_count) {
		if (key == 'z') { state().cursor_center(row_count); mode = Mode::normal; }
		else if (key == 't') { state().cursor_top(row_count); mode = Mode::normal; }
		else if (key == 'b') { state().cursor_bottom(row_count); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_d(WPARAM key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'd') { clip(state().erase_line()); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { clip(state().erase_words(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { clip(state().erase_all_up()); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { clip(state().erase_all_down()); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { clip(state().erase_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { clip(state().erase_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { } // [TODO] Delete until char.
		else if (key == 't') { } // [TODO] Delete to char.
		else if (key == 'i') { } // [TODO] di mode.
		else if (key == 'a') { } // [TODO] da mode.
		else { mode = Mode::normal; }
	}

	void process_space(WPARAM key, bool released, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'w') { notify(close()); }
		else if (key == 'e') { notify(load()); }
		else if (key == 's') { notify(save()); }
		else if (key == 'o') { state().window_up(row_count); }
		else if (key == 'i') { state().window_down(row_count); }
		else if (key == ' ') { if (released) { mode = Mode::normal; } }
	}

public:
	Buffer(const std::string_view filename)
		: filename(filename) {
	}

	bool process(WPARAM key, bool released, unsigned col_count, unsigned row_count) {
		stack.push();
		switch (mode) {
		case Mode::normal: process_normal(key, released, row_count); break;
		case Mode::normal_number: process_normal_number(key); break;
		case Mode::normal_d: process_normal_d(key); break;
		case Mode::normal_f: process_normal_f(key); break;
		case Mode::normal_F: process_normal_F(key); break;
		case Mode::normal_r: process_normal_r(key); break;
		case Mode::normal_z: process_normal_z(key, row_count); break;
		case Mode::insert: process_insert(key, released); break;
		case Mode::space: process_space(key, released, row_count); break;
		};
		stack.pop();
		verify(stack.get_cursor() <= stack.get_text().size());
		return quit;
	}

	std::string build_status_text() {
		const auto text_perc = std::to_string(1 + unsigned(stack.get_cursor() * 100 / stack.get_text().size())) + "%";
		const auto text_size = std::to_string(stack.get_text().size()) + " bytes";
		return filename + " [" + text_perc + " " + text_size + "]";
	}

	void notify(const std::string& s) {
		notification = timestamp() + "  " + s;
	}

	State& state() { return stack.state(); }

	std::string_view get_notification() const { return notification; }
	Mode get_mode() const { return mode; }
};

class Manager {
	Buffer buffer;

	Color mode_text_color = Color::rgba(255, 155, 155, 255);
	Color status_line_color = Color::rgba(1, 22, 39, 255);
	Color status_text_color = Color::rgba(155, 155, 155, 255);
	Color notification_line_color = Color::rgba(1, 22, 39, 255);
	Color notification_text_color = Color::rgba(64, 152, 179, 255);
	Color cursor_color = Color::rgba(255, 255, 0, 255);
	Color cursor_line_color = Color::rgba(65, 80, 29, 255);
	Color whitespace_color = Color::rgba(75, 100, 93, 255);
	Color text_color = Color::rgba(205, 226, 239, 255);
	Color text_cursor_color = Color::rgba(5, 5, 5, 255);
	Color line_number_color = Color::rgba(75, 100, 121, 255);
	Color diff_note_color = Color::rgba(255, 192, 0, 255);
	Color diff_add_color = Color::rgba(0, 192, 0, 255);
	Color diff_remove_color = Color::rgba(192, 0, 0, 255);

	void push_digit(Characters& characters, unsigned row, unsigned col, unsigned digit) {
		characters.emplace_back((uint8_t)(48 + digit), line_number_color, row, col);
	}

	void push_line_number(Characters& characters, unsigned row, unsigned col, unsigned line) {
		if (line > 999) { push_digit(characters, row, col + 0, (line % 10000) / 1000); }
		if (line > 99) { push_digit(characters, row, col + 1, (line % 1000) / 100); }
		if (line > 9) { push_digit(characters, row, col + 2, (line % 100) / 10); }
		push_digit(characters, row, col + 3, line % 10);
	}

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) {
		for (unsigned i = 0; i < col_count - 5; ++i) {
			characters.emplace_back(Glyph::BLOCK, cursor_line_color, row, 7 + i);
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col) {
		characters.emplace_back(
			buffer.get_mode() == Mode::insert ? Glyph::LINE :
			buffer.get_mode() == Mode::normal ? Glyph::BLOCK :
			Glyph::BOTTOM_BLOCK,
			cursor_color, row, col);
	};

	void push_return(Characters& characters, unsigned row, unsigned col) {
		characters.emplace_back(Glyph::RETURN, whitespace_color, row, col);
	};

	void push_tab(Characters& characters, unsigned row, unsigned col) {
		characters.emplace_back(Glyph::TABSIGN, whitespace_color, row, col);
	};

	void push_char(Characters& characters, unsigned row, unsigned col, char character, bool block_cursor, const Color& row_color) {
		characters.emplace_back((uint8_t)character, block_cursor && buffer.get_mode() == Mode::normal ? text_cursor_color : row_color, row, col);
	};

	void push_text(Characters& characters, unsigned col_count, unsigned row_count) { // [TODO] Clean.
		Color row_color = text_color;
		const unsigned cursor_row = buffer.state().cursor_clamp(row_count);
		const unsigned end_row = buffer.state().get_begin_row() + row_count;
		unsigned absolute_row = 0;
		unsigned index = 0;
		unsigned row = 2;
		unsigned col = 0;
		bool new_row = true;
		for (auto& character : buffer.state().get_text()) {
			if (absolute_row >= buffer.state().get_begin_row() && absolute_row <= end_row) {
				if (new_row) {
					if (absolute_row == cursor_row) { push_cursor_line(characters, row, col_count); }
					if (buffer.state().test(index, "---")) { row_color = diff_note_color; } // [TODO] Better syntax highlighting.
					else if (character == '+') { row_color = diff_add_color; }
					else if (character == '-') { row_color = diff_remove_color; }
					else { row_color = text_color; }
					unsigned column = absolute_row == cursor_row ? col : col + 1;
					const unsigned line = absolute_row == cursor_row ? absolute_row :
						absolute_row < cursor_row ? cursor_row - absolute_row :
						absolute_row - cursor_row;
					push_line_number(characters, row, column, line); col += 7; new_row = false;
				}
				if (index == buffer.state().get_cursor()) { push_cursor(characters, row, col); }
				if (character == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; new_row = true; }
				else if (character == '\t') { push_tab(characters, row, col); col += 4; }
				else { push_char(characters, row, col, character, index == buffer.state().get_cursor(), row_color); col++; }
			}
			else {
				if (character == '\n') { absolute_row++; }
			}
			index++;
		}
	}

	void push_special_text(Characters& characters, unsigned row, unsigned col, const Color& color, const std::string_view text) {
		unsigned offset = 0;
		for (auto& character : text) {
			characters.emplace_back((uint8_t)character, color, row, col + offset++);
		}
	}

	void push_special_line(Characters& characters, unsigned row, const Color& color, unsigned col_count) {
		for (unsigned i = 0; i < col_count; ++i) {
			characters.emplace_back(Glyph::BLOCK, color, row, i);
		}
	}

	void push_status_bar(Characters& characters, float process_time, float cull_time, float redraw_time, unsigned col_count) {
		push_special_line(characters, 0, status_line_color, col_count);
		push_special_text(characters, 0, 0, mode_text_color, std::string(mode_letter(buffer.get_mode())) + " ");
		push_special_text(characters, 0, 2, status_text_color, build_status_text(process_time, cull_time, redraw_time));
	}

	void push_notification_bar(Characters& characters, unsigned col_count) {
		push_special_line(characters, 1, notification_line_color, col_count);
		push_special_text(characters, 1, 0, notification_text_color, buffer.get_notification());
	}

	std::string build_status_text(float process_time, float cull_time, float redraw_time) {
		const auto process_duration = std::string("proc ") + std::to_string((unsigned)(process_time * 1000.0f)) + "us";
		const auto cull_duration = std::string("cull ") + std::to_string((unsigned)(cull_time * 1000.0f)) + "us";
		const auto redraw_duration = std::string("draw ") + std::to_string((unsigned)(redraw_time * 1000.0f)) + "us";
		return buffer.build_status_text() + " (" + process_duration + ", " + cull_duration + ", " + redraw_duration + ")";
	}

public:
	Manager()
		: buffer("todo.diff") { // [TODO] File picker.
	}

	void run(float init_time) {
		buffer.notify(std::string("init in ") + std::to_string((unsigned)(init_time * 1000.0f)) + "us");
	}

	bool process(WPARAM key, bool released, unsigned col_count, unsigned row_count) {
		return buffer.process(key, released, col_count, row_count);
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

