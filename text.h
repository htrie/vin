#pragma once

enum class Mode {
	normal,
	normal_number,
	normal_slash,
	normal_question,
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
	RETURN = 9166,
	BOTTOM_BLOCK = 9601,
	TABSIGN = 9205,
	SPACESIGN = 8228,
};

Vec3 hsv_to_rgb(float H, float S, float V) {
  const float C = V * S; // Chroma
  const float HPrime = std::fmod(H / 60.0f, 6.0f);
  const float X = C * (1.0f - std::fabs(std::fmod(HPrime, 2.0f) - 1.0f));
  const float M = V - C;
  
  float R, G, B;
  if(0.0f <= HPrime && HPrime < 1.0f) { R = C; G = X; B = 0.0f; } 
  else if(1.0f <= HPrime && HPrime < 2.0f) { R = X; G = C; B = 0.0f; } 
  else if(2.0f <= HPrime && HPrime < 3.0f) { R = 0.0f; G = C; B = X; } 
  else if(3.0f <= HPrime && HPrime < 4.0f) { R = 0.0f; G = X; B = C; } 
  else if(4.0f <= HPrime && HPrime < 5.0f) { R = X; G = 0.0f; B = C; } 
  else if(5.0f <= HPrime && HPrime < 6.0f) { R = C; G = 0.0f; B = X; } 
  else { R = 0.0f; G = 0.0f; B = 0.0f; }
  
  R += M; G += M; B += M;
  return Vec3(R, G, B);
}

constexpr bool is_number(char c) { return (c >= '0' && c <= '9'); }
constexpr bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'); }
constexpr bool is_lowercase_letter(uint16_t c) { return (c >= 'a' && c <= 'z'); }
constexpr bool is_uppercase_letter(uint16_t c) { return (c >= 'A' && c <= 'Z'); }
constexpr bool is_whitespace(char c) { return c == '\n' || c == '\t' || c == ' '; }
constexpr bool is_line_whitespace(char c) { return c == '\t' || c == ' '; }
constexpr bool is_quote(char c) { return c == '\'' || c == '"'; }
constexpr bool is_punctuation(char c) { return 
	c == '-' || c == '+' || c == '*' || c == '/' || c == '=' || c == '\\' ||
	c == ',' || c == '.' || c == '<' || c == '>' || c == ';' || c == ':' ||
	c == '[' || c == ']' || c == '{' || c == '}' || c == '(' || c == ')' ||
	c == '&' || c == '|' || c == '%' || c == '^' || c == '!' || c == '~' || c == '?';
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
	{ "if", "inline", "int","int8_t",  "int16_t", "int32_t",  "int64_t" },
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

static unsigned compute_letter_index(const uint16_t c) {
	if (c >= 'a' && c <= 'z') return c - 'a';
	return (unsigned)-1;
}

struct Character {
	uint16_t index;
	Color color = Color::rgba(255, 0, 0, 255);
	unsigned row = 0;
	unsigned col = 0;
	bool bold = false;
	bool italic = false;

	Character(uint16_t index, Color color, unsigned row, unsigned col, bool bold = false, bool italic = false)
		: index(index), color(color), row(row), col(col), bold(bold), italic(italic) {}
};

typedef std::vector<Character> Characters;

class Word {
	size_t start = 0;
	size_t finish = 0;

	bool has_letter_or_number = false;
	bool has_whitespace = false;
	bool has_punctuation = false;

	bool test_letter_or_number(std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_number(text[pos]) || is_letter(text[pos]);
		}
		return false;
	}

	bool test_whitespace(std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_line_whitespace(text[pos]);
		}
		return false;
	}

	bool test_punctuation(std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_punctuation(text[pos]);
		}
		return false;
	}

