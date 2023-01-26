#pragma once

class Picker {
	std::string pattern;
	std::vector<std::string> filtered;
	unsigned selected = 0; 

	void push_char(Characters& characters, unsigned row, unsigned col, char c, bool bold, Color color) const {
		characters.emplace_back((uint16_t)c, color, row, col, bold);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s, bool bold, Color color) const {
		for (auto& c : s) {
			push_char(characters, row, col++, c, bold, color);
		}
	}

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

class Switcher {
	Buffer empty_buffer;
	std::vector<Buffer> buffers;
	size_t active = (size_t)-1;

	unsigned longest_filename() const {
		unsigned longest = 0;
		for (auto& buffer : buffers) {
			longest = std::max(longest, (unsigned)buffer.get_filename().size());
		}
		return longest;
	}

	void push_char(Characters& characters, unsigned row, unsigned col, char c, bool bold, Color color) const {
		characters.emplace_back((uint16_t)c, color, row, col, bold);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s, bool bold, Color color) const {
		for (auto& c : s) {
			push_char(characters, row, col++, c, bold, color);
		}
	}

	void push_line(Characters& characters, unsigned row, unsigned col_begin, unsigned col_end, Color color) const {
		for (unsigned i = col_begin; i < col_end; ++i) {
			characters.emplace_back(Glyph::BLOCK, color, row, i);
			characters.emplace_back(Glyph::BLOCK, color, float(row), float(i) + 0.5f); // Fill in gaps.
		}
	}

	size_t find_buffer(const std::string_view filename) {
		for (size_t i = 0; i < buffers.size(); ++i) {
			if (buffers[i].get_filename() == filename)
				return i;
		}
		return (size_t)-1;
	}

public:
	std::string load(const std::string_view filename) {
		if (!filename.empty()) {
			if (auto index = find_buffer(filename); index != (size_t)-1) {
				active = index;
				return std::string("switch ") + std::string(filename);
			}
			else if (buffers.size() < 16) {
				const Timer timer;
				buffers.emplace_back(filename);
				active = buffers.size() - 1;
				return std::string("load ") + std::string(buffers[active].get_filename()) + " (" + std::to_string(buffers[active].get_size()) + " bytes) in " + timer.us();
			}
			else {
				return std::string("too many files open");
			}
		}
		return {};
	}

	std::string reload() {
		if (active != (size_t)-1) {
			const Timer timer;
			buffers[active].reload();
			return std::string("reload ") + std::string(buffers[active].get_filename()) + " in " + timer.us();
		}
		return {};
	}

	std::string save() {
		if (active != (size_t)-1) {
			const Timer timer;
			buffers[active].save();
			return std::string("save ") + std::string(buffers[active].get_filename()) + " (" + std::to_string(buffers[active].get_size()) + " bytes) in " + timer.us();
		}
		return {};
	}

	std::string close() {
		if (active != (size_t)-1) {
			const Timer timer;
			const auto filename = std::string(buffers[active].get_filename());
			buffers.erase(buffers.begin() + active);
			select_previous();
			return std::string("close ") + filename + " in " + timer.us();
		}
		return {};
	}

	Buffer& current() {
		if (active != (size_t)-1)
			return buffers[active];
		return empty_buffer;
	}

	void select_previous() {
		if (active != (size_t)-1) {
			active = active > 0 ? active - 1 : buffers.size() - 1;
		} else {
			if (buffers.size() > 0)
				active = 0;
		}
	}

	void select_next() {
		if (active != (size_t)-1) {
			active = (active + 1) % buffers.size();
		} else {
			if (buffers.size() > 0)
				active = 0;
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
		for (size_t i = 0; i < buffers.size(); ++i) {
			col = left_col;
			push_line(characters, row, left_col, right_col + 1, i == active ? colors().text : colors().overlay);
			const auto name = std::string(buffers[i].get_filename()) + (buffers[i].is_dirty() ? "*" : "");
			push_string(characters, row, col, name, buffers[i].is_dirty(), i == active ? colors().overlay : colors().text);
			row++;
		}
	}
};

class Finder {
	struct Entry {
		std::string filename;
		size_t position;
		std::string context;
	};

	std::string pattern;
	std::vector<Entry> filtered;
	unsigned selected = 0; 

	void push_char(Characters& characters, unsigned row, unsigned col, char c, const Color& color, bool bold) const {
		const uint16_t index = 
			c == ' ' ? (uint16_t)Glyph::SPACESIGN :
			c == '\t' ? (uint16_t)Glyph::TABSIGN :
			c == CR ? (uint16_t)Glyph::CARRIAGE :
			c == '\n' ? (uint16_t)Glyph::RETURN :
			(uint16_t)c;
		const auto final_color = c == ' ' || c == '\t' || c == CR || c == '\n' ? colors().whitespace : color;
		characters.emplace_back(index, final_color, row, col, bold);
	};

	void push_string(Characters& characters, unsigned row, unsigned& col, const std::string_view s, bool bold, Color color) const {
		for (auto& c : s) {
			push_char(characters, row, col++, c, color, bold);
		}
	}

	void push_line(Characters& characters, unsigned row, unsigned col_count, Color color) const {
		for (unsigned i = 0; i <= col_count; ++i) {
			characters.emplace_back(Glyph::BLOCK, color, row, i);
			characters.emplace_back(Glyph::BLOCK, color, float(row), float(i) + 0.5f); // Fill in gaps.
		}
	}

	void push_cursor(Characters& characters, unsigned row, unsigned col, Color color) const {
		characters.emplace_back(Glyph::LINE, color, row, col);
	};

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
		if (key == Glyph::CR) { return false; }
		else if (key == Glyph::ESC) { return false; }
		else if (key == Glyph::TAB) { return false; }
		else if (key == Glyph::BS) { if (pattern.size() > 0) { pattern.pop_back(); return true; } }
		else if (key == '<') { selected++; }
		else if (key == '>') { if (selected > 0) selected--; }
		else { pattern += (char)key; return true; }
		return false;
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		unsigned col = 0;
		unsigned row = 0;
		push_line(characters, row, col_count, colors().text);
		push_string(characters, row, col, "find: ", true, colors().clear);
		push_string(characters, row, col, pattern, false, colors().clear);
		push_cursor(characters, row, col, colors().clear);
		row++;

		unsigned displayed = 0;
		for (auto& entry : filtered) {
			col = 0;
			if (selected == displayed)
				push_line(characters, row, col_count, colors().cursor_line);
			push_string(characters, row, col, entry.filename + " (" + std::to_string(entry.position) + "): ", true, colors().text);
			push_string(characters, row, col, entry.context, false, colors().comment);
			row++;
			displayed++;
		}
	}
};

