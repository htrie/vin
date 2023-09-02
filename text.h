#pragma once

enum Codepoint {
	ESCAPE = 27,
	SPACE = 9251,
	TAB = 10230,
	CARRIAGE = 10232,
	RETURN = 9166,
	BOTTOM = 9601,
	BLOCK = 9608,
	LINE = 9615,
};

uint16_t superscript_codepoint(unsigned index) {
	switch (index) {
	default:
	case 0: return 8304;
	case 1: return 185;
	case 2: return 178;
	case 3: return 179;
	case 4: return 8308;
	case 5: return 8309;
	case 6: return 8310;
	case 7: return 8311;
	case 8: return 8312;
	case 9: return 8313;
	}
}

struct Color {
	uint8_t b = 0, g = 0, r = 0, a = 0;

	uint32_t& as_uint() { return (uint32_t&)*this; }
	const uint32_t& as_uint() const { return (uint32_t&)*this; }

	Color() noexcept {}
	Color(const Color& o) { *this = o; }

	explicit Color(uint32_t C) { as_uint() = C; }
	explicit Color(float r, float g, float b, float a) : r((uint8_t)(r * 255.0f)), g((uint8_t)(g * 255.0f)), b((uint8_t)(b * 255.0f)), a((uint8_t)(a * 255.0f)) {}
	explicit Color(const std::array<float, 4>& v) : Color(v[0], v[1], v[2], v[3]) {}

	static Color argb(unsigned a, unsigned r, unsigned g, unsigned b) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
	static Color rgba(unsigned r, unsigned g, unsigned b, unsigned a) { Color color; color.a = (uint8_t)a; color.r = (uint8_t)r;  color.g = (uint8_t)g;  color.b = (uint8_t)b; return color; }
	static Color gray(unsigned c) { Color color; color.a = (uint8_t)255; color.r = (uint8_t)c;  color.g = (uint8_t)c;  color.b = (uint8_t)c; return color; }

	Color& set_alpha(uint8_t alpha) { a = alpha; return *this; }

	static uint8_t blend(const uint8_t s, const uint8_t d, const uint8_t a) { return ((s * (255 - a)) + (d * a)) >> 8; }

	void blend(const Color& dst) {
		r = blend(r, dst.r, dst.a);
		g = blend(g, dst.g, dst.a);
		b = blend(b, dst.b, dst.a);
	}
};

struct Colors {
	Color clear = Color::gray(30);
	Color column_indicator = Color::gray(28);
	Color cursor_line = Color::rgba(80, 80, 0, 255);
	Color cursor = Color::rgba(255, 255, 0, 255);
	Color highlight = Color::rgba(180, 0, 180, 255);
	Color search = Color::rgba(0, 180, 180, 255);
	Color whitespace = Color::gray(30);
	Color status = Color::rgba(20, 10, 0, 255);
	Color status_text = Color::rgba(89, 86, 84, 255);
	Color bar = Color::rgba(59, 56, 54, 255);
	Color bar_text = Color::rgba(120, 110, 100, 255);
	Color text = Color::rgba(229, 218, 184, 255);
	Color text_cursor = Color::gray(30);
	Color line_number = Color::gray(80);
	Color context = Color::gray(140);
	Color note = Color::rgba(255, 200, 0, 255);
	Color add = Color::rgba(229, 218, 184, 255);
	Color remove = Color::rgba(120, 110, 100, 255);
};

Colors& colors() {
	static Colors colors;
	return colors;
}

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
constexpr bool is_url_punctuation(char c) { return 
	c == '-' || c == '+' || c == '*' || c == '/' || c == '=' || c == '\\' ||
	c == '.' || c == '<' || c == '>' || c == '(' || c == ')' || c == ':' ||
	c == '&' || c == '%' || c == '^' || c == '!' || c == '~' ||
	c == '?' || c == '#';
}

std::string to_lower(const std::string_view s) {
	auto res = std::string(s);
	std::transform(res.begin(), res.end(), res.begin(), [](const auto c){ return std::tolower(c); });
	return res;
}

std::optional<int> to_int(const std::string_view& input)
{
	int out = 0;
	const std::from_chars_result result = std::from_chars(input.data(), input.data() + input.size(), out);
	if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
        return std::nullopt;
    }
    return out;
}

static unsigned compute_letter_index(const uint16_t c) {
	if (c >= 'a' && c <= 'z') return c - 'a';
	return (unsigned)-1;
}

struct Character {
	uint16_t index = 0;
	Color color = Color::rgba(255, 0, 0, 255);
	unsigned row = 0;
	unsigned col = 0;