public:
	Word(std::string_view text, size_t pos) {
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

	bool check_letter_or_number() const { return has_letter_or_number; }
	bool check_punctuation() const { return has_punctuation; }
	bool check_whitespace() const { return has_whitespace; }

	bool check_keyword(const std::string_view text) const {
		if (const auto letter_index = compute_letter_index(text[start]); letter_index != (unsigned)-1) {
			const auto& keywords = cpp_keywords[letter_index];
			const std::string_view word(text.substr(start, finish - start + 1));
			for (const auto& keyword : keywords) {
				if (word == keyword)
					return true;
			}
		}
		return false;
	}

	bool check_class(const std::string_view text) const {
		return is_uppercase_letter(text[start]);
	}

	float generate_channel(const std::string_view text, unsigned offset) const {
		const size_t len = finish - start;
		if (len < offset) return 0.0f;
		const char c = text[start + offset];
		if (c == '_') return 1.0f;
		if (!is_letter(c)) return 1.0f;
		const unsigned a = std::tolower(c);
		return (float)(a - 'a') / 26.0f; 
	}

	Color generate_color(const std::string_view text, float h_start, float h_range, float h_adjust, float s, float v) const {
		const auto x = generate_channel(text, 0);
		const auto y = generate_channel(text, 1);
		const auto z = generate_channel(text, 2);
		const auto i = generate_channel(text, 4);
		const auto j = generate_channel(text, 5);
		const auto k = generate_channel(text, 6);
		const auto rand0 = (x * 1000.0f + y * 100.0f + z * 10.0f) / 1000.0f;
		const auto rand1 = (i * 1000.0f + j * 100.0f + k * 10.0f) / 1000.0f;
		const auto h = std::fmod(h_start + rand0 * h_range + rand1 * h_adjust, 360.0f); // [0:60:120:180:240:300:360] (red-yellow-green-cyan-blue-magenta-red)
		const auto rgb = hsv_to_rgb(h, s, v);
		return Color::rgba(
			unsigned(rgb.x * 255.0f),
			unsigned(rgb.y * 255.0f),
			unsigned(rgb.z * 255.0f),
			255u);
	}
};

class Enclosure {
	size_t start = 0;
	size_t finish = 0;

