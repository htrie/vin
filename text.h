#pragma once

enum class Mode {
	normal,
	normal_number,
	normal_c,
	normal_cf,
	normal_ct,
	normal_ci,
	normal_ca,
	normal_d,
	normal_df,
	normal_dt,
	normal_di,
	normal_da,
	normal_f,
	normal_F,
	normal_r,
	normal_y,
	normal_yf,
	normal_yt,
	normal_yi,
	normal_ya,
	normal_z,
	insert,
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
	const std::string& get_text() const { return text; }
	void set_text(const std::string& t) { text = t; }

	size_t get_cursor() const { return cursor; }
	void set_cursor(size_t u) { cursor = u; }

	unsigned get_begin_row() const { return begin_row; }

	bool is_modified() const { return modified; }
	void set_modified(bool b) { modified = b; };

	bool test(size_t index, const std::string_view s) const {
		if (index + s.size() <= text.size())
			return text.substr(index, s.size()) == s;
		return false;
	}

	unsigned find_cursor_row() const {
		unsigned number = 0;
		size_t index = 0;
		while (index < text.size() && index < cursor) {
			if (text[index++] == '\n')
				number++;
		}
		return number;
	}

	size_t find_char(unsigned key) {
		if (text.size() > 0) {
			Line current(text, cursor);
			size_t pos = text[cursor] == key ? cursor + 1 : cursor;
			bool found = false;
			while (pos < current.end()) {
				if (text[pos] == key) { found = true; break; }
				pos++;
			}
			if (found) return pos;
		}
		return std::string::npos;
	}

	size_t rfind_char(unsigned key) {
		if (text.size() > 0) {
			Line current(text, cursor);
			size_t pos = text[cursor] == key && cursor > current.begin() ? cursor - 1 : cursor;
			bool found = false;
			while (pos >= current.begin()) {
				if (text[pos] == key) { found = true; break; }
				if (pos > current.begin()) { pos--; }
				else { break; }
			}
			if (found) return pos;
		}
		return std::string::npos;
	}

	void line_find(unsigned key) {
		if (const auto pos = find_char(key); pos != std::string::npos) {
			cursor = pos;
		}
	}

