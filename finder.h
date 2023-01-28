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

	std::string_view cut_line(const std::string_view text, size_t pos) {
		Line line(text, pos);
		return line.to_string(text);
	}

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
				const auto pos = location.position;
				const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
				const auto count = std::min((size_t)200, size - start);
				const auto context = std::string(&mem[start], count);
				filtered.emplace_back(file.name.c_str(), pos, std::string(cut_line(context, pos - start)));
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
		push_line(characters, colors().text, float(row), 0, col_count);
		push_string(characters, colors().clear, row, col, "find:", true);
		push_string(characters, colors().clear, row, col, pattern);
		push_cursor(characters, colors().clear, row, col);
		row++;

		unsigned displayed = 0;
		for (auto& entry : filtered) {
			col = 0;
			if (selected == displayed)
				push_line(characters, colors().cursor_line, float(row), 0, col_count);
			const auto location = entry.filename + "(" + std::to_string(entry.position) + "):";
			push_string(characters, colors().text, row, col, location);
			const auto pos = entry.context.find(pattern);
			const auto pre_context = entry.context.substr(0, pos);
			const auto post_context = entry.context.substr(pos + pattern.size());
			push_string(characters, colors().comment, row, col, pre_context);
			push_string(characters, colors().text, row, col, pattern, true);
			push_string(characters, colors().comment, row, col, post_context);
			row++;
			displayed++;
		}
	}
};