	size_t find_prev(std::string_view text, size_t pos, uint16_t left, uint16_t right) {
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

	size_t find_next(std::string_view text, size_t pos, uint16_t left, uint16_t right) {
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
	Enclosure(std::string_view text, size_t pos, uint16_t left, uint16_t right) {
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

	bool check_string(const std::string_view text, const std::string_view s) const {
		return text.substr(start, s.size()) == s;
	}
};

class Comment {
	size_t start = 0;
	size_t finish = 0;

public:
	Comment(std::string_view text, size_t pos) {
		if (text.size() > 0) {
			verify(pos < text.size());
			const auto n = text.rfind("//", pos);
			const auto pn = text.rfind('\n', pos > 0 && text[pos] == '\n' ? pos - 1 : pos);
			const auto nn = text.find('\n', pos);
			if (n != std::string::npos && pn < n) {
				start = n;
				finish = nn != std::string::npos ? nn : text.size() - 1;
				verify(start <= finish);
			}
		}
	}

	bool valid() const { return start < finish; }
};

class State {
	std::string text;
	size_t cursor = 0;
	unsigned begin_row = 0;

public:
	const std::string& get_text() const { return text; }
	void set_text(const std::string& t) { text = t; }

	size_t get_cursor() const { return cursor; }
	void set_cursor(size_t u) { cursor = u; }

	unsigned get_begin_row() const { return begin_row; }

	Word incr(const Word& w) { return Word(text, w.end() < text.size() - 1 ? w.end() + 1 : w.end()); }
	Word decr(const Word& w) { return Word(text, w.begin() > 0 ? w.begin() - 1 : 0); }

	Line incr(const Line& w) { return Line(text, w.end() < text.size() - 1 ? w.end() + 1 : w.end()); }
	Line decr(const Line& w) { return Line(text, w.begin() > 0 ? w.begin() - 1 : 0); }

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

	void word_find(const std::string_view s) {
		if (const auto pos = text.find(s, cursor); pos != std::string::npos) {
			if (pos == cursor && (cursor + 1 < text.size())) {
				if (const auto pos = text.find(s, cursor + 1); pos != std::string::npos) { cursor = pos; }
				else if (const auto pos = text.find(s, 0); pos != std::string::npos) { cursor = pos; }
			}
			else { cursor = pos; }
		}
		else if (const auto pos = text.find(s, 0); pos != std::string::npos) { cursor = pos; }
	}

	void word_rfind(const std::string_view s) {
		if (const auto pos = text.rfind(s, cursor); pos != std::string::npos) {
			if (pos == cursor && cursor > 0) {
				if (const auto pos = text.rfind(s, cursor - 1); pos != std::string::npos) { cursor = pos; }
				else if (const auto pos = text.rfind(s, text.size() - 1); pos != std::string::npos) { cursor = pos; }
			}
			else { cursor = pos; }
		}
		else if (const auto pos = text.rfind(s, text.size() - 1); pos != std::string::npos) { cursor = pos; }
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

class Buffer {
	Stack stack;

	Mode mode = Mode::normal;

	std::vector<unsigned> record;
	std::vector<unsigned> temp_record;
	bool repeat = false;

	std::string filename;
	std::string clipboard;
	std::string highlight;

	unsigned accu = 0;

	unsigned f_key = 0;
	size_t cursor = 0;

	bool char_forward = false;
	bool word_forward = false;

	bool needs_save = false;

	size_t last_cursor = 0;

	float hue_start = 0.0f;
	float hue_range = 260.0f;
	float hue_adjust = 10.0f;
	float saturation = 0.50f;
	float brightness = 0.85f;

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

	void replay(unsigned col_count, unsigned row_count) {
		const auto s = record; // Cache since process_key will modify it.
		for (const auto& c : s) {
			process_key(c, col_count, row_count);
		}
	}

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

	void line_find() {
		if(char_forward) { state().line_find(f_key); }
		else { state().line_rfind(f_key); }
		mode = Mode::normal;
	}

	void line_rfind() {
		if(char_forward) { state().line_rfind(f_key); }
		else { state().line_find(f_key); }
		mode = Mode::normal;
	}

	void word_find_under_cursor(unsigned row_count) {
		highlight = state().current_word();
		word_forward = true;
		state().word_find(highlight);
		state().cursor_center(row_count);
	}

	void word_rfind_under_cursor(unsigned row_count) {
		highlight = state().current_word();
		word_forward = false;
		state().word_rfind(highlight);
		state().cursor_center(row_count);
	}

	void word_find_partial(unsigned row_count) {
		word_forward = true;
		if (!state().current_word().starts_with(highlight)) {
			state().word_find(highlight);
			state().cursor_center(row_count);
		}
	}

	void word_rfind_partial(unsigned row_count) {
		word_forward = false;
		if (!state().current_word().starts_with(highlight)) {
			state().word_rfind(highlight);
			state().cursor_center(row_count);
		}
	}

	void word_find_again(unsigned row_count) {
		if (word_forward) { state().word_find(highlight); }
		else { state().word_rfind(highlight); }
		state().cursor_center(row_count);
	}

	void word_rfind_again(unsigned row_count) {
		if (word_forward) { state().word_rfind(highlight); }
		else { state().word_find(highlight); }
		state().cursor_center(row_count);
	}

	void process_key(unsigned key, unsigned col_count, unsigned row_count) {
		if (mode != Mode::insert) {
			stack.push();
		}

		switch (mode) {
		case Mode::normal: process_normal(key, row_count); break;
		case Mode::normal_number: process_normal_number(key); break;
		case Mode::normal_slash: process_normal_slash(key, row_count); break;
		case Mode::normal_question: process_normal_question(key, row_count); break;
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

		if (mode != Mode::insert) {
			needs_save = stack.pop() || needs_save;
		}
	}

	void process_insert(unsigned key) {
		if (key == Glyph::ESC) { end_record(key); mode = Mode::normal; }
		else if (key == Glyph::BS) { append_record(key); state().erase_back(); }
		else if (key == Glyph::TAB) { append_record(key); state().insert("\t"); }
		else if (key == Glyph::CR) { append_record(key); state().insert("\n"); }
		else { append_record(key); state().insert(std::string(1, (char)key)); }
	}

	void process_normal(unsigned key, unsigned row_count) {
		if (key == 'u') { stack.set_undo(); }
		else if (key >= '0' && key <= '9') { accumulate(key); mode = Mode::normal_number; }
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
		else if (key == 'o') { begin_record(key); state().line_end(); state().insert("\n"); mode = Mode::insert; }
		else if (key == 'O') { begin_record(key); state().line_start(); state().insert("\n"); state().prev_line(); mode = Mode::insert; }
		else if (key == 's') { begin_record(key); state().erase(); mode = Mode::insert; }
		else if (key == 'S') { begin_record(key); clip(state().erase_line_contents()); mode = Mode::insert; }
		else if (key == 'C') { begin_record(key); clip(state().erase_to_line_end()); mode = Mode::insert; }
		else if (key == 'x') { begin_end_record(key); clip(state().erase()); }
		else if (key == 'D') { begin_end_record(key); clip(state().erase_to_line_end()); }
		else if (key == 'J') { begin_end_record(key); state().line_end(); state().erase(); state().remove_line_whitespace(); state().insert(" "); state().prev_char(); }
		else if (key == '<') { begin_end_record(key); state().line_start(); state().erase_if('\t'); state().line_start_whitespace(); }
		else if (key == '>') { begin_end_record(key); state().line_start_whitespace(); state().insert("\t"); }
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
		else if (key == 'g') { save_cursor(); state().buffer_start(); }
		else if (key == 'G') { save_cursor(); state().buffer_end(); }
		else if (key == 'H') { save_cursor(); state().window_top(row_count); }
		else if (key == 'M') { save_cursor(); state().window_center(row_count); }
		else if (key == 'L') { save_cursor(); state().window_bottom(row_count); }
		else if (key == '+') { save_cursor(); state().next_line(); state().line_start_whitespace(); }
		else if (key == '-') { save_cursor(); state().prev_line(); state().line_start_whitespace(); }
		else if (key == ';') { line_find(); }
		else if (key == ',') { line_rfind(); }
		else if (key == '*') { save_cursor(); word_find_under_cursor(row_count); }
		else if (key == '#') { save_cursor(); word_rfind_under_cursor(row_count); }
		else if (key == '/') { highlight.clear(); mode = Mode::normal_slash; }
		else if (key == '?') { highlight.clear(); mode = Mode::normal_question; }
		else if (key == 'n') { word_find_again(row_count); }
		else if (key == 'N') { word_rfind_again(row_count); }
		else if (key == '.') { repeat = true; }
		else if (key == '\'') { load_cursor(); }
	}

	void process_normal_number(unsigned key) {
		if (key >= '0' && key <= '9') { accumulate(key); }
		else if (key == 'j') { save_cursor(); state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { save_cursor(); state().jump_up(accu); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { save_cursor(); state().buffer_start(); state().jump_down(accu); accu = 0; mode = Mode::normal; }
		else { accu = 0; mode = Mode::normal; }
	}

	void process_normal_slash(unsigned key, unsigned row_count) {
		if (key == Glyph::ESC) { highlight.clear(); mode = Mode::normal; }
		else if (key == Glyph::CR) { word_find_partial(row_count); mode = Mode::normal; }
		else if (key == Glyph::BS) { if (highlight.size() > 0) { highlight.pop_back(); word_find_partial(row_count); } }
		else { highlight += key; word_find_partial(row_count); }
	}

	void process_normal_question(unsigned key, unsigned row_count) {
		if (key == Glyph::ESC) { highlight.clear(); mode = Mode::normal; }
		else if (key == Glyph::CR) { word_rfind_partial(row_count); mode = Mode::normal; }
		else if (key == Glyph::BS) { if (highlight.size() > 0) { highlight.pop_back(); word_rfind_partial(row_count); } }
		else { highlight += key; word_rfind_partial(row_count); }
	}
	void process_normal_f(unsigned key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { state().line_find(key); f_key = key; char_forward = true; mode = Mode::normal; }
	}

	void process_normal_F(unsigned key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { state().line_rfind(key); f_key = key; char_forward = false; mode = Mode::normal; }
	}

	void process_normal_r(unsigned key) {
		if (key == Glyph::ESC) { mode = Mode::normal; }
		else { end_record(key); clip(state().erase()); state().insert(std::string(1, (char)key)); state().prev_char(); mode = Mode::normal; }
	}

	void process_normal_z(unsigned key, unsigned row_count) {
		if (key == 'z') { state().cursor_center(row_count); mode = Mode::normal; }
		else if (key == 't') { state().cursor_top(row_count); mode = Mode::normal; }
		else if (key == 'b') { state().cursor_bottom(row_count); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_y(unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == 'y') { end_record(key); clip(state().yank_line()); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { end_record(key); clip(state().yank_words(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { end_record(key); clip(state().yank_all_up()); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { end_record(key); clip(state().yank_all_down()); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { end_record(key); clip(state().yank_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { end_record(key); clip(state().yank_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { append_record(key); accu = 0; mode = Mode::normal_yf; }
		else if (key == 't') { append_record(key); accu = 0; mode = Mode::normal_yt; }
		else if (key == 'i') { append_record(key); accu = 0; mode = Mode::normal_yi; }
		else if (key == 'a') { append_record(key); accu = 0; mode = Mode::normal_ya; }
		else { mode = Mode::normal; }
	}

	void process_normal_yf(unsigned key) {
		end_record(key); clip(state().yank_to(key)); mode = Mode::normal;
	}

	void process_normal_yt(unsigned key) {
		end_record(key); clip(state().yank_until(key)); mode = Mode::normal;
	}

	void process_normal_yi(unsigned key) {
		if (key == 'w') { end_record(key); clip(state().yank_word()); mode = Mode::normal; }
		else if (key == '(' || key == ')') { end_record(key); clip(state().yank_enclosure('(', ')', false)); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clip(state().yank_enclosure('{', '}', false)); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clip(state().yank_enclosure('[', ']', false)); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_ya(unsigned key) {
		if (key == '(' || key == ')') { end_record(key); clip(state().yank_enclosure('(', ')', true)); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clip(state().yank_enclosure('{', '}', true)); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clip(state().yank_enclosure('[', ']', true)); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_c(unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == 'c') { append_record(key); clip(state().erase_line_contents()); accu = 0; mode = Mode::insert; }
		else if (key == 'w') { append_record(key); clip(state().erase_words(std::max(1u, accu))); accu = 0; mode = Mode::insert; }
		else if (key == 'g') { append_record(key); clip(state().erase_all_up()); accu = 0; mode = Mode::insert; }
		else if (key == 'G') { append_record(key); clip(state().erase_all_down()); accu = 0; mode = Mode::insert; }
		else if (key == 'j') { append_record(key); clip(state().erase_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::insert; }
		else if (key == 'k') { append_record(key); clip(state().erase_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::insert; }
		else if (key == 'f') { append_record(key); accu = 0; mode = Mode::normal_cf; }
		else if (key == 't') { append_record(key); accu = 0; mode = Mode::normal_ct; }
		else if (key == 'i') { append_record(key); accu = 0; mode = Mode::normal_ci; }
		else if (key == 'a') { append_record(key); accu = 0; mode = Mode::normal_ca; }
		else { mode = Mode::normal; }
	}

	void process_normal_cf(unsigned key) {
		append_record(key); clip(state().erase_to(key)); mode = Mode::insert;
	}

	void process_normal_ct(unsigned key) {
		append_record(key); clip(state().erase_until(key)); mode = Mode::insert;
	}

	void process_normal_ci(unsigned key) {
		if (key == 'w') { append_record(key); clip(state().erase_word(false)); mode = Mode::insert; }
		else if (key == '(' || key == ')') { append_record(key); clip(state().erase_enclosure('(', ')', false)); mode = Mode::insert; }
		else if (key == '{' || key == '}') { append_record(key); clip(state().erase_enclosure('{', '}', false)); mode = Mode::insert; }
		else if (key == '[' || key == ']') { append_record(key); clip(state().erase_enclosure('[', ']', false)); mode = Mode::insert; }
		else { mode = Mode::normal; }
	}

	void process_normal_ca(unsigned key) {
		if (key == '(' || key == ')') { append_record(key); clip(state().erase_enclosure('(', ')', true)); mode = Mode::insert; }
		else if (key == '{' || key == '}') { append_record(key); clip(state().erase_enclosure('{', '}', true)); mode = Mode::insert; }
		else if (key == '[' || key == ']') { append_record(key); clip(state().erase_enclosure('[', ']', true)); mode = Mode::insert; }
		else { mode = Mode::normal; }
	}

	void process_normal_d(unsigned key) {
		if (key >= '0' && key <= '9') { append_record(key); accumulate(key); }
		else if (key == 'd') { end_record(key); clip(state().erase_line()); accu = 0; mode = Mode::normal; }
		else if (key == 'w') { end_record(key); clip(state().erase_words(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'g') { end_record(key); clip(state().erase_all_up()); accu = 0; mode = Mode::normal; }
		else if (key == 'G') { end_record(key); clip(state().erase_all_down()); accu = 0; mode = Mode::normal; }
		else if (key == 'j') { end_record(key); clip(state().erase_lines_down(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'k') { end_record(key); clip(state().erase_lines_up(std::max(1u, accu))); accu = 0; mode = Mode::normal; }
		else if (key == 'f') { append_record(key); accu = 0; mode = Mode::normal_df; }
		else if (key == 't') { append_record(key); accu = 0; mode = Mode::normal_dt; }
		else if (key == 'i') { append_record(key); accu = 0; mode = Mode::normal_di; }
		else if (key == 'a') { append_record(key); accu = 0; mode = Mode::normal_da; }
		else { mode = Mode::normal; }
	}

	void process_normal_df(unsigned key) {
		end_record(key); clip(state().erase_to(key)); mode = Mode::normal;
	}

	void process_normal_dt(unsigned key) {
		end_record(key); clip(state().erase_until(key)); mode = Mode::normal;
	}

	void process_normal_di(unsigned key) {
		if (key == 'w') { end_record(key); clip(state().erase_word(false)); mode = Mode::normal; }
		else if (key == '(' || key == ')') { end_record(key); clip(state().erase_enclosure('(', ')', false)); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clip(state().erase_enclosure('{', '}', false)); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clip(state().erase_enclosure('[', ']', false)); mode = Mode::normal; }
		else { mode = Mode::normal; }
	}

	void process_normal_da(unsigned key) {
		if (key == '(' || key == ')') { end_record(key); clip(state().erase_enclosure('(', ')', true)); mode = Mode::normal; }
		else if (key == '{' || key == '}') { end_record(key); clip(state().erase_enclosure('{', '}', true)); mode = Mode::normal; }
		else if (key == '[' || key == ']') { end_record(key); clip(state().erase_enclosure('[', ']', true)); mode = Mode::normal; }
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

	void push_highlight(Characters& characters, unsigned row, unsigned col) const {
		for (unsigned i = 0; i < (unsigned)highlight.size(); ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().highlight, row, col + i);
		}
	};

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

	void push_space(Characters& characters, unsigned row, unsigned col) const {
		characters.emplace_back(Glyph::SPACESIGN, colors().whitespace, row, col);
	};

	void push_char(Characters& characters, unsigned row, unsigned col, char c, unsigned index) const {
		const Word word(state().get_text(), index);
		const Line line(state().get_text(), index);
		const Comment comment(state().get_text(), index);
		if (index == state().get_cursor() && get_mode() == Mode::normal) { characters.emplace_back((uint16_t)c, colors().text_cursor, row, col); }
		else if (comment.valid()) { characters.emplace_back((uint16_t)c, colors().comment, row, col); }
		else if (line.check_string(state().get_text(), "---")) { characters.emplace_back((uint16_t)c, colors().diff_note, row, col, false, false); }
		else if (line.check_string(state().get_text(), "+")) { characters.emplace_back((uint16_t)c, colors().diff_add, row, col); }
		else if (line.check_string(state().get_text(), "-")) { characters.emplace_back((uint16_t)c, colors().diff_remove, row, col); }
		else if (word.check_keyword(state().get_text())) { characters.emplace_back((uint16_t)c, colors().keyword, row, col, false, false); }
		else if (word.check_class(state().get_text())) { characters.emplace_back((uint16_t)c, word.generate_color(state().get_text(), hue_start, hue_range, hue_adjust, saturation, brightness), row, col, true, false); }
		else if (is_quote(c)) { characters.emplace_back((uint16_t)c, colors().quote, row, col, true); }
		else if (is_punctuation(c)) { characters.emplace_back((uint16_t)c, colors().punctuation, row, col, true); }
		else if (is_whitespace(c)) { characters.emplace_back((uint16_t)c, colors().whitespace, row, col); }
		else { characters.emplace_back((uint16_t)c, word.generate_color(state().get_text(), hue_start, hue_range, hue_adjust, saturation, brightness), row, col); }
	};

	void push_text(Characters& characters, unsigned col_count, unsigned row_count) const {
		const unsigned cursor_row = state().find_cursor_row();
		const unsigned end_row = state().get_begin_row() + row_count;
		unsigned absolute_row = 0;
		unsigned index = 0;
		unsigned row = 0;
		unsigned col = 0;
		for (auto& c : state().get_text()) {
			if (absolute_row >= state().get_begin_row() && absolute_row <= end_row) {
				if (col == 0 && absolute_row == cursor_row) { push_cursor_line(characters, row, col_count); }
				if (col == 0) { push_line_number(characters, row, col, absolute_row, cursor_row); col += 7; }
				if (state().test(index, highlight)) { push_highlight(characters, row, col); }
				if (index == state().get_cursor()) { push_cursor(characters, row, col); }
				if (c == '\n') { push_return(characters, row, col); absolute_row++; row++; col = 0; }
				else if (c == '\t') { push_tab(characters, row, col); col += 4; }
				else if (c == ' ') { push_space(characters, row, col); col++; }
				else { push_char(characters, row, col, c, index); col++; }
			} else {
				if (c == '\n') { absolute_row++; }
			}
			index++;
		}
	}

	void init(const std::string& text) {
		stack.set_cursor(0);
		stack.set_text(text);
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
				needs_save = false;
			}
		}
	}

	void clear_highlight() { highlight.clear(); }

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		process_key(key, col_count, row_count);
		if (repeat) { repeat = false; replay(col_count, row_count); }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		push_text(characters, col_count, row_count);
	}

	State& state() { return stack.state(); }
	const State& state() const { return stack.state(); }

	const std::string_view get_filename() const { return filename; }

	Mode get_mode() const { return mode; }

	std::string get_record() const {
		std::string s;
		for (auto& c : record) {
			s += (uint8_t)c;
		}
		return s;
	}

	size_t location_percentage() const {
		return 1 + state().get_cursor() * 100 / state().get_text().size();
	}

	bool is_normal() const { return mode == Mode::normal; }
	bool is_dirty() const { return needs_save; }

	std::string increase_hue_start() { hue_start = std::clamp(hue_start + 5.0f, 0.0f, 360.0f); return std::string("hue_start = ") + std::to_string(hue_start); }
	std::string decrease_hue_start() { hue_start = std::clamp(hue_start - 5.0f, 0.0f, 360.0f); return std::string("hue_start = ") + std::to_string(hue_start); }

	std::string increase_hue_range() { hue_range = std::clamp(hue_range + 5.0f, 0.0f, 360.0f); return std::string("hue_range = ") + std::to_string(hue_range); }
	std::string decrease_hue_range() { hue_range = std::clamp(hue_range - 5.0f, 0.0f, 360.0f); return std::string("hue_range = ") + std::to_string(hue_range); }

	std::string increase_hue_adjust() { hue_adjust = std::clamp(hue_adjust + 5.0f, 0.0f, 360.0f); return std::string("hue_adjust = ") + std::to_string(hue_adjust); }
	std::string decrease_hue_adjust() { hue_adjust = std::clamp(hue_adjust - 5.0f, 0.0f, 360.0f); return std::string("hue_adjust = ") + std::to_string(hue_adjust); }

	std::string increase_saturation() { saturation = std::clamp(saturation + 0.05f, 0.0f, 1.0f); return std::string("saturation = ") + std::to_string(saturation); }
	std::string decrease_saturation() { saturation = std::clamp(saturation - 0.05f, 0.0f, 1.0f); return std::string("saturation = ") + std::to_string(saturation); }

	std::string increase_brightness() { brightness = std::clamp(brightness + 0.05f, 0.0f, 1.0f); return std::string("brightness = ") + std::to_string(brightness); }
	std::string decrease_brightness() { brightness = std::clamp(brightness - 0.05f, 0.0f, 1.0f); return std::string("brightness = ") + std::to_string(brightness); }
};

class Picker {
	std::string pattern;
	std::vector<std::string> filtered;
	unsigned selected = 0; 

	std::string tolower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); } );
		return s;
	}

	void push_char(Characters& characters, unsigned row, unsigned col, char c) const {
		characters.emplace_back((uint16_t)c, colors().text, row, col);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s) const {
		for (auto& c : s) {
			push_char(characters, row, col++, c);
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
		pattern.clear();
		filtered.clear();
		selected = 0;
	}

	void filter(Index& index, unsigned row_count) {
		filtered.clear();
		index.process([&](const auto& path) {
			if (filtered.size() > row_count - 2)
				return false;
			if (tolower(path).find(pattern) != std::string::npos)
				filtered.push_back(path);
			return true;
		});
		selected = std::min(selected, (unsigned)filtered.size() - 1);
	}

	std::string selection() {
		if (selected < filtered.size())
			return filtered[selected];
		return {};
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == Glyph::CR) { return; }
		else if (key == Glyph::ESC) { return; }
		else if (key == Glyph::TAB) { return; }
		else if (key == Glyph::BS) { if (pattern.size() > 0) { pattern.pop_back(); } }
		else if (key == '<') { selected++; }
		else if (key == '>') { if (selected > 0) selected--; }
		else { pattern += (char)key; }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned col = 0;
		unsigned row = 0;
		push_string(characters, row, col, "open: ");
		push_string(characters, row, col, pattern);
		push_cursor(characters, row, col);
		row++;

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
	Buffer empty_buffer;
	std::map<std::string, Buffer> buffers;
	std::string active;

	unsigned longest_filename() const {
		unsigned longest = 0;
		for (auto& it : buffers) {
			longest = std::max(longest, (unsigned)it.second.get_filename().size());
		}
		return longest;
	}

	void push_char(Characters& characters, unsigned row, unsigned col, char c) const {
		characters.emplace_back((uint16_t)c, colors().text, row, col);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s) const {
		for (auto& c : s) {
			push_char(characters, row, col++, c);
		}
	}

	void push_background_line(Characters& characters, unsigned row, unsigned col_begin, unsigned col_end) const {
		for (unsigned i = col_begin; i < col_end; ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().overlay, row, i);
		}
	}

	void push_cursor_line(Characters& characters, unsigned row, unsigned col_begin, unsigned col_end) const {
		for (unsigned i = col_begin; i < col_end; ++i) {
			characters.emplace_back(Glyph::BLOCK, colors().cursor_line, row, i);
		}
	}

public:
	std::string load(const std::string_view filename) {
		if (!filename.empty() && buffers.find(std::string(filename)) == buffers.end()) {
			const Timer timer;
			buffers.emplace(filename, filename);
			active = filename;
			return std::string("load ") + std::string(filename) + " in " + timer.us();
		}
		return {};
	}

	std::string reload() {
		if (!active.empty()) {
			const Timer timer;
			buffers[active].reload();
			return std::string("reload ") + std::string(buffers[active].get_filename()) + " in " + timer.us();
		}
		return {};
	}

	std::string save() {
		if (!active.empty()) {
			const Timer timer;
			buffers[active].save();
			return std::string("save ") + std::string(buffers[active].get_filename()) + " in " + timer.us();
		}
		return {};
	}

	std::string close() {
		if (!active.empty()) {
			const Timer timer;
			const auto filename = std::string(buffers[active].get_filename());
			buffers.erase(filename);
			active.clear();
			select_previous();
			return std::string("close ") + filename + " in " + timer.us();
		}
		return {};
	}

	Buffer& current() {
		if (!active.empty())
			return buffers[active];
		return empty_buffer;
	}

	void select_previous() {
		if (!active.empty()) {
			if (auto found = buffers.find(active); found != buffers.end()) {
				if (auto prev = --found; prev != buffers.end()) {
					active = buffers.at((*prev).first).get_filename();
				}
				else {
					active = buffers.at((*buffers.rbegin()).first).get_filename();
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
				else {
					active = buffers.at((*buffers.begin()).first).get_filename();
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

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == 'j') { select_next(); }
		else if (key == 'k') { select_previous(); }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned width = std::min(longest_filename(), col_count);
		unsigned height = std::min((unsigned)buffers.size(), row_count);

		unsigned left_col = (col_count - width) / 2;
		unsigned right_col = left_col + width;
		unsigned top_row = (row_count - height) / 2;
		unsigned bottom_row = top_row + height;

		unsigned col = left_col;
		unsigned row = top_row;
		for (auto& it : buffers) {
			col = left_col;
			push_background_line(characters, row, left_col, right_col);
			if (active == it.second.get_filename())
				push_cursor_line(characters, row, left_col, right_col);
			push_string(characters, row, col, it.second.get_filename());
			if (it.second.is_dirty()) 
				push_string(characters, row, col, "*");
			row++;
		}
	}
};

class Finder {

public:
	void reset() {
	}

	void filter(Database& database, unsigned row_count) {
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) {
	}
};

