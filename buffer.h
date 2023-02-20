#pragma once

enum class Mode {
	normal,
	normal_number,
	normal_slash,
	normal_question,
	normal_gt,
	normal_lt,
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

class Buffer {
	Stack stack;

	Mode mode = Mode::normal;

	std::vector<unsigned> record;
	std::vector<unsigned> temp_record;
	bool repeat = false;

	std::string filename;
	std::string highlight;

	unsigned accu = 0;

	unsigned f_key = 0;
	size_t cursor = 0;

	bool char_forward = false;
	bool word_forward = false;
	bool word_strict = false;

	bool needs_save = false;
	bool is_code = false;

	size_t last_cursor = 0;

	void save_cursor() {
		last_cursor = state().get_cursor();
	}

	void load_cursor() {
		const auto cursor = state().get_cursor();
		state().set_cursor(last_cursor);
		last_cursor = cursor;
	}

	void begin_record(unsigned key) {
		temp_record.clear();
		temp_record.push_back(key);
	}

	void append_record(unsigned key) {
		temp_record.push_back(key);
	}

	void end_record(unsigned key) {
		temp_record.push_back(key);
		record = std::move(temp_record);
	}

	void begin_end_record(unsigned key) {
		temp_record.clear();
		end_record(key);
	}

	void replay(std::string& clipboard, unsigned col_count, unsigned row_count) {
		const auto s = record; // Cache since process_key will modify it.
		for (const auto& c : s) {
			process_key(clipboard, c, col_count, row_count);
		}
	}

	void accumulate(unsigned key) {
		verify(key >= '0' && key <= '9');
		const auto digit = (unsigned)key - (unsigned)'0';
		accu *= 10;
		accu += digit;
	}

	std::pair<bool, bool> check_highlight_strict(size_t index) const {
		Word current(state().get_text(), index);
		if (current.to_string(state().get_text()) == highlight)
			return { true, index == current.end() };
		return { false, false };
	}

	bool check_highlight_loose(size_t index) const {
		return state().get_text().substr(index, highlight.size()) == highlight;
	}

	void line_find() {
		if(char_forward) { state().line_find(f_key); }
		else { state().line_rfind(f_key); }
	}

	void line_rfind() {
		if(char_forward) { state().line_rfind(f_key); }
		else { state().line_find(f_key); }
	}

	void word_find_under_cursor(unsigned row_count) {
		highlight = state().current_word();
		word_forward = true;
		word_strict = true;
		state().word_find(highlight, word_strict);
		state().cursor_center(row_count);
	}

	void word_rfind_under_cursor(unsigned row_count) {
		highlight = state().current_word();
		word_forward = false;
		word_strict = true;
		state().word_rfind(highlight, word_strict);
		state().cursor_center(row_count);
	}

	void word_find_partial(unsigned row_count) {
		word_forward = true;
		word_strict = false;
		if (!state().current_word().starts_with(highlight)) {
			state().word_find(highlight, word_strict);
			state().cursor_center(row_count);
		}
	}

	void word_rfind_partial(unsigned row_count) {
		word_forward = false;
		word_strict = false;
		if (!state().current_word().starts_with(highlight)) {
			state().word_rfind(highlight, word_strict);
			state().cursor_center(row_count);
		}
	}

	void word_find_again(unsigned row_count) {
		if (word_forward) { state().word_find(highlight, word_strict); }
		else { state().word_rfind(highlight, word_strict); }
		state().cursor_center(row_count);
	}

	void word_rfind_again(unsigned row_count) {
		if (word_forward) { state().word_rfind(highlight, word_strict); }
		else { state().word_find(highlight, word_strict); }
		state().cursor_center(row_count);
	}

