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
	void seed(const std::string_view word) {
		pattern = word;
	}

	const std::string_view get_pattern() const { return pattern; }

	void filter(const Database& database) {
		filtered.clear();
		database.process([&](const auto& file, const auto& location) {
			const auto pos = location.position;
			const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
			const auto count = std::min((size_t)200, file.contents.size() - start);
			const auto context = std::string(&file.contents[start], count);
			filtered.emplace_back(file.name.c_str(), pos, std::string(cut_line(context, pos - start)));
			return true;
		});
		selected = std::min(selected, (unsigned)filtered.size() - 1);
	}

	void select_next() {
		selected = selected < (unsigned)filtered.size() - 1 ? selected + 1 : 0;
	}

	void select_previous() {
		selected = selected > 0 ? selected - 1 : (unsigned)filtered.size() - 1;
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

	bool process(unsigned key) {
		if (key == '\r') { return false; }
		else if (key == Glyph::ESCAPE) { return false; }
		else if (key == '\t') { return false; }
		else if (key == '\b') { if (pattern.size() > 0) { pattern.pop_back(); return true; } }
		else if (key == 10/*j*/ && is_ctrl_down()) { selected++; }
		else if (key == 11/*k*/ && is_ctrl_down()) { if (selected > 0) selected--; }
		else { pattern += (char)key; return true; }
		return false;
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count, unsigned row_start) const {
		unsigned col = 0;
		unsigned row = row_start;
		push_line(characters, colors().text, float(row), 0, col_count);
		push_string(characters, colors().clear, row, col, "find:", true);
		push_string(characters, colors().clear, row, col, pattern);
		push_cursor(characters, colors().clear, row, col);
		row++;

		const unsigned begin = selected > row_count / 2 ? selected - row_count / 2 : 0;
		const unsigned end = std::clamp(std::max(selected + row_count / 2, row_count), 0u, (unsigned)filtered.size());
		for (unsigned i = begin; i < end; ++i) {
			const auto& entry = filtered[i];
			if (i == selected)
				push_line(characters, colors().cursor_line, float(row), 0, col_count);
			col = 0;
			const auto location = entry.filename + "(" + std::to_string(entry.position) + "):";
			push_string(characters, colors().text, row, col, location);
			const auto pos = entry.context.find(pattern);
			const auto pre_context = entry.context.substr(0, pos);
			const auto post_context = entry.context.substr(pos + pattern.size());
			push_string(characters, colors().comment, row, col, pre_context);
			push_string(characters, colors().text, row, col, pattern, true);
			push_string(characters, colors().comment, row, col, post_context);
			row++;
		}
	}
};

