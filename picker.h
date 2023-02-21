#pragma once

class Picker {
	std::string pattern;
	std::vector<std::string> filtered;
	unsigned selected = 0; 

	bool match_pattern(const std::string_view path, const std::string_view lowercase_pattern) {
		const auto s = to_lower(path);
		size_t pos = 0;
		for (auto& c : lowercase_pattern) {
			if (pos = s.find(c, pos); pos == std::string::npos)
				return false;
		}
		return true;
	}

public:
	void filter(const Index& index) {
		filtered.clear();
		const auto lowercase_pattern = to_lower(pattern);
		index.process([&](const auto& path) {
			if (match_pattern(path, lowercase_pattern))
				filtered.push_back(path.c_str());
			return true;
		});
		selected = std::min(selected, (unsigned)filtered.size() - 1);
	}

	std::string selection() const {
		if (filtered.size() == 0)
			return pattern;
		if (selected < filtered.size())
			return filtered[selected];
		return {};
	}

	void process(unsigned key) {
		if (key == '\r') { return; }
		else if (key == Glyph::ESCAPE) { return; }
		else if (key == '\b') { if (pattern.size() > 0) { pattern.pop_back(); } }
		else if (key == '\t') { if (is_shift_down()) { if (selected > 0) { selected--; } } else { selected++; } }
		else { pattern += (char)key; }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count, unsigned row_start) const {
		unsigned col = 0;
		unsigned row = row_start;
		push_line(characters, colors().bar, float(row), 0, col_count);
		push_string(characters, colors().bar_text, row, col, "open:");
		push_string(characters, colors().bar_pattern, row, col, pattern);
		push_cursor(characters, colors().bar_text, row, col);
		push_string(characters, colors().bar_text, row, col, "   (" + std::to_string(filtered.size()) + " found)");
		row++;

		const unsigned begin = selected > row_count / 2 ? selected - row_count / 2 : 0;
		const unsigned end = std::clamp(std::max(selected + row_count / 2, row_count), 0u, (unsigned)filtered.size());
		for (unsigned i = begin; i < end; ++i) {
			const auto& path = filtered[i];
			if (i == selected)
				push_line(characters, colors().cursor_line, float(row), 0, col_count);
			col = 0;
			push_string(characters, colors().text, row++, col, path);
		}
	}
};