	void process_key(std::string& clipboard, unsigned key, unsigned col_count, unsigned row_count) {
		if (mode != Mode::insert) {
			stack.push();
		}

		switch (mode) {
		case Mode::normal: process_normal(clipboard, key, row_count); break;
		case Mode::normal_number: process_normal_number(key); break;
		case Mode::normal_slash: process_normal_slash(key, row_count); break;
		case Mode::normal_question: process_normal_question(key, row_count); break;
		case Mode::normal_gt: process_normal_gt(key); break;
		case Mode::normal_lt: process_normal_lt(key); break;
		case Mode::normal_c: process_normal_c(clipboard, key); break;
		case Mode::normal_cf: process_normal_cf(clipboard, key); break;
		case Mode::normal_ct: process_normal_ct(clipboard, key); break;
		case Mode::normal_ci: process_normal_ci(clipboard, key); break;
		case Mode::normal_ca: process_normal_ca(clipboard, key); break;
		case Mode::normal_d: process_normal_d(clipboard, key); break;
		case Mode::normal_df: process_normal_df(clipboard, key); break;
		case Mode::normal_dt: process_normal_dt(clipboard, key); break;
		case Mode::normal_di: process_normal_di(clipboard, key); break;
		case Mode::normal_da: process_normal_da(clipboard, key); break;
		case Mode::normal_f: process_normal_f(key); break;
		case Mode::normal_F: process_normal_F(key); break;
		case Mode::normal_r: process_normal_r(clipboard, key); break;
		case Mode::normal_y: process_normal_y(clipboard, key); break;
		case Mode::normal_yf: process_normal_yf(clipboard, key); break;
		case Mode::normal_yt: process_normal_yt(clipboard, key); break;
		case Mode::normal_yi: process_normal_yi(clipboard, key); break;
		case Mode::normal_ya: process_normal_ya(clipboard, key); break;
		case Mode::normal_z: process_normal_z(key, row_count); break;
		case Mode::insert: process_insert(key); break;
		};

		if (mode != Mode::insert) {
			needs_save = stack.pop() || needs_save;
		}
	}

	void process_insert(unsigned key) {
		if (key == Glyph::ESCAPE) { end_record(key); mode = Mode::normal; }
		else if (key == '\b') { append_record(key); state().erase_back(); }
		else if (key == '\t') { append_record(key); state().insert("\t"); }
		else if (key == '\r') { append_record(key); state().insert("\n" + state().copy_line_whitespace()); }
		else { append_record(key); state().insert(std::string((char*)&key, 1)); }
	}

	void process_normal_o() {
		state().line_end();
		state().insert("\n" + state().copy_line_whitespace());
	}

	void process_normal_O() {
		state().line_start();
		state().insert(state().copy_line_whitespace() + "\n");
		state().prev_line();
		state().line_end();
	}

	void process_normal_J() {
		state().line_end();
		state().erase();
		state().remove_line_whitespace();
		state().insert(" ");
		state().prev_char();
	}

