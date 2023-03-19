#pragma once

enum Glyph {
	ESCAPE = 27,
	SPACE = 9251,
	TAB = 10230,
	CARRIAGE = 10232,
	RETURN = 9166,
	BOTTOM = 9601,
	BLOCK = 9608,
	LINE = 9615,
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

static inline const std::vector<std::vector<std::string_view>> cpp_keywords = {
	{ "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto" },
	{ "bitand", "bitor", "bool", "break" },
	{ "case", "catch", "char", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue" },
	{ "decltype", "default", "define", "delete", "do", "double", "dynamic_cast" },
	{ "else", "elsif", "endif", "enum", "explicit", "export", "extern" },
	{ "false", "float", "for", "friend" },
	{ "goto" },
	{ },
	{ "include", "if", "ifdef", "ifndef", "inline", "int","int8_t",  "int16_t", "int32_t",  "int64_t" },
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
	float scale_x = 1.0f;
	float scale_y = 1.0f;
	float offset_x = 0.0f;
	float offset_y = 0.0f;

	Character() {}
	Character(uint16_t index, Color color, float row, float col, float scale_x = 1.0f, float scale_y = 1.0f, float offset_x = 0.0f, float offset_y = 0.0f)
		: index(index), color(color), row(row), col(col), scale_x(scale_x), scale_y(scale_y), offset_x(offset_x), offset_y(offset_y) {}
	Character(uint16_t index, Color color, unsigned row, unsigned col, float scale_x = 1.0f, float scale_y = 1.0f, float offset_x = 0.0f, float offset_y = 0.0f)
		: index(index), color(color), row((float)row), col((float)col), scale_x(scale_x), scale_y(scale_y), offset_x(offset_x), offset_y(offset_y) {}
};

typedef std::vector<Character> Characters;

void push_digit(Characters& characters, unsigned row, unsigned col, unsigned digit) {
	characters.emplace_back((uint16_t)(48 + digit), colors().line_number, row, col);
}

void push_number(Characters& characters, unsigned row, unsigned col, unsigned line) {
	if (line > 999) { push_digit(characters, row, col + 0, (line % 10000) / 1000); }
	if (line > 99) { push_digit(characters, row, col + 1, (line % 1000) / 100); }
	if (line > 9) { push_digit(characters, row, col + 2, (line % 100) / 10); }
	push_digit(characters, row, col + 3, line % 10);
}

void push_char(Characters& characters, Color color, unsigned row, unsigned& col, char c) {
	switch (c) {
		case ' ': characters.emplace_back(Glyph::SPACE, colors().whitespace, row, col++); break;
		case '\t': characters.emplace_back(Glyph::TAB, colors().whitespace, row, col++); break;
		case '\r': characters.emplace_back(Glyph::CARRIAGE, colors().whitespace, row, col); break;
		case '\n': characters.emplace_back(Glyph::RETURN, colors().whitespace, row, col++); break;
		default: characters.emplace_back(c, color, row, col++); break;
	}
}

void push_string(Characters& characters, Color color, unsigned row, unsigned& col, const std::string_view s) {
	for (auto& c : s) {
		push_char(characters, color, row, col, c);
	}
}

void push_line(Characters& characters, Color color, float row, unsigned col_begin, unsigned col_end) {
	for (unsigned i = col_begin; i <= col_end; ++i) {
		characters.emplace_back(Glyph::BLOCK, color, row, float(i), 1.2f, 1.0f, -1.0f, 0.0f);
	}
}

void push_cursor(Characters& characters, Color color, unsigned row, unsigned col) {
	characters.emplace_back(Glyph::LINE, color, row, col);
};

void push_carriage(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Glyph::CARRIAGE, colors().whitespace, row, col);
};

void push_return(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Glyph::RETURN, colors().whitespace, row, col);
};

void push_tab(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Glyph::TAB, colors().whitespace, row, col);
};

void push_space(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Glyph::SPACE, colors().whitespace, row, col);
};

class Word {
	size_t start = 0;
	size_t finish = 0;
	size_t finish_no_whitespace = 0;
	bool is_class = false;
	bool is_function = false;

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
				finish_no_whitespace = finish;
				while(test_whitespace(text, finish + 1)) { finish++; } // Include next whitespace.
				while(test_letter_or_number(text, start - 1)) { start--; }
			}
			else if (is_whitespace(c)) {
				while(test_whitespace(text, finish + 1)) { finish++; }
				finish_no_whitespace = finish;
				while(test_whitespace(text, start - 1)) { start--; }
			}
			else if (is_punctuation(c)) {
				while(test_punctuation(text, finish + 1)) { finish++; }
				finish_no_whitespace = finish;
				while(test_whitespace(text, finish + 1)) { finish++; } // Include next whitespace.
				while(test_punctuation(text, start - 1)) { start--; }
			}
			verify(start <= finish);
			is_class = is_uppercase_letter(text[start]) || text[start] == '_';
			is_function = finish < text.size() - 1 ? text[finish + 1] == '(' : false;
		}
	}

	size_t begin() const { return start; }
	size_t end() const { return finish; }
	
	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish_no_whitespace - start + 1);
	}

	bool check_class() const { return is_class; }
	bool check_function() const { return is_function; }

	bool check_keyword(const std::string_view text) const {
		if (const auto letter_index = compute_letter_index(text[start]); letter_index != (unsigned)-1) {
			const auto& keywords = cpp_keywords[letter_index];
			const std::string word(text.substr(start, finish_no_whitespace - start + 1));
			for (const auto& keyword : keywords) {
				if (word == keyword)
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

class Quote{
	size_t start = 0;
	size_t finish = 0;

	size_t find_prev(const std::string_view text, size_t pos, uint16_t quote) {
		unsigned count = 0;
		size_t last = pos;
		size_t index = pos;
		while (index < text.size() && text[index] != '\n') {
			if (text[index] == quote) {
				last = index;
				count++;
			}
			index--;
		}
		return count % 2 != 0 ? last : std::string::npos;
	}

	size_t find_next(const std::string_view text, size_t pos, uint16_t quote) {
		unsigned count = 0;
		size_t last = pos;
		size_t index = pos;
		while (index < text.size() && text[index] != '\n') {
			if (text[index] == quote) {
				last = index;
				count++;
			}
			index++;
		}
		return count % 2 != 0 ? last : std::string::npos;
	}

public:
	Quote(const std::string_view text, size_t pos, uint16_t quote) {
		if (text.size() > 0) {
			verify(pos < text.size());
			start = pos;
			finish = pos;
			const auto prev = find_prev(text, pos, quote);
			const auto next = find_next(text, pos, quote);
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

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish - start + 1);
	}

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

class Url {
	size_t start = 0;
	size_t finish = 0;

	bool test(const std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_number(text[pos]) || is_letter(text[pos]) || is_punctuation(text[pos]);
		}
		return false;
	}

public:
	Url(const std::string_view text, size_t pos) {
		if (text.size() > 0) {
			verify(pos < text.size());
			start = pos;
			finish = pos;
			const auto c = text[pos];
			while(test(text, finish + 1)) { finish++; }
			while(test(text, start - 1)) { start--; }
			verify(start <= finish);
		}
	}

	size_t begin() const { return start; }
	size_t end() const { return finish; }

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish - start + 1);
	}
};

