#pragma once

class State {
	std::string text;
	size_t cursor = 0;
	unsigned begin_row = 0;

public:
	const std::string_view get_text() const { return text; }
	void set_text(const std::string_view t) { text = t; }

	size_t get_cursor() const { return cursor; }
	void set_cursor(size_t u) { cursor = u; }

	unsigned get_begin_row() const { return begin_row; }

	Word incr(const Word& w) { return Word(text, w.end() < text.size() - 1 ? w.end() + 1 : w.end()); }
	Word decr(const Word& w) { return Word(text, w.begin() > 0 ? w.begin() - 1 : 0); }

	Line incr(const Line& w) { return Line(text, w.end() < text.size() - 1 ? w.end() + 1 : w.end()); }
	Line decr(const Line& w) { return Line(text, w.begin() > 0 ? w.begin() - 1 : 0); }

	unsigned find_cursor_row() const {
		unsigned number = 0;
		size_t index = 0;
		while (index < text.size() && index < cursor) {
			if (text[index++] == '\n')
				number++;
		}
		return number;
	}

	std::pair<unsigned, size_t> find_cursor_row_and_col() const {
		unsigned number = 0;
		size_t last = 0;
		size_t index = 0;
		while (index < text.size() && index < cursor) {
			if (text[index++] == '\n')
			{
				last = index;
				number++;
			}
		}
		return { number, cursor - last };
	}

