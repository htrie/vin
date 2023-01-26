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

enum Glyph {
	BS = 8,
	TAB = 9,
	CR = 13,
	ESC = 27,
	BLOCK = 9608,
	LINE = 9615,
	BOTTOM_BLOCK = 9601,
	SPACESIGN = 8901,
	TABSIGN = 9205,
	CARRIAGE = 9204,
	RETURN = 9207,
	UNKNOWN = 65533,
};

constexpr bool is_number(char c) { return (c >= '0' && c <= '9'); }
constexpr bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'); }
constexpr bool is_lowercase_letter(uint16_t c) { return (c >= 'a' && c <= 'z'); }
constexpr bool is_uppercase_letter(uint16_t c) { return (c >= 'A' && c <= 'Z'); }
constexpr bool is_whitespace(char c) { return c == '\n' || c == '\t' || c == ' '; }
constexpr bool is_line_whitespace(char c) { return c == '\t' || c == ' '; }
constexpr bool is_punctuation(char c) { return 
	c == '-' || c == '+' || c == '*' || c == '/' || c == '=' || c == '\\' ||
	c == ',' || c == '.' || c == '<' || c == '>' || c == ';' || c == ':' ||
	c == '[' || c == ']' || c == '{' || c == '}' || c == '(' || c == ')' ||
	c == '&' || c == '|' || c == '%' || c == '^' || c == '!' || c == '~' ||
	c == '?' || c == '"' || c == '#' || c == '\'';
}

bool is_code_extension(const std::string_view filename) {
	if (filename.ends_with(".cpp")) return true;
	if (filename.ends_with(".hpp")) return true;
	if (filename.ends_with(".c")) return true;
	if (filename.ends_with(".h")) return true;
	return false;
}

static inline const std::vector<std::vector<const char*>> cpp_keywords = {
	{ "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto" },
	{ "bitand", "bitor", "bool", "break" },
	{ "case", "catch", "char", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue" },
	{ "decltype", "default", "define", "delete", "do", "double", "dynamic_cast" },
	{ "else", "endif", "enum", "explicit", "export", "extern" },
	{ "false", "float", "for", "friend" },
	{ "goto" },
	{ },
	{ "include", "if", "inline", "int","int8_t",  "int16_t", "int32_t",  "int64_t" },
	{ },
	{ },
	{ "long" },
	{ "mutable" },
	{ "namespace", "new", "noexcept", "not", "not_eq", "nullptr" },
	{ "operator", "or", "or_eq" },
	{ "pragma", "private", "protected", "public" },
	{ },
	{ "reflexpr", "register", "reinterpret_cast", "requires", "return" },
	{ "short", "signed", "size_t", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized" },
	{ "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename" },
	{ "union", "unsigned", "using", "uint8_t", "uint16_t", "uint32_t", "uint64_t" },
	{ "virtual", "void", "volatile" },
	{ "wchar_t", "while" },
	{ "xor", "xor_eq" },
	{ },
	{ }
};

std::string to_lower(const std::string_view s) {
	auto res = std::string(s);
	std::transform(res.begin(), res.end(), res.begin(), [](const auto c){ return std::tolower(c); });
	return res;
}

static unsigned compute_letter_index(const uint16_t c) {
	if (c >= 'a' && c <= 'z') return c - 'a';
	return (unsigned)-1;
}

struct Character {
	uint16_t index;
	Color color = Color::rgba(255, 0, 0, 255);
	float row = 0.0f;
	float col = 0.0f;
	bool bold = false;
	bool italic = false;

	Character() {}
	Character(uint16_t index, Color color, float row, float col, bool bold = false, bool italic = false)
		: index(index), color(color), row(row), col(col), bold(bold), italic(italic) {}
	Character(uint16_t index, Color color, unsigned row, unsigned col, bool bold = false, bool italic = false)
		: index(index), color(color), row((float)row), col((float)col), bold(bold), italic(italic) {}
};

typedef std::vector<Character> Characters;

class Word {
	size_t start = 0;
	size_t finish = 0;

	bool has_letter_or_number = false;
	bool has_whitespace = false;
	bool has_punctuation = false;

	bool test_letter_or_number(const std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_number(text[pos]) || is_letter(text[pos]);
		}
		return false;
	}

	bool test_whitespace(const std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_line_whitespace(text[pos]);
		}
		return false;
	}

	bool test_punctuation(const std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_punctuation(text[pos]);
		}
		return false;
	}

public:
	Word(const std::string_view text, size_t pos) {
		if (text.size() > 0) {
			verify(pos < text.size());
			start = pos;
			finish = pos;
			const auto c = text[pos];
			if (is_number(c) || is_letter(c)) {
				while(test_letter_or_number(text, finish + 1)) { finish++; }
				while(test_letter_or_number(text, start - 1)) { start--; }
				has_letter_or_number = true;
			}
			else if (is_whitespace(c)) {
				while(test_whitespace(text, finish + 1)) { finish++; }
				while(test_whitespace(text, start - 1)) { start--; }
				has_whitespace = true;
			}
			else if (is_punctuation(c)) {
				while(test_punctuation(text, finish + 1)) { finish++; }
				while(test_punctuation(text, start - 1)) { start--; }
				has_punctuation = true;
			}
			verify(start <= finish);
		}
	}

	size_t begin() const { return start; }
	size_t end() const { return finish; }

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish - start + 1);
	}

	bool check_letter_or_number() const { return has_letter_or_number; }
	bool check_punctuation() const { return has_punctuation; }
	bool check_whitespace() const { return has_whitespace; }

	bool check_keyword(const std::string_view text) const {
		if (const auto letter_index = compute_letter_index(text[start]); letter_index != (unsigned)-1) {
			const auto& keywords = cpp_keywords[letter_index];
			const std::string word(text.substr(start, finish - start + 1));
			for (const auto& keyword : keywords) {
				if (strcmp(word.data(), keyword) == 0)
					return true;
			}
		}
		return false;
	}
};

