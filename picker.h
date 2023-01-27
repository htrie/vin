#pragma once

class Picker {
	std::string pattern;
	std::vector<std::string> filtered;
	unsigned selected = 0; 

	void push_line(Characters& characters, unsigned row, unsigned col_count, Color color) const {
		for (unsigned i = 0; i <= col_count; ++i) {
			characters.emplace_back(Glyph::BLOCK, color, row, i);
			characters.emplace_back(Glyph::BLOCK, color, float(row), float(i) + 0.5f); // Fill in gaps.
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col, Color color) const {
		characters.emplace_back(Glyph::LINE, colors().cursor, row, col);
	};

	bool match_pattern(const std::string_view path) {
		const auto s = to_lower(path);
		size_t pos = 0;
		for (auto& c : pattern) {
			if (pos = s.find(c, pos); pos == std::string::npos)
				return false;
		}
		return true;
	}

public:
	void reset() {
		pattern.clear();
		filtered.clear();
		selected = 0;
	}

	void filter(const Index& index, unsigned row_count) {
		filtered.clear();
		pattern = to_lower(pattern);
		index.process([&](const auto& path) {
			if (filtered.size() > row_count - 1)
				return false;
			if (match_pattern(path))
				filtered.push_back(path.c_str());
			return true;
		});
		selected = std::min(selected, (unsigned)filtered.size() - 1);
	}

	std::string selection() const {
		if (selected < filtered.size())
			return filtered[selected];
		return {};
	}

	void process(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == '\r') { return; }
		else if (key == Glyph::ESC) { return; }
		else if (key == '\t') { return; }
		else if (key == '\b') { if (pattern.size() > 0) { pattern.pop_back(); } }
		else if (key == '<') { selected++; }
		else if (key == '>') { if (selected > 0) selected--; }
		else { pattern += (char)key; }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned col = 0;
		unsigned row = 0;
		push_line(characters, row, col_count, colors().text);
		push_string(characters, row, col, "open: ", true, colors().clear);
		push_string(characters, row, col, pattern, false, colors().clear);
		push_cursor(characters, row, col, colors().clear);
		row++;

		unsigned displayed = 0;
		for (auto& path : filtered) {
			col = 0;
			if (selected == displayed)
				push_line(characters, row, col_count, colors().cursor_line);
			push_string(characters, row++, col, path, false, colors().text);
			displayed++;
		}
	}
};