	void line_rfind(unsigned key) {
		if (const auto pos = rfind_char(key); pos != std::string::npos) {
			cursor = pos;
		}
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

	void next_word() { // [TODO] Better word detection (like Line).
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
		const unsigned cursor_row = find_cursor_row();
		const unsigned down_row = cursor_row + row_count / 2;
		const unsigned skip = down_row > cursor_row ? down_row - cursor_row : 0;
		for (unsigned i = 0; i < skip; i++) { next_line(); }
	}

	void window_up(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned up_row = cursor_row > row_count / 2 ? cursor_row - row_count / 2 : 0;
		const unsigned skip = cursor_row - up_row;
		for (unsigned i = 0; i < skip; i++) { prev_line(); }
	}

	void window_top(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned top_row = begin_row;
		const unsigned skip = cursor_row - top_row;
		for (unsigned i = 0; i < skip; i++) { prev_line(); }
	}

	void window_center(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned middle_row = begin_row + row_count / 2;
		const unsigned skip = middle_row > cursor_row ? middle_row - cursor_row : cursor_row - middle_row;
		for (unsigned i = 0; i < skip; i++) { middle_row > cursor_row ? next_line() : prev_line(); }
	}

	void window_bottom(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned bottom_row = begin_row + row_count;
		const unsigned skip = bottom_row > cursor_row ? bottom_row - cursor_row : 0;
		for (unsigned i = 0; i < skip; i++) { next_line(); }
	}

	void cursor_clamp(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		begin_row = std::clamp(begin_row, cursor_row > row_count ? cursor_row - row_count : 0, cursor_row);
	}

	void cursor_center(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		begin_row = cursor_row - row_count / 2;
	}

	void cursor_top(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		begin_row = cursor_row;
	}

	void cursor_bottom(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		begin_row = cursor_row > row_count ? cursor_row - row_count : 0;
	}

	void remove_line_whitespace() {
		if (text.size() > 0) {
			while (is_line_whitespace(text[cursor])) {
				text.erase(cursor, 1);
			}
		}
	}

	std::string yank_to(unsigned key) {
		if (text.size() > 0) {
			if (const auto pos = find_char(key); pos != std::string::npos) {
				return text.substr(cursor, pos - cursor + 1);
			}
		}
		return {};
	}

	std::string yank_until(unsigned key) {
		if (text.size() > 0) {
			if (const auto pos = find_char(key); pos != std::string::npos) {
				return text.substr(cursor, pos - cursor);
			}
		}
		return {};
	}

	std::string yank_line() {
		if (text.size() > 0) {
			Line current(text, cursor);
			return text.substr(current.begin(), current.end() - current.begin() + 1);
		}
		return {};
	}

	std::string yank_all_up() {
		if (text.size() > 0) {
			return text.substr(0, cursor);
		}
		return {};
	}

	std::string yank_all_down() {
		if (text.size() > 0) {
			return text.substr(cursor, text.size() - cursor);
		}
		return {};
	}

	std::string yank_words(unsigned count) {
		std::string s;
		if (text.size() > 0) {
			Line current(text, cursor);
			const size_t begin = cursor;
			size_t end = begin;
			for (unsigned i = 0; i < count; i++) {
				while (end < current.end()) {
					if (is_whitespace(text[end])) break;
					end++;
				}
				while (end < current.end()) {
					if (!is_whitespace(text[end])) break;
					end++;
				}
			}
			s += text.substr(begin, end - begin);
		}
		return s;
	}

	std::string yank_lines_down(unsigned count) {
		std::string s;
		if (text.size() > 0) {
			size_t begin = cursor;
			for (unsigned i = 0; i <= count; i++) {
				if (begin < text.size() - 1) {
					Line current(text, begin);
					s += text.substr(current.begin(), current.end() - current.begin() + 1);
					begin = current.end() + 1;
				}
			}
		}
		return s;
	}

	std::string yank_lines_up(unsigned count) {
		std::string s;
		if (text.size() > 0) {
			size_t begin = cursor;
			for (unsigned i = 0; i <= count; i++) {
				if (begin < text.size() - 1) {
					Line current(text, begin);
					s.insert(0, text.substr(current.begin(), current.end() - current.begin() + 1));
					if (current.begin() == 0) break;
					begin = current.begin() - 1;
				}
			}
		}
		return s;
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

	std::string erase_to(unsigned key) {
		if (text.size() > 0) {
			if (const auto pos = find_char(key); pos != std::string::npos) {
				const auto s = text.substr(cursor, pos - cursor + 1);
				text.erase(cursor, pos - cursor + 1);
				modified = true;
				return s;
			}
		}
		return {};
	}

	std::string erase_until(unsigned key) {
		if (text.size() > 0) {
			if (const auto pos = find_char(key); pos != std::string::npos) {
				const auto s = text.substr(cursor, pos - cursor);
				text.erase(cursor, pos - cursor);
				modified = true;
				return s;
			}
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

	void paste_before(const std::string_view s) {
		if (s.find('\n') != std::string::npos) {
			line_start(); insert(s); prev_line();
		}
		else {
			insert(s);
		}
	}

	void paste_after(const std::string_view s) {
		if (s.find('\n') != std::string::npos) {
			next_line(); line_start(); insert(s); prev_line();
		}
		else {
			next_char(); insert(s);
		}
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
	const State& state() const { return states.back(); }

	const std::string& get_text() const { return states.back().get_text(); }
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

	Mode mode = Mode::normal;

	std::string filename;
	std::string clipboard;

	unsigned accu = 0;

	unsigned f_key = 0;
	bool f_is_forward = false;

	void accumulate(unsigned key) {
		verify(key >= '0' && key <= '9');
		const auto digit = (unsigned)key - (unsigned)'0';
		accu *= 10;
		accu += digit;
	}

	std::string clip(const std::string& s) {
		clipboard = s;
		return s;
	}

	void process_insert(unsigned key) { // [TODO] Undo whole insert sequences.
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else if (key == Glyph::BS) { state().erase_back(); }
		else if (key == Glyph::TAB) { state().insert("\t"); }
		else if (key == Glyph::CR) { state().insert("\n"); }
		else { state().insert(std::string(1, (char)key)); }
	}

	void process_normal(unsigned key, unsigned row_count) {
		if (key == 'u') { stack.set_undo(); }
		else if (key >= '0' && key <= '9') { accumulate(key); mode = Mode::normal_number; }
		else if (key == 'c') { mode = Mode::normal_c; }
		else if (key == 'd') { mode = Mode::normal_d; }
		else if (key == 'r') { mode = Mode::normal_r; }
		else if (key == 'f') { mode = Mode::normal_f; }
		else if (key == 'F') { mode = Mode::normal_F; }
		else if (key == 'y') { mode = Mode::normal_y; }
		else if (key == 'z') { mode = Mode::normal_z; }
		else if (key == 'i') { mode = Mode::insert; }
		else if (key == 'x') { clip(state().erase()); }
		else if (key == 'I') { state().line_start_whitespace(); mode = Mode::insert; }
		else if (key == 'a') { state().next_char(); mode = Mode::insert; }
		else if (key == 'A') { state().line_end(); mode = Mode::insert; }
		else if (key == 'o') { state().line_end(); state().insert("\n"); mode = Mode::insert; }
		else if (key == 'O') { state().line_start(); state().insert("\n"); state().prev_line(); mode = Mode::insert; }
		else if (key == 's') { state().erase(); mode = Mode::insert; }
		else if (key == 'S') { clip(state().erase_line_contents()); mode = Mode::insert; }
		else if (key == 'C') { clip(state().erase_to_line_end()); mode = Mode::insert; }
		else if (key == 'D') { clip(state().erase_to_line_end()); }
		else if (key == 'P') { state().paste_before(clipboard); }
		else if (key == 'p') { state().paste_after(clipboard); }
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
		else if (key == ';') { if(f_is_forward) { state().line_find(f_key); } else { state().line_rfind(f_key); } mode = Mode::normal; }
		else if (key == ',') { if(f_is_forward) { state().line_rfind(f_key); } else { state().line_find(f_key); } mode = Mode::normal; }
		else if (key == '/') {} // [TODO] Find.
		else if (key == '?') {} // [TODO] Reverse find.
		else if (key == '*') {} // [TODO] Find under cursor.
		else if (key == '.') {} // [TODO] Repeat command.
	}

	void process_normal_number(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'j') { state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { state().jump_up(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { state().buffer_start(); state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_f(unsigned key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { state().line_find(key); f_key = key; f_is_forward = true; mode = Mode::normal; }
	}

	void process_normal_F(unsigned key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { state().line_rfind(key); f_key = key; f_is_forward = false; mode = Mode::normal; }
	}

	void process_normal_r(unsigned key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { clip(state().erase()); state().insert(std::string(1, (char)key)); state().prev_char(); mode = Mode::normal; }
	}

	void process_normal_z(unsigned key, unsigned row_count) {
		if (key == 'z') { state().cursor_center(row_count); mode = Mode::normal; }
		else if (key == 't') { state().cursor_top(row_count); mode = Mode::normal; }
		else if (key == 'b') { state().cursor_bottom(row_count); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_y(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'y') { clip(state().yank_line()); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { clip(state().yank_words(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { clip(state().yank_all_up()); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { clip(state().yank_all_down()); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { clip(state().yank_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { clip(state().yank_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { accu = 0; mode = Mode::normal_yf; }
		else if (key == 't') { accu = 0; mode = Mode::normal_yt; }
		else if (key == 'i') { accu = 0; mode = Mode::normal_yi; }
		else if (key == 'a') { accu = 0; mode = Mode::normal_ya; }
		else { mode = Mode::normal; }
	}

	void process_normal_yf(unsigned key) {
		clip(state().yank_to(key)); mode = Mode::normal;
	}

	void process_normal_yt(unsigned key) {
		clip(state().yank_until(key)); mode = Mode::normal;
	}

	void process_normal_yi(unsigned key) {
		mode = Mode::normal; // [TODO] w, [, {, (, ", '
	}

	void process_normal_ya(unsigned key) {
		mode = Mode::normal; // [TODO] w, [, {, (, ", '
	}

	void process_normal_c(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'c') { clip(state().erase_line_contents()); accu = 0; mode = Mode::insert; }
		else if (key == 'w') { clip(state().erase_words(std::max(1u, accu))); accu = 0; mode = Mode::insert; }
		else if (key == 'g') { clip(state().erase_all_up()); accu = 0; mode = Mode::insert; }
		else if (key == 'G') { clip(state().erase_all_down()); accu = 0; mode = Mode::insert; }
		else if (key == 'j') { clip(state().erase_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::insert; }
		else if (key == 'k') { clip(state().erase_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::insert; }
		else if (key == 'f') { accu = 0; mode = Mode::normal_cf; }
		else if (key == 't') { accu = 0; mode = Mode::normal_ct; }
		else if (key == 'i') { accu = 0; mode = Mode::normal_ci; }
		else if (key == 'a') { accu = 0; mode = Mode::normal_ca; }
		else { mode = Mode::normal; }
	}

	void process_normal_cf(unsigned key) {
		clip(state().erase_to(key)); mode = Mode::insert;
	}

	void process_normal_ct(unsigned key) {
		clip(state().erase_until(key)); mode = Mode::insert;
	}

	void process_normal_ci(unsigned key) {
		mode = Mode::normal; // [TODO] w, [, {, (, ", '
	}

	void process_normal_ca(unsigned key) {
		mode = Mode::normal; // [TODO] w, [, {, (, ", '
	}

	void process_normal_d(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'd') { clip(state().erase_line()); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { clip(state().erase_words(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { clip(state().erase_all_up()); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { clip(state().erase_all_down()); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { clip(state().erase_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { clip(state().erase_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { accu = 0; mode = Mode::normal_df; }
		else if (key == 't') { accu = 0; mode = Mode::normal_dt; }
		else if (key == 'i') { accu = 0; mode = Mode::normal_di; }
		else if (key == 'a') { accu = 0; mode = Mode::normal_da; }
		else { mode = Mode::normal; }
	}

	void process_normal_df(unsigned key) {
		clip(state().erase_to(key)); mode = Mode::normal;
	}

	void process_normal_dt(unsigned key) {
		clip(state().erase_until(key)); mode = Mode::normal;
	}

	void process_normal_di(unsigned key) {
		mode = Mode::normal; // [TODO] w, [, {, (, ", '
	}

	void process_normal_da(unsigned key) {
		mode = Mode::normal; // [TODO] w, [, {, (, ", '
	}

	void push_digit(Characters& characters, unsigned row, unsigned col, unsigned digit) const {
		characters.emplace_back((uint8_t)(48 + digit), colors().line_number, row, col);
	}

	void push_line_number(Characters& characters, unsigned row, unsigned col, unsigned line) const {
		if (line > 999) { push_digit(characters, row, col + 0, (line % 10000) / 1000); }
		if (line > 99) { push_digit(characters, row, col + 1, (line % 1000) / 100); }
		if (line > 9) { push_digit(characters, row, col + 2, (line % 100) / 10); }
		push_digit(characters, row, col + 3, line % 10);
	}

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) const {
		for (unsigned i = 0; i < col_count - 5; ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().cursor_line, row, 7 + i);
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(
			get_mode() == Mode::insert ? Glyph::LINE :
			get_mode() == Mode::normal ? Glyph::BLOCK :
			Glyph::BOTTOM_BLOCK,
			colors().cursor, row, col);
	};

	void push_return(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::RETURN, colors().whitespace, row, col);
	};

	void push_tab(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::TABSIGN, colors().whitespace, row, col);
	};

	void push_char(Characters& characters, unsigned row, unsigned col, char character, bool block_cursor, const Color& row_color) const {
		characters.emplace_back((uint8_t)character, block_cursor && get_mode() == Mode::normal ? colors().text_cursor : row_color, row, col);
	};

	void push_text(Characters& characters, unsigned col_count, unsigned row_count) const { // [TODO] Clean.
		Color row_color = colors().text;
		const unsigned cursor_row = state().find_cursor_row();
		const unsigned end_row = state().get_begin_row() + row_count;
		unsigned absolute_row = 0;
		unsigned index = 0;
		unsigned row = 1;
		unsigned col = 0;
		bool new_row = true;
		for (auto& character : state().get_text()) {
			if (absolute_row >= state().get_begin_row() && absolute_row <= end_row) {
				if (new_row) {
					if (absolute_row == cursor_row) { push_cursor_line(characters, row, col_count); }
					if (state().test(index, "---")) { row_color = colors().diff_note; } // [TODO] Better syntax highlighting.
					else if (character == '+') { row_color = colors().diff_add; }
					else if (character == '-') { row_color = colors().diff_remove; }
					else { row_color = colors().text; }
					unsigned column = absolute_row == cursor_row ? col : col + 1;
					const unsigned line = absolute_row == cursor_row ? absolute_row :
						absolute_row < cursor_row ? cursor_row - absolute_row :
						absolute_row - cursor_row;
					push_line_number(characters, row, column, line); col += 7; new_row = false;
				}
				if (index == state().get_cursor()) { push_cursor(characters, row, col); }
				if (character == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; new_row = true; }
				else if (character == '\t') { push_tab(characters, row, col); col += 4; }
				else { push_char(characters, row, col, character, index == state().get_cursor(), row_color); col++; }
			}
			else {
				if (character == '\n') { absolute_row++; }
			}
			index++;
		}
	}

	void init(const std::string& text) {
		stack.set_cursor(0);
		stack.set_text(text);
		stack.set_modified(true);
	}

	std::string load() {
		if (!filename.empty()) {
			if (auto in = std::ifstream(filename)) {
				return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			}
		}
		return {};
	}

public:
	Buffer() {}
	Buffer(const std::string_view filename)
		: filename(filename) {
		init(load());
	}

	void reload() {
		init(load());
	}

	void save() {
		if (!filename.empty()) {
			if (auto out = std::ofstream(filename)) {
				out << stack.get_text();
			}
		}
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		stack.push();
		switch (mode) {
		case Mode::normal: process_normal(key, row_count); break;
		case Mode::normal_number: process_normal_number(key); break;
		case Mode::normal_c: process_normal_c(key); break;
		case Mode::normal_cf: process_normal_cf(key); break;
		case Mode::normal_ct: process_normal_ct(key); break;
		case Mode::normal_ci: process_normal_ci(key); break;
		case Mode::normal_ca: process_normal_ca(key); break;
		case Mode::normal_d: process_normal_d(key); break;
		case Mode::normal_df: process_normal_df(key); break;
		case Mode::normal_dt: process_normal_dt(key); break;
		case Mode::normal_di: process_normal_di(key); break;
		case Mode::normal_da: process_normal_da(key); break;
		case Mode::normal_f: process_normal_f(key); break;
		case Mode::normal_F: process_normal_F(key); break;
		case Mode::normal_r: process_normal_r(key); break;
		case Mode::normal_y: process_normal_y(key); break;
		case Mode::normal_yf: process_normal_yf(key); break;
		case Mode::normal_yt: process_normal_yt(key); break;
		case Mode::normal_yi: process_normal_yi(key); break;
		case Mode::normal_ya: process_normal_ya(key); break;
		case Mode::normal_z: process_normal_z(key, row_count); break;
		case Mode::insert: process_insert(key); break;
		};
		stack.pop();
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		push_text(characters, col_count, row_count);
	}

	State& state() { return stack.state(); }
	const State& state() const { return stack.state(); }

	const std::string_view get_filename() const { return filename; }

	Mode get_mode() const { return mode; }

	bool is_normal() const { return mode == Mode::normal; }
};

class Bar {
	std::string notification;

	void push_special_text(Characters& characters, unsigned row, unsigned col, const Color& color, const std::string_view text) const {
		unsigned offset = 0;
		for (auto& character : text) {
			characters.emplace_back((uint8_t)character, color, row, col + offset++);
		}
	}

	void push_special_line(Characters& characters, unsigned row, const Color& color, unsigned col_count) const {
		for (unsigned i = 0; i < col_count; ++i) {
			characters.emplace_back(Glyph::BLOCK, color, row, i);
		}
	}

	void push_notification_bar(Characters& characters, const std::string_view notification, unsigned col_count) const {
		push_special_line(characters, 0, colors().notification_line, col_count);
		push_special_text(characters, 0, 0, colors().notification_text, notification);
	}

public:
	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		push_notification_bar(characters, notification, col_count);
	}

	void notify(const std::string& s) {
		notification = timestamp() + "  " + s;
	}
};

class Picker {
	std::vector<std::string> paths;
	std::string filename;

	std::vector<std::string> filtered;
	unsigned selected = 0;

	void populate_directory(const std::filesystem::path& dir) {
		for (const auto& path : std::filesystem::directory_iterator{ dir }) {
			if (path.is_directory()) {
				auto dirname = path.path().generic_string();
				if (dirname.size() > 0 && (dirname.substr(0, 3) != "./.")) { // Skip hidden directories.
					//populate_directory(path); // [TODO] Recursion seems to crash on large folders.
				}
			} else if (path.is_regular_file()) {
				auto filename = path.path().generic_string();
				if (filename.size() > 0 && (filename.substr(0, 3) != "./.")) { // Skip hidden files.
					paths.push_back(path.path().generic_string());
				}
			}
		}
	}

	void push_char(Characters& characters, unsigned row, unsigned col, char character) const {
		characters.emplace_back((uint8_t)character, colors().text, row, col);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s) const {
		for (auto& character : s) {
			push_char(characters, row, col++, character);
		}
	}

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) const {
		for (unsigned i = 0; i < col_count; ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().cursor_line, row, i);
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::LINE, colors().cursor, row, col);
	};

public:
	void reset() {
		paths.clear();
		filename.clear();
		filtered.clear();
		selected = 0;
	}

	void populate() {
		populate_directory(".");
	}

	void filter(unsigned row_count) {
		filtered.clear();
		for (auto& path : paths) {
			if (filtered.size() > row_count - 2) { break; }
			if( path.find(filename) != std::string::npos) {
				filtered.push_back(path);
			}
		}
		selected = std::min(selected, (unsigned)filtered.size() - 1);
	}

	std::string selection() {
		if (selected < filtered.size())
			return filtered[selected];
		return filename;
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == Glyph::CR) { return; }
		else if (key == Glyph::ESC) { return; }
		else if (key == Glyph::TAB) { return; } // [TODO] Auto completion.
		else if (key == Glyph::BS) { if (filename.size() > 0) { filename.pop_back(); } }
		else if (key == '<') { selected++; }
		else if (key == '>') { if (selected > 0) selected--; }
		else { filename += (char)key; }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned col = 0;
		unsigned row = 1;
		push_string(characters, row, col, "open: ");
		push_string(characters, row, col, filename);
		push_cursor(characters, row++, col);

		unsigned displayed = 0;
		for (auto& path : filtered) {
			col = 0;
			if (selected == displayed)
				push_cursor_line(characters, row, col_count);
			push_string(characters, row++, col, path);
			displayed++;
		}
	}
};

class Switcher {
	std::map<std::string, Buffer> buffers;
	std::string active;

	void select_previous() {
		if (!active.empty()) {
			if (auto found = buffers.find(active); found != buffers.end()) {
				if (auto prev = --found; prev != buffers.end()) {
					active = buffers.at((*prev).first).get_filename();
				}
			} else {
				active.clear();
			}
		} else {
			if (auto found = buffers.begin(); found != buffers.end()) {
				active = buffers.at((*found).first).get_filename();
			}
		}
	}

	void select_next() {
		if (!active.empty()) {
			if (auto found = buffers.find(active); found != buffers.end()) {
				if (auto next = ++found; next != buffers.end()) {
					active = buffers.at((*next).first).get_filename();
				}
			} else {
				active.clear();
			}
		} else {
			if (auto found = buffers.begin(); found != buffers.end()) {
				active = buffers.at((*found).first).get_filename();
			}
		}
	}

	void push_char(Characters& characters, unsigned row, unsigned col, char character) const {
		characters.emplace_back((uint8_t)character, colors().text, row, col);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s) const {
		for (auto& character : s) {
			push_char(characters, row, col++, character);
		}
	}

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) const {
		for (unsigned i = 0; i < col_count; ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().cursor_line, row, i);
		}
	}

public:
	std::string load(const std::string_view filename) {
		if (buffers.find(std::string(filename)) == buffers.end()) {
			const Timer timer;
			buffers.emplace(filename, filename);
			active = filename;
			return std::string("load ") + std::string(filename) + " in " + timer.us();
		}
		return {};
	}

	std::string reload() {
		if (auto* buffer = current()) {
			const Timer timer;
			buffer->reload();
			return std::string("reload ") + std::string(buffer->get_filename()) + " in " + timer.us();
		}
		return {};
	}

	std::string save() {
		if (auto* buffer = current()) {
			const Timer timer;
			buffer->save();
			return std::string("save ") + std::string(buffer->get_filename()) + " in " + timer.us();
		}
		return {};
	}

	std::string close() {
		if (auto* buffer = current()) {
			const Timer timer;
			const auto filename = std::string(buffer->get_filename());
			buffers.erase(filename);
			active.clear();
			select_previous();
			return std::string("close ") + filename + " in " + timer.us();
		}
		return {};
	}

	Buffer* current() {
		if (!active.empty())
			return &buffers[active];
		return nullptr;
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == 'j') { select_next(); }
		else if (key == 'k') { select_previous(); }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned col = 0;
		unsigned row = 1;
		for (auto& it : buffers) {
			col = 0;
			if (active == it.second.get_filename())
				push_cursor_line(characters, row, col_count);
			push_string(characters, row++, col, it.second.get_filename()); // [TODO] Display if need save.
		}
	}
};