	void process_normal(std::string& clipboard, unsigned key, unsigned row_count) {
		if (key == 'u') { stack.set_undo(); }
		else if (key >= '0' && key <= '9') { accumulate(key); mode = Mode::normal_number; }
		else if (key == '>') { begin_record(key); mode = Mode::normal_gt; }
		else if (key == '<') { begin_record(key); mode = Mode::normal_lt; }
		else if (key == 'c') { begin_record(key); mode = Mode::normal_c; }
		else if (key == 'd') { begin_record(key); mode = Mode::normal_d; }
		else if (key == 'y') { begin_record(key); mode = Mode::normal_y; }
		else if (key == 'r') { begin_record(key); mode = Mode::normal_r; }
		else if (key == 'f') { mode = Mode::normal_f; }
		else if (key == 'F') { mode = Mode::normal_F; }
		else if (key == 'z') { mode = Mode::normal_z; }
		else if (key == 'i') { begin_record(key); mode = Mode::insert; }
		else if (key == 'I') { begin_record(key); state().line_start_whitespace(); mode = Mode::insert; }
		else if (key == 'a') { begin_record(key); state().next_char(); mode = Mode::insert; }
		else if (key == 'A') { begin_record(key); state().line_end(); mode = Mode::insert; }
		else if (key == 'o') { begin_record(key); process_normal_o(); mode = Mode::insert; }
		else if (key == 'O') { begin_record(key); process_normal_O(); mode = Mode::insert; }
		else if (key == 's') { begin_record(key); state().erase(); mode = Mode::insert; }
		else if (key == 'S') { begin_record(key); clipboard = state().erase_line_contents(); mode = Mode::insert; }
		else if (key == 'C') { begin_record(key); clipboard = state().erase_to_line_end(); mode = Mode::insert; }
		else if (key == 'x') { begin_end_record(key); clipboard = state().erase(); }
		else if (key == 'D') { begin_end_record(key); clipboard = state().erase_to_line_end(); }
		else if (key == 'J') { begin_end_record(key); process_normal_J(); }
		else if (key == '~') { begin_end_record(key); state().change_case(); }
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
		else if (key == 'e') { state().word_end(); }
		else if (key == '[') { state().enclosure_start(); }
		else if (key == ']') { state().enclosure_end(); }
		else if (key == 'g') { save_cursor(); state().buffer_start(); }
		else if (key == 'G') { save_cursor(); state().buffer_end(); }
		else if (key == 'H') { save_cursor(); state().window_top(row_count); }
		else if (key == 'M') { save_cursor(); state().window_center(row_count); }
		else if (key == 'L') { save_cursor(); state().window_bottom(row_count); }
		else if (key == '+') { save_cursor(); state().next_line(); state().line_start_whitespace(); }
		else if (key == '-') { save_cursor(); state().prev_line(); state().line_start_whitespace(); }
		else if (key == ';') { line_find(); mode = Mode::normal; }
		else if (key == ',') { line_rfind(); mode = Mode::normal; }
		else if (key == '*') { save_cursor(); word_find_under_cursor(row_count); }
		else if (key == '#') { save_cursor(); word_rfind_under_cursor(row_count); }
		else if (key == '/') { highlight.clear(); mode = Mode::normal_slash; }
		else if (key == '?') { highlight.clear(); mode = Mode::normal_question; }
		else if (key == 'n') { word_find_again(row_count); }
		else if (key == 'N') { word_rfind_again(row_count); }
		else if (key == '.') { repeat = true; }
		else if (key == '\'') { load_cursor(); state().cursor_center(row_count); }
	}