class Enclosure {
	size_t start = 0;
	size_t finish = 0;

	size_t find_prev(const std::string_view text, size_t pos, uint16_t left, uint16_t right) {
		unsigned count = 1;
		size_t index = pos > 0 && text[pos] == right ? pos - 1 : pos;
		while (index < text.size() && count > 0) {
			if (text[index] == right) count++;
			if (text[index] == left) count--;
			if (count > 0)
				index--;
		}
		return count == 0 ? index : std::string::npos;
	}

	size_t find_next(const std::string_view text, size_t pos, uint16_t left, uint16_t right) {
		unsigned count = 1;
		size_t index = text[pos] == left ? pos + 1 : pos;
		while (index < text.size() && count > 0) {
			if (text[index] == left) count++;
			if (text[index] == right) count--;
			if (count > 0)
				index++;
		}
		return count == 0 ? index : std::string::npos;
	}

public:
	Enclosure(const std::string_view text, size_t pos, uint16_t left, uint16_t right) {
		if (text.size() > 0) {
			verify(pos < text.size());
			start = pos;
			finish = pos;
			const auto prev = find_prev(text, pos, left, right);
			const auto next = find_next(text, pos, left, right);
			if (prev != std::string::npos && next != std::string::npos) {
				start = prev != std::string::npos ? prev : pos;
				finish = next != std::string::npos ? next : pos;
			}
			verify(start <= finish);
		}
	}

	bool valid() const { return start < finish; }

	size_t begin() const { return start; }
	size_t end() const { return finish; }
};

class Line {
	size_t start = 0;
	size_t finish = 0;

public:
	Line(const std::string_view text, size_t pos) {
		if (text.size() > 0) {
			verify(pos < text.size());
			const auto pn = text.rfind("\n", pos > 0 && text[pos] == '\n' ? pos - 1 : pos);
			const auto nn = text.find("\n", pos);
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

	bool check_string(const std::string_view text, const std::string_view s) const {
		if (start + s.size() <= text.size())
			return strncmp(&text[start], s.data(), s.size()) == 0;
		return false;
	}
};

class Comment {
	size_t start = 0;
	size_t finish = 0;

public:
	Comment(const std::string_view text, size_t pos) {
		if (text.size() > 0) {
			verify(pos < text.size());
			const auto pn = text.rfind("\n", pos > 0 && text[pos] == '\n' ? pos - 1 : pos);
			const auto nn = text.find("\n", pos);
			const auto begin = pn != std::string::npos && pn < pos ? pn : 0;
			const auto end = nn != std::string::npos && nn > pos ? nn : text.size() - 1; 
			const auto n = text.find("//", begin);
			if (n != std::string::npos && n < end) {
				start = n;
				finish = end;
				verify(start <= finish);
			}
		}
	}

	bool valid() const { return start < finish; }
	bool contains(size_t pos) const { return start <= pos && pos < finish; }
};

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

	void change_case() {
		const char c = text[cursor];
		char res = c;
		if (std::islower(c)) { res = std::toupper(c);
		} else { res = std::tolower(c); }
		if (res != c) {
			text[cursor] = res;
		}
	}

	std::string get_word() const {
		const Word current(text, cursor);
		if (!is_whitespace(text[current.begin()]))
			return text.substr(current.begin(), current.end() - current.begin() + 1);
		return "";
	}

	std::string current_word() {
		const Word current(text, cursor);
		if (is_whitespace(text[current.begin()])) {
			const Word next = incr(current);
			cursor = next.begin();
			return text.substr(next.begin(), next.end() - next.begin() + 1);
		}
		return text.substr(current.begin(), current.end() - current.begin() + 1);
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

	void next_line() {
		const Line current(text, cursor);
		const Line next = incr(current);
		cursor = next.to_absolute(current.to_relative(cursor));
	}

	void prev_line() {
		const Line current(text, cursor);
		const Line prev = decr(current);
		cursor = prev.to_absolute(current.to_relative(cursor));
	}

	void buffer_end() {
		const Line current(text, cursor);
		const Line last(text, text.size() - 1);
		cursor = last.to_absolute(current.to_relative(cursor));
	}

	void buffer_start() {
		const Line current(text, cursor);
		const Line first(text, 0);
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

	void paste_before(const std::string_view s) {
		if (s.find("\n") != std::string::npos) {
			line_start(); insert(s); prev_line();
		}
		else {
			insert(s);
		}
	}

	void paste_after(const std::string_view s) {
		if (s.find("\n") != std::string::npos) {
			next_line(); line_start(); insert(s); prev_line();
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

	void indent_right_down(unsigned count) {
		line_start_whitespace();
		insert("\t");
		for (unsigned i = 0; i < count; ++i) {
			next_line();
			line_start_whitespace();
			insert("\t");
		}
	}

	void indent_right_up(unsigned count) {
		line_start_whitespace();
		insert("\t");
		for (unsigned i = 0; i < count; ++i) {
			prev_line();
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

	void indent_left_down(unsigned count) {
		line_start();
		erase_if('\t');
		for (unsigned i = 0; i < count; ++i) {
			next_line();
			line_start();
 			erase_if('\t');
		}
	}

	void indent_left_up(unsigned count) {
		line_start();
		erase_if('\t');
		for (unsigned i = 0; i < count; ++i) {
			prev_line();
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
		reset();
	}

	void reset() {
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

