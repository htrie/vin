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

	void push_tabs(Characters& characters, unsigned col_count, unsigned row_count) const {
		std::vector<std::string> tabs;
		for (size_t i = 0; i < buffers.size(); ++i) {
			tabs.push_back(std::to_string(i) + ")" + std::string(buffers[i].get_filename()) + (buffers[i].is_dirty() ? "*" : ""));
		}
		const unsigned row = 0;
		unsigned col = 0;
		for (size_t i = 0; i < buffers.size(); ++i) {
			push_line(characters, i == active ? colors().bar_text : colors().bar, (float)row, col, col + (int)tabs[i].size());
			push_string(characters, i == active ? colors().bar : colors().bar_text, row, col, tabs[i]);
			col++;
		}
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
			else {
				const Timer timer;
				buffers.emplace_back(filename);
				active = buffers.size() - 1;
				return std::string("load ") + std::string(buffers[active].get_filename()) + " (" + std::to_string(buffers[active].get_size()) + " bytes) in " + timer.us();
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
			active = (active >= buffers.size() ? active - 1 : active) % buffers.size();
			return std::string("close ") + filename + " in " + timer.us();
		}
		return std::string("can't close ") + filename + " in " + timer.us();
	}

	Buffer& current() { return buffers[active]; }
	const Buffer& current() const { return buffers[active]; }

	void select_index(unsigned index) { active = index >= 0 && index < buffers.size() ? index : active; }
	void select_previous() { active = active > 0 ? active - 1 : buffers.size() - 1; }
	void select_next() { active = (active + 1) % buffers.size(); }

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		push_tabs(characters, col_count, row_count);
		current().cull(characters, col_count, row_count);
	}
};