	void process_normal_number(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'j') { save_cursor(); state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { save_cursor(); state().jump_up(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { save_cursor(); state().buffer_start(); state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_slash(unsigned key, unsigned row_count) {
		if (key == Glyph::ESCAPE) { highlight.clear(); mode = Mode::normal; }
		else if (key == '\r') { word_find_partial(row_count); mode = Mode::normal; }
		else if (key == '\b') { if (highlight.size() > 0) { highlight.pop_back(); word_find_partial(row_count); } }
		else { highlight += key; word_find_partial(row_count); }
	}

	void process_normal_question(unsigned key, unsigned row_count) {
		if (key == Glyph::ESCAPE) { highlight.clear(); mode = Mode::normal; }
		else if (key == '\r') { word_rfind_partial(row_count); mode = Mode::normal; }
		else if (key == '\b') { if (highlight.size() > 0) { highlight.pop_back(); word_rfind_partial(row_count); } }
		else { highlight += key; word_rfind_partial(row_count); }
	}
	void process_normal_f(unsigned key) {
		if (key == Glyph::ESCAPE) { mode = Mode::normal; }
		else { state().line_find(key); f_key = key; char_forward = true; mode = Mode::normal; }
	}

	void process_normal_F(unsigned key) {
		if (key == Glyph::ESCAPE) { mode = Mode::normal; }
		else { state().line_rfind(key); f_key = key; char_forward = false; mode = Mode::normal; }
	}

	void process_normal_r(std::string& clipboard, unsigned key) {
		if (key == Glyph::ESCAPE) { mode = Mode::normal; }
		else { end_record(key); clipboard = state().erase(); state().insert(std::string((char*)&key, 1)); state().prev_char(); mode = Mode::normal; }
	}

	void process_normal_z(unsigned key, unsigned row_count) {
		if (key == 'z') { state().cursor_center(row_count); mode = Mode::normal; }
		else if (key == 't') { state().cursor_top(row_count); mode = Mode::normal; }
		else if (key == 'b') { state().cursor_bottom(row_count); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_y(std::string& clipboard, unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == 'y') { end_record(key); clipboard = state().yank_line(); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { end_record(key); clipboard = state().yank_words(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { end_record(key); clipboard = state().yank_all_up(); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { end_record(key); clipboard = state().yank_all_down(); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { end_record(key); clipboard = state().yank_lines_down(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { end_record(key); clipboard = state().yank_lines_up(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { append_record(key); accu = 0; mode = Mode::normal_yf; }
		else if (key == 't') { append_record(key); accu = 0; mode = Mode::normal_yt; }
		else if (key == 'i') { append_record(key); accu = 0; mode = Mode::normal_yi; }
		else if (key == 'a') { append_record(key); accu = 0; mode = Mode::normal_ya; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_yf(std::string& clipboard, unsigned key) {
		end_record(key); clipboard = state().yank_to(key); mode = Mode::normal;
	}

	void process_normal_yt(std::string& clipboard, unsigned key) {
		end_record(key); clipboard = state().yank_until(key); mode = Mode::normal;
	}

	void process_normal_yi(std::string& clipboard, unsigned key) {
		if (key == 'w') { end_record(key); clipboard = state().yank_word(); mode = Mode::normal; }
		else if (key == '(' || key == ')') { end_record(key); clipboard = state().yank_enclosure('(', ')', false); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clipboard = state().yank_enclosure('{', '}', false); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clipboard = state().yank_enclosure('[', ']', false); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_ya(std::string& clipboard, unsigned key) {
		if (key == '(' || key == ')') { end_record(key); clipboard = state().yank_enclosure('(', ')', true); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clipboard = state().yank_enclosure('{', '}', true); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clipboard = state().yank_enclosure('[', ']', true); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_gt(unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == '>') { end_record(key); state().indent_right(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { end_record(key); state().indent_right_down(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { end_record(key); state().indent_right_up(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_lt(unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == '<') { end_record(key); state().indent_left(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { end_record(key); state().indent_left_down(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { end_record(key); state().indent_left_up(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_c(std::string& clipboard, unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == 'c') { append_record(key); clipboard = state().erase_line_contents(); accu = 0; mode = Mode::insert; }
		else if (key == 'w') { append_record(key); clipboard = state().erase_words(std::max(1u, accu)); accu = 0; mode = Mode::insert; }
		else if (key == 'g') { append_record(key); clipboard = state().erase_all_up(); accu = 0; mode = Mode::insert; }
		else if (key == 'G') { append_record(key); clipboard = state().erase_all_down(); accu = 0; mode = Mode::insert; }
		else if (key == 'j') { append_record(key); clipboard = state().erase_lines_down(std::max(1u, accu)); accu = 0; mode = Mode::insert; }
		else if (key == 'k') { append_record(key); clipboard = state().erase_lines_up(std::max(1u, accu)); accu = 0; mode = Mode::insert; }
		else if (key == 'f') { append_record(key); accu = 0; mode = Mode::normal_cf; }
		else if (key == 't') { append_record(key); accu = 0; mode = Mode::normal_ct; }
		else if (key == 'i') { append_record(key); accu = 0; mode = Mode::normal_ci; }
		else if (key == 'a') { append_record(key); accu = 0; mode = Mode::normal_ca; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_cf(std::string& clipboard, unsigned key) {
		append_record(key); clipboard = state().erase_to(key); mode = Mode::insert;
	}

	void process_normal_ct(std::string& clipboard, unsigned key) {
		append_record(key); clipboard = state().erase_until(key); mode = Mode::insert;
	}

	void process_normal_ci(std::string& clipboard, unsigned key) {
		if (key == 'w') { append_record(key); clipboard = state().erase_word(false); mode = Mode::insert; }
		else if (key == '(' || key == ')') { append_record(key); clipboard = state().erase_enclosure('(', ')', false); mode = Mode::insert; }
		else if (key == '{' || key == '}') { append_record(key); clipboard = state().erase_enclosure('{', '}', false); mode = Mode::insert; }
		else if (key == '[' || key == ']') { append_record(key); clipboard = state().erase_enclosure('[', ']', false); mode = Mode::insert; }
		else { mode = Mode::normal; }
	}

	void process_normal_ca(std::string& clipboard, unsigned key) {
		if (key == '(' || key == ')') { append_record(key); clipboard = state().erase_enclosure('(', ')', true); mode = Mode::insert; }
		else if (key == '{' || key == '}') { append_record(key); clipboard = state().erase_enclosure('{', '}', true); mode = Mode::insert; }
		else if (key == '[' || key == ']') { append_record(key); clipboard = state().erase_enclosure('[', ']', true); mode = Mode::insert; }
		else { mode = Mode::normal; }
	}

	void process_normal_d(std::string& clipboard, unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == 'd') { end_record(key); clipboard = state().erase_line(); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { end_record(key); clipboard = state().erase_words(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { end_record(key); clipboard = state().erase_all_up(); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { end_record(key); clipboard = state().erase_all_down(); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { end_record(key); clipboard = state().erase_lines_down(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { end_record(key); clipboard = state().erase_lines_up(std::max(1u, accu)); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { append_record(key); accu = 0; mode = Mode::normal_df; }
		else if (key == 't') { append_record(key); accu = 0; mode = Mode::normal_dt; }
		else if (key == 'i') { append_record(key); accu = 0; mode = Mode::normal_di; }
		else if (key == 'a') { append_record(key); accu = 0; mode = Mode::normal_da; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_df(std::string& clipboard, unsigned key) {
		end_record(key); clipboard = state().erase_to(key); mode = Mode::normal;
	}

	void process_normal_dt(std::string& clipboard, unsigned key) {
		end_record(key); clipboard = state().erase_until(key); mode = Mode::normal;
	}

	void process_normal_di(std::string& clipboard, unsigned key) {
		if (key == 'w') { end_record(key); clipboard = state().erase_word(false); mode = Mode::normal; }
		else if (key == '(' || key == ')') { end_record(key); clipboard = state().erase_enclosure('(', ')', false); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clipboard = state().erase_enclosure('{', '}', false); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clipboard = state().erase_enclosure('[', ']', false); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_da(std::string& clipboard, unsigned key) {
		if (key == '(' || key == ')') { end_record(key); clipboard = state().erase_enclosure('(', ')', true); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clipboard = state().erase_enclosure('{', '}', true); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clipboard = state().erase_enclosure('[', ']', true); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void push_digit(Characters& characters, unsigned row, unsigned col, unsigned digit) const {
		characters.emplace_back((uint16_t)(48 + digit), colors().line_number, row, col);
	}

	void push_number(Characters& characters, unsigned row, unsigned col, unsigned line) const {
		if (line > 999) { push_digit(characters, row, col + 0, (line % 10000) / 1000); }
		if (line > 99) { push_digit(characters, row, col + 1, (line % 1000) / 100); }
		if (line > 9) { push_digit(characters, row, col + 2, (line % 100) / 10); }
		push_digit(characters, row, col + 3, line % 10);
	}

	void push_line_number(Characters& characters, unsigned row, unsigned col, unsigned absolute_row, unsigned cursor_row) const {
		unsigned column = absolute_row == cursor_row ? col : col + 1;
		const unsigned line = absolute_row == cursor_row ? absolute_row :
			absolute_row < cursor_row ? cursor_row - absolute_row :
			absolute_row - cursor_row;
		push_number(characters, row, column, line);
	}

	void push_highlight_one(Characters& characters, unsigned row, unsigned col, bool last) const {
		characters.emplace_back(Glyph::BLOCK, colors().highlight, row, col, 1.2f, 1.0f, -1.0f, 0.0f);
	}

	void push_highlight(Characters& characters, unsigned row, unsigned col) const {
		for (unsigned i = 0; i < (unsigned)highlight.size(); ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().highlight, row, col + i, 1.2f, 1.0f, -1.0f, 0.0f);
		}
	};

	void push_column_indicator(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::BLOCK, colors().column_indicator, row, col);
	};

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) const {
		for (unsigned i = 0; i < col_count - 5; ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().cursor_line, row, 7 + i, 1.2f, 1.0f, -1.0f, 0.0f);
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(
			mode == Mode::insert ? Glyph::LINE :
			mode == Mode::normal ? Glyph::BLOCK :
			Glyph::BOTTOM,
			colors().cursor, row, col);
	};

	void push_carriage(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::CARRIAGE, colors().whitespace, row, col);
	};

	void push_return(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::RETURN, colors().whitespace, row, col);
	};

	void push_tab(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::TAB, colors().whitespace, row, col);
	};

	void push_space(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::SPACE, colors().whitespace, row, col);
	};

	void push_char_text(Characters& characters, unsigned row, unsigned col, char c, unsigned index) const {
		const Line line(state().get_text(), index);
		if (index == state().get_cursor() && mode == Mode::normal) { characters.emplace_back((uint16_t)c, colors().text_cursor, row, col); }
		else if (line.check_string(state().get_text(), "+++")) { characters.emplace_back((uint16_t)c, colors().diff_note, row, col, true); }
		else if (line.check_string(state().get_text(), "---")) { characters.emplace_back((uint16_t)c, colors().diff_note, row, col, true); }
		else if (line.check_string(state().get_text(), "+")) { characters.emplace_back((uint16_t)c, colors().diff_add, row, col); }
		else if (line.check_string(state().get_text(), "-")) { characters.emplace_back((uint16_t)c, colors().diff_remove, row, col); }
		else { characters.emplace_back((uint16_t)c, colors().text, row, col); }
	};

	void push_char_code(Characters& characters, unsigned row, unsigned col, char c, unsigned index) const {
		const Word word(state().get_text(), index);
		const Comment comment(state().get_text(), index);
		const Quote single_quote(state().get_text(), index, '\'');
		const Quote double_quote(state().get_text(), index, '"');
		if (index == state().get_cursor() && mode == Mode::normal) { characters.emplace_back((uint16_t)c, colors().text_cursor, row, col); }
		else if (comment.valid() && comment.contains(index)) { characters.emplace_back((uint16_t)c, colors().comment, row, col); }
		else if (single_quote.valid()) { characters.emplace_back((uint16_t)c, colors().quote, row, col); }
		else if (double_quote.valid()) { characters.emplace_back((uint16_t)c, colors().quote, row, col); }
		else if (word.check_keyword(state().get_text())) { characters.emplace_back((uint16_t)c, colors().keyword, row, col, true); }
		else if (word.check_function()) { characters.emplace_back((uint16_t)c, colors().function, row, col, true); }
		else if (word.check_class()) { characters.emplace_back((uint16_t)c, colors().clas, row, col, true); }
		else if (is_punctuation(c)) { characters.emplace_back((uint16_t)c, colors().punctuation, row, col, true); }
		else if (is_number(c)) { characters.emplace_back((uint16_t)c, colors().number, row, col, true); }
		else if (is_whitespace(c)) { characters.emplace_back((uint16_t)c, colors().whitespace, row, col); }
		else { characters.emplace_back((uint16_t)c, colors().text, row, col); }
	};

	void push_text(Characters& characters, unsigned col_count, unsigned row_count) const {
		const unsigned cursor_row = state().find_cursor_row();
		const unsigned begin_row = state().get_begin_row();
		const unsigned end_row = begin_row + row_count;
		unsigned absolute_row = 0;
		unsigned index = 0;
		unsigned row = 1;
		unsigned col = 0;
		for (auto& c : state().get_text()) {
			if (absolute_row >= begin_row && absolute_row <= end_row) {
				if (col <= col_count) {
				if (col == 0) { push_column_indicator(characters, row, 87);}
				if (col == 0 && absolute_row == cursor_row) { push_cursor_line(characters, row, col_count); }
				if (col == 0) { push_line_number(characters, row, col, absolute_row, cursor_row); col += 7; }
				if (word_strict) { if (auto match = check_highlight_strict(index); match.first) { push_highlight_one(characters, row, col, match.second); } }
				else { if (check_highlight_loose(index)) { push_highlight(characters, row, col); } }
				if (index == state().get_cursor()) { push_cursor(characters, row, col); }
				if (c == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; }
				else if (c == '\t') { push_tab(characters, row, col); col += spacing().tab; }
				else if (c == '\r') { push_carriage(characters, row, col); col++; }
				else if (c == ' ') { push_space(characters, row, col); col++; }
				else if (is_code) { push_char_code(characters, row, col, c, index); col++; }
				else { push_char_text(characters, row, col, c, index); col++; }
			} else {
					if (c == '\n') { absolute_row++; row++; col = 0; }
				}
			} else {
				if (c == '\n') { absolute_row++; }
			}
			index++;
		}
	}

	size_t location_percentage() const {
		if (const auto size = get_size())
			return 1 + state().get_cursor() * 100 / size;
		return 0;
	}

	void push_status(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned row = 0;
		unsigned col = 0;
		push_line(characters, colors().bar, float(row), col, col_count);
		col = 0;
		const auto name = filename + (is_dirty() ? "*" : "");
		push_string(characters, colors().bar_text, row, col, name);
		col = 62;
		push_string(characters, colors().bar_text, row, col, readable_size(get_size()));
		col = 74;
		const auto percentage = std::to_string(location_percentage()) + "%";
		push_string(characters, colors().bar_text, row, col, percentage);
		col = 82;
		const auto col_and_row = state().find_cursor_row_and_col();
		const auto locations = std::to_string(col_and_row.first) + "," + std::to_string(col_and_row.second);
		push_string(characters, colors().bar_text, row, col, locations);
	}

	void init(const std::string_view text) {
		stack.set_cursor(0);
		stack.set_text(text);
	}

	std::string load() {
		std::string text;
		if (!filename.empty()) {
			text = "\n"; // EOF
			needs_save = true;
			if (std::filesystem::exists(filename)) {
				map(filename, [&](const char* mem, size_t size) {
					text = std::string(mem, size);
					needs_save = false;
				});
			}
		}
		return text;
	}

public:
	Buffer(const std::string_view filename)
		: filename(filename)
		, is_code(is_code_extension(filename)) {
		init(load());
	}

	void reload() {
		init(load());
	}

	void save() {
		if (!filename.empty()) {
			if (write(filename, stack.get_text())) {
				needs_save = false;
			}
		}
	}

	void clear_highlight() { highlight.clear(); }

	void process(std::string& clipboard, unsigned key, unsigned col_count, unsigned row_count) {
		process_key(clipboard, key, col_count, row_count);
		if (repeat) { repeat = false; replay(clipboard, col_count, row_count); }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		push_status(characters, col_count, row_count);
		push_text(characters, col_count, row_count);
	}

	void jump(size_t position, unsigned row_count) {
		state().set_cursor(position);
		state().cursor_center(row_count);
	}

	State& state() { return stack.state(); }
	const State& state() const { return stack.state(); }

	const std::string_view get_filename() const { return filename; }
	size_t get_size() const { return state().get_text().size(); }

	std::string get_word() const { return state().get_word(); }

	bool is_normal() const { return mode == Mode::normal; }
	bool is_dirty() const { return needs_save; }
};

