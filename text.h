#pragma once

enum Glyph {
	ESC = 27, // TODO: Remove.
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

void push_char(Characters& characters, unsigned row, unsigned col, char c, const Color& color, bool bold) {
	const uint16_t index = 
		c == ' ' ? (uint16_t)Glyph::SPACE :
		c == '\t' ? (uint16_t)Glyph::TAB :
		c == '\r' ? (uint16_t)Glyph::CARRIAGE :
		c == '\n' ? (uint16_t)Glyph::RETURN :
		(uint16_t)c;
	const auto final_color = c == ' ' || c == '\t' || c == '\r' || c == '\n' ? colors().whitespace : color;
	characters.emplace_back(index, final_color, row, col, bold);
};

void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s, bool bold, Color color) {
	for (auto& c : s) {
		push_char(characters, row, col++, c, color, bold);
	}
}