	Character() {}
	Character(uint16_t index, Color color, unsigned row, unsigned col)
		: index(index), color(color), row(row), col(col) {}
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
		case ' ': characters.emplace_back(Codepoint::SPACE, colors().whitespace, row, col++); break;
		case '\t': characters.emplace_back(Codepoint::TAB, colors().whitespace, row, col++); break;
		case '\r': characters.emplace_back(Codepoint::CARRIAGE, colors().whitespace, row, col); break;
		case '\n': characters.emplace_back(Codepoint::RETURN, colors().whitespace, row, col++); break;
		default: characters.emplace_back(c, color, row, col++); break;
	}
}

void push_string(Characters& characters, Color color, unsigned row, unsigned& col, const std::string_view s) {
	for (auto& c : s) {
		push_char(characters, color, row, col, c);
	}
}

void push_line(Characters& characters, Color color, unsigned row, unsigned col_begin, unsigned col_end) {
	for (unsigned i = col_begin; i < col_end; ++i) {
		characters.emplace_back(Codepoint::BLOCK, color, row, i);
	}
}

void push_cursor(Characters& characters, Color color, unsigned row, unsigned col) {
	characters.emplace_back(Codepoint::LINE, color, row, col);
};

void push_carriage(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Codepoint::CARRIAGE, colors().whitespace, row, col);
};

void push_return(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Codepoint::RETURN, colors().whitespace, row, col);
};

void push_tab(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Codepoint::TAB, colors().whitespace, row, col);
};

void push_space(Characters& characters, unsigned row, unsigned col) {
	characters.emplace_back(Codepoint::SPACE, colors().whitespace, row, col);
};

class Range {
protected:
	size_t start = 0;
	size_t finish = 0;

public:
	size_t begin() const { return start; }
	size_t end() const { return finish; }

	bool valid() const { return start < finish; }

	bool contains(size_t pos) const { return start <= pos && pos < finish; }
};

class Word : public Range {
	size_t finish_no_whitespace = 0;

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
		if (text.size() > 0 && pos < text.size()) {
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
		}
	}

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish_no_whitespace - start + 1);
	}
};

class Enclosure : public Range {
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
		if (text.size() > 0 && pos < text.size()) {
			start = pos;
			finish = pos;
			const auto prev = find_prev(text, pos, left, right);
			const auto next = find_next(text, pos, left, right);
			if (prev != std::string::npos && next != std::string::npos) {
				start = prev != std::string::npos ? prev : pos;
				finish = next != std::string::npos ? next : pos;
			}
		}
	}

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start + 1, finish - start);
	}
};

class Line : public Range {
public:
	Line(const std::string_view text, size_t pos) {
		if (text.size() > 0 && pos < text.size()) {
			const auto pn = text.rfind("\n", pos > 0 && text[pos] == '\n' ? pos - 1 : pos);
			const auto nn = text.find("\n", pos);
			start = pn != std::string::npos ? (pn < pos ? pn + 1 : pn) : 0;
			finish = nn != std::string::npos ? nn : text.size() - 1;
		}
	}

	size_t to_relative(size_t pos) const {
		if (pos >= start && pos <= finish)
			return pos - start;
		return pos;
	}

	size_t to_absolute(size_t pos) const {
		return std::min(start + pos, finish);
	}

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish - start + 1);
	}

	bool check_string(const std::string_view text, const std::string_view s) const {
		if (start + s.size() <= text.size())
			return strncmp(&text[start], s.data(), s.size()) == 0;
		return false;
	}
};

class Url : public Range {
	bool test(const std::string_view text, size_t pos) {
		if (pos < text.size()) {
			return is_number(text[pos]) || is_letter(text[pos]) || is_url_punctuation(text[pos]);
		}
		return false;
	}

public:
	Url(const std::string_view text, size_t pos) {
		if (text.size() > 0 && pos < text.size()) {
			start = pos;
			finish = pos;
			const auto c = text[pos];
			while(test(text, finish + 1)) { finish++; }
			while(test(text, start - 1)) { start--; }
		}
	}

	std::string_view to_string(const std::string_view text) const {
		return text.substr(start, finish - start + 1);
	}
};

std::string extract_filename(const std::string_view url) {
	Enclosure location(url, url.size() - 1, '(', ')');
	return std::string(location.valid() ? url.substr(0, location.begin()) : url);
}

unsigned extract_location(const std::string_view url) {
	Enclosure location(url, url.size() - 1, '(', ')');
	if (location.valid()) 
		if (const auto pos = to_int(location.to_string(url)))
			return pos.value();
	return 0;
}

