#pragma once

class Finder {
	struct Entry {
		std::string filename;
		size_t position;
		std::string context;
	};

	std::string pattern;
	std::vector<Entry> filtered;
	unsigned selected = 0; 

public:
	void reset() {
		pattern.clear();
		filtered.clear();
		selected = 0;
	}

	void seed(const std::string_view word) {
		pattern = word;
	}

	const std::string_view get_pattern() const { return pattern; }

	void filter(const Database& database, unsigned row_count) {
		filtered.clear();
		database.process([&](const auto& file, const auto& location) {
			if (filtered.size() > row_count - 1)
				return false;
			map(file.name, [&](const char* mem, size_t size) {
				const auto context = std::string(&mem[location.position], std::min((size_t)60, size - location.position));
				filtered.emplace_back(file.name.c_str(), location.position, context);
			});
			return true;
		});
		selected = std::min(selected, (unsigned)filtered.size() - 1);
	}

	std::string selection() const {
		if (selected < filtered.size())
			return filtered[selected].filename;
		return {};
	}

	size_t position() const {
		if (selected < filtered.size())
			return filtered[selected].position;
		return 0;
	}

	bool process(unsigned key, unsigned col_count, unsigned row_count) {
		if (key == '\r') { return false; }
		else if (key == Glyph::ESCAPE) { return false; }
		else if (key == '\t') { return false; }
		else if (key == '\b') { if (pattern.size() > 0) { pattern.pop_back(); return true; } }
		else if (key == '<') { selected++; }
		else if (key == '>') { if (selected > 0) selected--; }
		else { pattern += (char)key; return true; }
		return false;
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned col = 0;
		unsigned row = 0;
		push_line(characters, float(row), 0, col_count, colors().text);
		push_string(characters, row, col, "find:", true, colors().clear);
		push_string(characters, row, col, pattern, false, colors().clear);
		push_cursor(characters, row, col, colors().clear);
		row++;

		unsigned displayed = 0;
		for (auto& entry : filtered) {
			col = 0;
			if (selected == displayed)
				push_line(characters, float(row), 0, col_count, colors().cursor_line);
			push_string(characters, row, col, entry.filename + " (" + std::to_string(entry.position) + "): ", true, colors().text);
			push_string(characters, row, col, entry.context, false, colors().comment);
			row++;
			displayed++;
		}
	}
};

