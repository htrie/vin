#pragma once

class Switcher {
	std::vector<Buffer> buffers;
	size_t active = 0;

	unsigned longest_filename() const {
		unsigned longest = 0;
		for (auto& buffer : buffers) {
			longest = std::max(longest, (unsigned)buffer.get_filename().size());
		}
		return longest;
	}

	size_t find_buffer(const std::string_view filename) {
		for (size_t i = 0; i < buffers.size(); ++i) {
			if (buffers[i].get_filename() == filename)
				return i;
		}
		return (size_t)-1;
	}

public:
	Switcher() {
		buffers.emplace_back("scratch");
	}

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
		const Timer timer;
		buffers[active].reload();
		return std::string("reload ") + std::string(buffers[active].get_filename()) + " in " + timer.us();
	}

	std::string save() {
		const Timer timer;
		buffers[active].save();
		return std::string("save ") + std::string(buffers[active].get_filename()) + " (" + std::to_string(buffers[active].get_size()) + " bytes) in " + timer.us();
	}

	std::string close() {
		const Timer timer;
		const auto filename = std::string(buffers[active].get_filename());
		if (active != 0) { // Don't close scratch.
			buffers.erase(buffers.begin() + active);
			select_previous();
			return std::string("close ") + filename + " in " + timer.us();
		}
		return std::string("can't close ") + filename + " in " + timer.us();
	}

	Buffer& current() { return buffers[active]; }

	void select_previous() { active = active > 0 ? active - 1 : buffers.size() - 1; }
	void select_next() { active = (active + 1) % buffers.size(); }

	void process(unsigned key, unsigned col_count, unsigned row_count) {
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
			push_line(characters, i == active ? colors().text : colors().overlay, float(row), left_col, right_col + 1);
			const auto name = std::string(buffers[i].get_filename()) + (buffers[i].is_dirty() ? "*" : "");
			push_string(characters, i == active ? colors().overlay : colors().text, row, col, name, buffers[i].is_dirty());
			row++;
		}
	}
};

