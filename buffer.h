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

	State& state() { return stack.state(); }
	const State& state() const { return stack.state(); }

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

	void replay(std::string& clipboard, bool& jump) {
		const auto s = record; // Cache since process_key will modify it.
		for (const auto& c : s) {
			process_key(clipboard, jump, c);
		}
	}

	void accumulate(unsigned key) {
		if (key >= '0' && key <= '9') {
			const auto digit = (unsigned)key - (unsigned)'0';
			accu *= 10;
			accu += digit;
		}
	}

	bool is_mode_search() const {
		return mode == Mode::normal_question || mode == Mode::normal_slash;
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

	void word_find_under_cursor() {
		highlight = state().current_word();
		word_forward = true;
		word_strict = true;
		state().word_find(highlight, word_strict);
		state().cursor_center();
	}

	void word_rfind_under_cursor() {
		highlight = state().current_word();
		word_forward = false;
		word_strict = true;
		state().word_rfind(highlight, word_strict);
		state().cursor_center();
	}

	void word_find_partial() {
		word_forward = true;
		word_strict = false;
		if (!state().current_word().starts_with(highlight)) {
			state().word_find(highlight, word_strict);
			state().cursor_center();
		}
	}

	void word_rfind_partial() {
		word_forward = false;
		word_strict = false;
		if (!state().current_word().starts_with(highlight)) {
			state().word_rfind(highlight, word_strict);
			state().cursor_center();
		}
	}

	void word_find_again() {
		if (word_forward) { state().word_find(highlight, word_strict); }
		else { state().word_rfind(highlight, word_strict); }
		state().cursor_center();
	}

	void word_rfind_again() {
		if (word_forward) { state().word_rfind(highlight, word_strict); }
		else { state().word_find(highlight, word_strict); }
		state().cursor_center();
	}

	void process_key(std::string& clipboard, bool& jump, unsigned key) {
		if (mode != Mode::insert) {
			stack.push();
		}

		switch (mode) {
		case Mode::normal: process_normal(clipboard, jump, key); break;
		case Mode::normal_number: process_normal_number(key); break;
		case Mode::normal_slash: process_normal_slash(key); break;
		case Mode::normal_question: process_normal_question(key); break;
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
		case Mode::normal_z: process_normal_z(key); break;
		case Mode::insert: process_insert(key); break;
		};

		if (mode != Mode::insert) {
			needs_save = stack.pop() || needs_save;
		}
	}

	void process_insert(unsigned key) {
		if (key == Codepoint::ESCAPE) { end_record(key); mode = Mode::normal; }
		else if (key == '\b') { append_record(key); state().erase_back(); }
		else if (key == '\t') { append_record(key); state().insert("\t"); }
		else if (key == '\r') { append_record(key); state().insert("\n" + state().copy_line_whitespace()); }
		else { append_record(key); state().insert(std::string((char*)&key, 1)); }
	}

	void process_normal(std::string& clipboard, bool& jump, unsigned key) {
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
		else if (key == 'o') { begin_record(key); state().insert_line_down(); mode = Mode::insert; }
		else if (key == 'O') { begin_record(key); state().insert_line_up(); mode = Mode::insert; }
		else if (key == 's') { begin_record(key); state().erase(); mode = Mode::insert; }
		else if (key == 'S') { begin_record(key); clipboard = state().erase_line_contents(); mode = Mode::insert; }
		else if (key == 'C') { begin_record(key); clipboard = state().erase_to_line_end(); mode = Mode::insert; }
		else if (key == 'x') { begin_end_record(key); clipboard = state().erase(); }
		else if (key == 'D') { begin_end_record(key); clipboard = state().erase_to_line_end(); }
		else if (key == 'J') { begin_end_record(key); state().join_lines(); }
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
		else if (key == '[') { state().enclosure_start('{', '}'); }
		else if (key == ']') { state().enclosure_end('{', '}'); }
		else if (key == '%') { state().enclosure_match(); }
		else if (key == 'g') { state().buffer_start(); }
		else if (key == 'G') { state().buffer_end(); }
		else if (key == 'H') { state().window_top(); }
		else if (key == 'M') { state().window_center(); }
		else if (key == 'L') { state().window_bottom(); }
		else if (key == '+') { state().next_line(); state().line_start_whitespace(); }
		else if (key == '-') { state().prev_line(); state().line_start_whitespace(); }
		else if (key == ';') { line_find(); mode = Mode::normal; }
		else if (key == ',') { line_rfind(); mode = Mode::normal; }
		else if (key == '*') { word_find_under_cursor(); }
		else if (key == '#') { word_rfind_under_cursor(); }
		else if (key == '\'') { mode = Mode::normal_slash; }
		else if (key == '"') { mode = Mode::normal_question; }
		else if (key == '/') { highlight.clear(); mode = Mode::normal_slash; }
		else if (key == '?') { highlight.clear(); mode = Mode::normal_question; }
		else if (key == 'n') { word_find_again(); }
		else if (key == 'N') { word_rfind_again(); }
		else if (key == '.') { repeat = true; }
		else if (key == '\r') { jump = true; }
	}

	void process_normal_number(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'j') { state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { state().jump_up(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { state().buffer_start(); state().jump_down(accu > 0 ? accu - 1 : 0); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_slash(unsigned key) {
		if (key == Codepoint::ESCAPE) { highlight.clear(); mode = Mode::normal; }
		else if (key == '\r') { word_find_partial(); mode = Mode::normal; }
		else if (key == '\b') { if (highlight.size() > 0) { highlight.pop_back(); word_find_partial(); } }
		else { highlight += key; word_find_partial(); }
	}

	void process_normal_question(unsigned key) {
		if (key == Codepoint::ESCAPE) { highlight.clear(); mode = Mode::normal; }
		else if (key == '\r') { word_rfind_partial(); mode = Mode::normal; }
		else if (key == '\b') { if (highlight.size() > 0) { highlight.pop_back(); word_rfind_partial(); } }
		else { highlight += key; word_rfind_partial(); }
	}
	void process_normal_f(unsigned key) {
		if (key == Codepoint::ESCAPE) { mode = Mode::normal; }
		else { state().line_find(key); f_key = key; char_forward = true; mode = Mode::normal; }
	}

	void process_normal_F(unsigned key) {
		if (key == Codepoint::ESCAPE) { mode = Mode::normal; }
		else { state().line_rfind(key); f_key = key; char_forward = false; mode = Mode::normal; }
	}

	void process_normal_r(std::string& clipboard, unsigned key) {
		if (key == Codepoint::ESCAPE) { mode = Mode::normal; }
		else { end_record(key); clipboard = state().erase(); state().insert(std::string((char*)&key, 1)); state().prev_char(); mode = Mode::normal; }
	}

	void process_normal_z(unsigned key) {
		if (key == 'z') { state().cursor_center(); mode = Mode::normal; }
		else if (key == 't') { state().cursor_top(); mode = Mode::normal; }
		else if (key == 'b') { state().cursor_bottom(); mode = Mode::normal; }
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

	void push_line_number(Characters& characters, unsigned row, unsigned col, unsigned absolute_row, unsigned cursor_row) const {
		unsigned column = absolute_row == cursor_row ? col : col + 1;
		const unsigned line = absolute_row == cursor_row ? 1 + absolute_row :
			absolute_row < cursor_row ? cursor_row - absolute_row :
			absolute_row - cursor_row;
		push_number(characters, row, column, line);
	}

	void push_highlight_one(Characters& characters, unsigned row, unsigned col, bool last) const {
		characters.emplace_back(Codepoint::BLOCK, is_mode_search() ? colors().search : colors().highlight, row, col);
	}

	void push_highlight(Characters& characters, unsigned row, unsigned col) const {
		for (unsigned i = 0; i < (unsigned)highlight.size(); ++i) {
			characters.emplace_back(Codepoint::BLOCK, is_mode_search() ? colors().search : colors().highlight, row, col + i);
		}
	};

	void push_column_indicator(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Codepoint::BLOCK, colors().column_indicator, row, col);
	};

	void push_line_indicator(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Codepoint::LINE, colors().line_number, row, col);
	};

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_count) const {
		for (unsigned i = 0; i < col_count - 7; ++i) {
			characters.emplace_back(Codepoint::BLOCK, colors().cursor_line, row, 7 + i);
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(
			mode == Mode::insert ? Codepoint::LINE :
			mode == Mode::normal ? Codepoint::BLOCK :
			Codepoint::BOTTOM,
			colors().cursor, row, col);
	};

	void push_char_text(Characters& characters, unsigned row, unsigned col, char c, unsigned index) const {
		if (index == state().get_cursor() && mode == Mode::normal) { characters.emplace_back((uint16_t)c, colors().text_cursor, row, col); }
		else { characters.emplace_back((uint16_t)c, colors().text, row, col); }
	};

	unsigned push_text(Characters& characters, unsigned col_count, unsigned row_count) const {
		const unsigned cursor_row = state().find_cursor_row();
		const unsigned begin_row = state().get_begin_row();
		unsigned absolute_row = 0;
		unsigned index = 0;
		unsigned row = 2;
		unsigned col = 0;
		for (auto& c : state().get_text()) {
			if (absolute_row < begin_row) {
				if (c == '\n') { absolute_row++; }
			}
			else if (absolute_row >= begin_row && (row - 1) <= row_count - 2) {
				if (col == col_count) { row++; col = 7; }
				if (col == 0 && absolute_row == cursor_row) { push_cursor_line(characters, row, col_count); }
				if (col == 0 && absolute_row != cursor_row) { push_column_indicator(characters, row, 87); }
				if (col == 0) { push_line_number(characters, row, col, absolute_row, cursor_row); col += 6; }
				if (col == 6) { push_line_indicator(characters, row, col); col += 1; }
				if (word_strict) { if (auto match = check_highlight_strict(index); match.first) { push_highlight_one(characters, row, col, match.second); } }
				else { if (check_highlight_loose(index)) { push_highlight(characters, row, col); } }
				if (index == state().get_cursor()) { push_cursor(characters, row, col); }
				if (c == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; }
				else if (c == '\t') { push_tab(characters, row, col); col += 3; }
				else if (c == '\r') { push_carriage(characters, row, col); col++; }
				else if (c == ' ') { push_space(characters, row, col); col++; }
				else { push_char_text(characters, row, col, c, index); col++; }
			}
			else {
				break;
			}
			index++;
		}
		return absolute_row > begin_row ? absolute_row - begin_row - 1 : 0;
	}

public:
	Buffer(const std::string_view filename)
		: filename(filename) {
	}

	void init(const std::string_view text) {
		stack.set_cursor(0);
		stack.set_text(text);
	}

	void append(const std::string_view text) {
		stack.append_text(text);
		set_dirty(true);
	}

	void process(std::string& clipboard, bool& jump, unsigned key) {
		process_key(clipboard, jump, key);
		if (repeat) { repeat = false; replay(clipboard, jump); }
	}

	unsigned cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		return push_text(characters, col_count, row_count);
	}

	const std::string status() const {
		switch (mode) {
		case Mode::normal_question: return "?" + highlight;
		case Mode::normal_slash: return "/" + highlight;
		case Mode::insert: return "insert";
		default: return "";
		}
	}

	void jump(size_t position) {
		state().set_cursor(position);
		state().cursor_center();
	}

	void set_line_count(unsigned count) {
		state().set_line_count(count);
		state().cursor_clamp();
	}

	void window_down() { state().window_down(); }
	void window_up() { state().window_up(); }

	const std::string_view get_filename() const { return filename; }

	std::string get_url() const { return state().get_url(); }
	std::string get_line_url() const { return state().get_line_url(); }
	std::string get_word() const { return state().get_word(); }
	std::string_view get_text() const { return state().get_text(); }

	bool is_normal() const { return mode == Mode::normal; }

	void set_highlight(const std::string_view pattern) { highlight = pattern; word_forward = true; }
	void clear_highlight() { highlight.clear(); }

	void set_dirty(bool b) { needs_save = b; }
	bool is_dirty() const { return needs_save; }
};