	size_t find_char(unsigned key) {
		if (text.size() > 0) {
			const Line current(text, cursor);
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
			const Line current(text, cursor);
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

	size_t match(const std::string_view s, size_t index, bool strict) {
		size_t pos = index;
		do {
			pos = text.find(s, pos);
			if (strict && pos != std::string::npos)
				if (Word(text, pos).to_string(text) == s)
					break;
				else 
					pos += 1;
		}
		while (strict && pos != std::string::npos);
		return pos;
	}

	size_t rmatch(const std::string_view s, size_t index, bool strict) {
		size_t pos = index;
		do {
			pos = text.rfind(s, pos);
			if (strict && pos != std::string::npos)
				if (Word(text, pos).to_string(text) == s)
					break;
				else if (pos > 0) 
					pos -= 1;
				else
					break;
		}
		while (strict && pos != std::string::npos);
		return pos;
	}

	void word_find(const std::string_view s, bool strict) {
		if (const auto pos = match(s, cursor, strict); pos != std::string::npos) {
			if (pos == cursor && (cursor + 1 < text.size())) {
				if (const auto pos = match(s, cursor + 1, strict); pos != std::string::npos) { cursor = pos; }
				else if (const auto pos = match(s, 0, strict); pos != std::string::npos) { cursor = pos; }
			}
			else { cursor = pos; }
		}
		else if (const auto pos = match(s, 0, strict); pos != std::string::npos) { cursor = pos; }
	}

	void word_rfind(const std::string_view s, bool strict) {
		if (const auto pos = rmatch(s, cursor, strict); pos != std::string::npos) {
			if (pos == cursor && cursor > 0) {
				if (const auto pos = rmatch(s, cursor - 1, strict); pos != std::string::npos) { cursor = pos; }
				else if (const auto pos = rmatch(s, text.size() - 1, strict); pos != std::string::npos) { cursor = pos; }
			}
			else { cursor = pos; }
		}
		else if (const auto pos = rmatch(s, text.size() - 1, strict); pos != std::string::npos) { cursor = pos; }
	}

	void next_char() {
		const Line current(text, cursor);
		cursor = std::clamp(cursor + 1, current.begin(), current.end());
	}

	void prev_char() {
		const Line current(text, cursor);
		cursor = std::clamp(cursor > 0 ? cursor - 1 : 0, current.begin(), current.end());
	}

	void line_start() {
		const Line current(text, cursor);
		cursor = current.begin();
	}

	void line_end() {
		const Line current(text, cursor);
		cursor = current.end();
	}

	void line_start_whitespace() {
		const Line current(text, cursor);
		cursor = current.begin();
		while (cursor <= current.end()) {
			if (!is_line_whitespace(text[cursor]))
				break;
			next_char();
		}
	}

	void enclosure_start() {
		const auto pos = text[cursor] == '{' && cursor > 1 ? cursor - 1 : cursor;
		const Enclosure current(text, pos, '{', '}');
		if (current.valid())
			cursor = current.begin();
	}

	void enclosure_end() {
		const auto pos = text[cursor] == '}' && cursor < text.size() - 1 ? cursor - 1 : cursor;
		const Enclosure current(text, pos, '{', '}');
		if (current.valid())
			cursor = current.end();
	}

	void change_case() {
		const char c = text[cursor];
		char res = c;
		if (std::islower(c)) { res = std::toupper(c);
		} else { res = std::tolower(c); }
		if (res != c) {
			text[cursor] = res;
		}
	}

	std::string get_url() const {
		const Url current(text, cursor);
		if (!is_whitespace(text[current.begin()]))
			return std::string(current.to_string(text));
		return "";
	}

	std::string get_word() const {
		const Word current(text, cursor);
		if (!is_whitespace(text[current.begin()]))
			return std::string(current.to_string(text));
		return "";
	}

	std::string current_word() {
		const Word current(text, cursor);
		if (is_whitespace(text[current.begin()])) {
			const Word next = incr(current);
			cursor = next.begin();
			return std::string(next.to_string(text));
		}
		return std::string(current.to_string(text));
	}

	void word_end() {
		const Word current(text, cursor);
		if (is_whitespace(text[current.begin()])) { cursor = incr(current).end(); }
		else if (cursor == current.end()) {
			const Word next = incr(current);
			if (is_whitespace(text[next.begin()])) { cursor = incr(next).end(); }
			else { cursor = next.end(); }
		}
		else { cursor = current.end(); }
	}

	void next_word() {
		const Word current(text, cursor);
		const Word next = incr(current);
		if (is_whitespace(text[next.begin()])) { cursor = incr(next).begin(); }
		else { cursor = next.begin(); }
	}

	void prev_word() {
		const Word current(text, cursor);
		const Word prev = decr(current);
		if (is_whitespace(text[prev.begin()])) { cursor = decr(prev).begin(); }
		else { cursor = prev.begin(); }
	}

	bool is_first_line() {
		const Line current(text, cursor);
		return current.begin() == 0;
	}

	void next_line(unsigned row_count) {
		const Line current(text, cursor);
		const Line next = incr(current);
		cursor = next.to_absolute(current.to_relative(cursor));
		cursor_clamp(row_count);
	}

	void prev_line(unsigned row_count) {
		const Line current(text, cursor);
		const Line prev = decr(current);
		cursor = prev.to_absolute(current.to_relative(cursor));
		cursor_clamp(row_count);
	}

	void buffer_end(unsigned row_count) {
		const Line current(text, cursor);
		const Line last(text, text.size() - 1);
		cursor = last.to_absolute(current.to_relative(cursor));
		cursor_clamp(row_count);
	}

	void buffer_start(unsigned row_count) {
		const Line current(text, cursor);
		const Line first(text, 0);
		cursor = first.to_absolute(current.to_relative(cursor));
		cursor_clamp(row_count);
	}

	void jump_down(unsigned skip, unsigned row_count) {
		for (unsigned i = 0; i < skip; i++) { next_line(row_count); }
		cursor_clamp(row_count);
	}

	void jump_up(unsigned skip, unsigned row_count) {
		for (unsigned i = 0; i < skip; i++) { prev_line(row_count); }
		cursor_clamp(row_count);
	}

	void window_down(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned down_row = cursor_row + row_count / 2;
		const unsigned skip = down_row > cursor_row ? down_row - cursor_row : 0;
		for (unsigned i = 0; i < skip; i++) { next_line(row_count); }
		cursor_clamp(row_count);
	}

	void window_up(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned up_row = cursor_row > row_count / 2 ? cursor_row - row_count / 2 : 0;
		const unsigned skip = cursor_row - up_row;
		for (unsigned i = 0; i < skip; i++) { prev_line(row_count); }
		cursor_clamp(row_count);
	}

	void window_top(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned top_row = begin_row;
		const unsigned skip = cursor_row - top_row;
		for (unsigned i = 0; i < skip; i++) { prev_line(row_count); }
		cursor_clamp(row_count);
	}

	void window_center(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned middle_row = begin_row + row_count / 2;
		const unsigned skip = middle_row > cursor_row ? middle_row - cursor_row : cursor_row - middle_row;
		for (unsigned i = 0; i < skip; i++) { middle_row > cursor_row ? next_line(row_count) : prev_line(row_count); }
		cursor_clamp(row_count);
	}

	void window_bottom(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		const unsigned bottom_row = begin_row + row_count;
		const unsigned skip = bottom_row > cursor_row ? bottom_row - cursor_row : 0;
		for (unsigned i = 0; i < skip; i++) { next_line(row_count); }
		cursor_clamp(row_count);
	}

	void cursor_clamp(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		begin_row = std::clamp(begin_row, cursor_row > row_count ? cursor_row - row_count : 0, cursor_row);
	}

	void cursor_center(unsigned row_count) {
		const unsigned cursor_row = find_cursor_row();
		begin_row = cursor_row > row_count / 2 ? cursor_row - row_count / 2 : 0;
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

	std::string copy_line_whitespace() {
		std::string res;
		if (text.size() > 0) {
			const Line current(text, cursor);
			size_t pos = current.begin();
			while (pos < text.size() && is_line_whitespace(text[pos])) {
				res += text[pos];
				pos++;
			}
		}
		return res;
	}

	std::string yank_line() {
		if (text.size() > 0) {
			const Line current(text, cursor);
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

	std::string yank_word() {
		if (text.size() > 0) {
			Word current(text, cursor);
			return text.substr(current.begin(), current.end() - current.begin() + 1);
		}
		return {};
	}

	std::string yank_words(unsigned count) {
		std::string s;
		if (text.size() > 0) {
			Word current(text, cursor);
			const size_t begin = cursor;
			size_t end = begin;
			for (unsigned i = 0; i < count; i++) {
				end = current.end();
				current = incr(current);
			}
			const auto count = std::min(end + 1, text.size() - 1) - begin;
			s += text.substr(begin, count);
		}
		return s;
	}

	std::string yank_enclosure(uint16_t left, uint16_t right, bool inclusive) {
		if (text.size() > 0 && cursor < text.size()) {
			const Enclosure current(text, cursor, left, right);
			if (current.valid()) {
				const auto begin = inclusive ? current.begin() : current.begin() + 1;
				const auto end = inclusive ? current.end() : current.end() - 1;
				return text.substr(begin, end - begin + 1);
			}
		}
		return {};
	}

	std::string yank_lines_down(unsigned count) {
		std::string s;
		if (text.size() > 0) {
			size_t begin = cursor;
			for (unsigned i = 0; i <= count; i++) {
				if (begin < text.size() - 1) {
					const Line current(text, begin);
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
					const Line current(text, begin);
					s.insert(0, text.substr(current.begin(), current.end() - current.begin() + 1));
					if (current.begin() == 0) break;
					begin = current.begin() - 1;
				}
			}
		}
		return s;
	}

	void insert(const std::string_view s) {
		text.insert(cursor, s);
		cursor = std::min(cursor + s.length(), text.size() - 1);
	}

	void erase_back() {
		if (cursor > 0) {
			text.erase(cursor - 1, 1);
			cursor = cursor > 0 ? cursor - 1 : cursor;
		}
	}

	std::string erase() {
		if (text.size() > 0) {
			const auto s = text.substr(cursor, 1);
			text.erase(cursor, 1);
			cursor = text.size() > 0 && cursor == text.size() ? cursor - 1 : cursor;
			return s;
		}
		return {};
	}

	std::string erase_if(char c) {
		if (text.size() > 0 && text[cursor] == c) {
			const auto s = text.substr(cursor, 1);
			text.erase(cursor, 1);
			cursor = text.size() > 0 && cursor == text.size() ? cursor - 1 : cursor;
			return s;
		}
		return {};
	}

	std::string erase_all_up() {
		if (text.size() > 0) {
			const auto s = text.substr(0, cursor);
			text.erase(0, cursor);
			cursor = 0;
			return s;
		}
		return {};
	}

	std::string erase_all_down() {
		if (text.size() > 0) {
			const auto s = text.substr(cursor, text.size() - cursor);
			text.erase(cursor, text.size() - cursor);
			cursor = std::min(cursor, text.size() - 1);
			return s;
		}
		return {};
	}

	std::string erase_to(unsigned key) {
		if (text.size() > 0) {
			if (const auto pos = find_char(key); pos != std::string::npos) {
				const auto s = text.substr(cursor, pos - cursor + 1);
				text.erase(cursor, pos - cursor + 1);
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
				return s;
			}
		}
		return {};
	}

	std::string erase_line() {
		if (text.size() > 0) {
			const Line current(text, cursor);
			const auto s = text.substr(current.begin(), current.end() - current.begin() + 1);
			text.erase(current.begin(), current.end() - current.begin() + 1);
			cursor = std::min(current.begin(), text.size() - 1);
			return s;
		}
		return {};
	}

	std::string erase_line_contents() {
		if (text.size() > 0) {
			const Line current(text, cursor);
			const auto s = text.substr(current.begin(), current.end() - current.begin());
			text.erase(current.begin(), current.end() - current.begin());
			cursor = std::min(current.begin(), text.size() - 1);
			return s;
		}
		return {};
	}

	std::string erase_to_line_end() {
		if (text.size() > 0) {
			const Line current(text, cursor);
			const auto s = text.substr(cursor, current.end() - cursor);
			text.erase(cursor, current.end() - cursor);
			cursor = std::min(cursor, text.size() - 1);
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

	std::string erase_lines_up(unsigned count, unsigned row_count) {
		std::string s;
		bool first_line = false;
		for (unsigned i = 0; i <= count; i++) {
			if (first_line) break; // Don't erase twice.
			first_line = is_first_line();
			s.insert(0, erase_line());
			prev_line(row_count);
		}
		return s;
	}

	std::string erase_word(bool from_cursor) {
		if (text.size() > 0 && cursor < text.size()) {
			const Word current(text, cursor);
			const auto begin = from_cursor ? cursor : current.begin();
			const auto count = std::min(current.end() + 1, text.size() - 1) - begin;
			const auto s = text.substr(begin, count);
			text.erase(begin, count);
			cursor = begin;
			return s;
		}
		return {};
	}

	std::string erase_words(unsigned count) {
		std::string s;
		for (unsigned i = 0; i < count; i++) {
			s += erase_word(i == 0);
		}
		return s;
	}

	std::string erase_enclosure(uint16_t left, uint16_t right, bool inclusive) {
		if (text.size() > 0 && cursor < text.size()) {
			const Enclosure current(text, cursor, left, right);
			if (current.valid()) {
				const auto begin = inclusive ? current.begin() : current.begin() + 1;
				const auto end = inclusive ? current.end() : current.end() - 1;
				const auto s = text.substr(begin, end - begin + 1);
				text.erase(begin, end - begin + 1);
				cursor = begin;
				return s;
			}
		}
		return {};
	}

	void paste_before(const std::string_view s, unsigned row_count) {
		if (s.find("\n") != std::string::npos) {
			line_start(); insert(s); prev_line(row_count);
		}
		else {
			insert(s);
		}
	}

	void paste_after(const std::string_view s, unsigned row_count) {
		if (s.find("\n") != std::string::npos) {
			next_line(row_count); line_start(); insert(s); prev_line(row_count);
		}
		else {
			next_char(); insert(s);
		}
	}

	void indent_right(unsigned count) {
		line_start_whitespace();
		for (unsigned i = 0; i < count; ++i)
			insert("\t");
	}

	void indent_right_down(unsigned count, unsigned row_count) {
		line_start_whitespace();
		insert("\t");
		for (unsigned i = 0; i < count; ++i) {
			next_line(row_count);
			line_start_whitespace();
			insert("\t");
		}
	}

	void indent_right_up(unsigned count, unsigned row_count) {
		line_start_whitespace();
		insert("\t");
		for (unsigned i = 0; i < count; ++i) {
			prev_line(row_count);
			line_start_whitespace();
			insert("\t");
		}
	}

	void indent_left(unsigned count) {
		line_start();
		for (unsigned i = 0; i < count; ++i)
 			erase_if('\t');
		line_start_whitespace();
	}

	void indent_left_down(unsigned count, unsigned row_count) {
		line_start();
		erase_if('\t');
		for (unsigned i = 0; i < count; ++i) {
			next_line(row_count);
			line_start();
 			erase_if('\t');
		}
	}

	void indent_left_up(unsigned count, unsigned row_count) {
		line_start();
		erase_if('\t');
		for (unsigned i = 0; i < count; ++i) {
			prev_line(row_count);
			line_start();
			erase_if('\t');
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
		states.clear();
		states.emplace_back();
		states.back().fix_eof();
	}

	State& state() { return states.back(); }
	const State& state() const { return states.back(); }

	const std::string_view get_text() const { return states.back().get_text(); }
	void set_text(const std::string_view t) { states.back().set_text(t); }

	size_t get_cursor() const { return states.back().get_cursor(); }
	void set_cursor(size_t u) { states.back().set_cursor(u); }

	void set_undo() { undo = true; }

	void push() {
		if (states.size() > 100) { states.erase(states.begin()); }
		if (states.size() > 0) { states.push_back(states.back()); }
	}

	bool pop() {
		bool modified = false;
		if (states.size() > 1) {
			auto& last = states[states.size() - 1];
			auto& previous = states[states.size() - 2];
			modified = last.get_text() != previous.get_text();
			if (!modified) {
				std::swap(previous, last);
				states.pop_back();
			}
		}
		if (undo) {
			undo = false;
			if (states.size() > 1)
				states.pop_back();
		}
		states.back().fix_eof();
		return modified;
	}
};

