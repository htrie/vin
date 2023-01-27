#pragma once

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

