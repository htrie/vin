#pragma once

bool ignore_file(const std::string_view path) {
	if (path.ends_with(".db")) return true;
	if (path.ends_with(".aps")) return true;
	if (path.ends_with(".bin")) return true;
	if (path.ends_with(".dat")) return true;
	if (path.ends_with(".exe")) return true;
	if (path.ends_with(".idb")) return true;
	if (path.ends_with(".ilk")) return true;
	if (path.ends_with(".iobj")) return true;
	if (path.ends_with(".ipdb")) return true;
	if (path.ends_with(".jpg")) return true;
	if (path.ends_with(".lib")) return true;
	if (path.ends_with(".obj")) return true;
	if (path.ends_with(".pch")) return true;
	if (path.ends_with(".pdb")) return true;
	if (path.ends_with(".png")) return true;
	if (path.ends_with(".tlog")) return true;
	return false;
}

std::string list() {
	std::string list;
	process_files(".", [&](const auto& path) {
		if (!ignore_file(path))
			list += path + "\n";
	});
	return list;
}

std::string_view cut_line(const std::string_view text, size_t pos) {
	Line line(text, pos);
	return line.to_string(text);
}

std::string make_line(const std::string_view text) {
	if (text.size() > 0)
		if (text[text.size() - 1] == '\n')
			return std::string(text);
	return std::string(text) + "\n";
}

std::string make_entry(size_t pos, const char* mem, size_t size) {
	const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
	const auto count = std::min((size_t)200, size - start);
	const auto context = std::string(&mem[start], count);
	return "(" + std::to_string(pos) + ") " + make_line(cut_line(context, pos - start));
}

std::string scan(const std::string_view path, const std::string_view pattern) {
	std::string list;
	map(path, [&](const char* mem, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			if (mem[i] == pattern[0]) {
				if (strncmp(&mem[i], pattern.data(), std::min(pattern.size(), size - i)) == 0) {
					list += std::string(path) + make_entry(i, mem, size);
				}
			}
		}
	});
	return list;
}

std::string find(const std::string_view pattern) {
	std::string list;
	if (!pattern.empty() && pattern.size() > 2) {
		process_files(".", [&](const auto& path) {
			if (!ignore_file(path)) {
				list += scan(path, pattern);
			}
		});
	}
	return list;
}

class Switcher {
	std::vector<Buffer> buffers;
	size_t active = 0;
	
	std::string url;
	std::string clipboard;

	unsigned col_count = 0;
	unsigned row_count = 0;

	Buffer& current() { return buffers[active]; }
	const Buffer& current() const { return buffers[active]; }

	void select_index(unsigned index) { active = index >= 0 && index < buffers.size() ? index : active; }
	void select_previous() { active = active > 0 ? active - 1 : buffers.size() - 1; }
	void select_next() { active = (active + 1) % buffers.size(); }

	size_t find_buffer(const std::string_view filename) {
		for (size_t i = 0; i < buffers.size(); ++i) {
			if (buffers[i].get_filename() == filename)
				return i;
		}
		return (size_t)-1;
	}

	void load(const std::string_view filename) {
		if (!filename.empty()) {
			if (auto index = find_buffer(filename); index != (size_t)-1) {
				active = index;
			}
			else {
				buffers.emplace_back(filename);
				active = buffers.size() - 1;
			}
		}
	}

	void reload() {
		buffers[active].reload();
	}

	void save() {
		buffers[active].save();
	}

	void close() {
		const auto filename = std::string(buffers[active].get_filename());
		if (active != 0) { // Don't close scratch.
			buffers.erase(buffers.begin() + active);
			active = (active >= buffers.size() ? active - 1 : active) % buffers.size();
		}
	}

	void process_space_e() {
		load("open");
		current().init(list());
	}

	void process_space_f() {
		const auto seed = current().get_word();
		load(std::string("find ") + seed);
		current().init(find(seed));
		current().set_highlight(seed);
	}

	void process_space(bool& quit, bool& toggle, unsigned key) {
		if (key == 'q') { quit = true; }
		else if (key == 'm') { toggle = true; }
		else if (key == 'w') { close(); }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); }
		else if (key == 'i') { current().window_down(row_count); }
		else if (key == 'o') { current().window_up(row_count); }
		else if (key >= '0' && key <= '9') { select_index(key - '0'); }
		else if (key == 'h') { select_previous(); }
		else if (key == 'l') { select_next(); }
		else if (key == 'n') { current().clear_highlight(); }
	}

	void process_normal(unsigned key) {
		current().process(clipboard, url, key, col_count, row_count);
		if (!url.empty()) {
			load(extract_filename(url));
			current().jump(extract_location(url), row_count);
			url.clear();
		}
	}

	void push_tabs(Characters& characters) const {
		std::vector<std::string> tabs;
		for (size_t i = 0; i < buffers.size(); ++i) {
			tabs.push_back(std::to_string(i) + ":" + std::string(buffers[i].get_filename()) + (buffers[i].is_dirty() ? "*" : ""));
		}
		const unsigned row = 0;
		unsigned col = 0;
		for (size_t i = 0; i < buffers.size(); ++i) {
			push_line(characters, i == active ? colors().bar_text : colors().bar, (float)row, col, col + (int)tabs[i].size());
			push_string(characters, i == active ? colors().bar : colors().bar_text, row, col, tabs[i]);
			col += 1;
		}
	}

public:
	Switcher() {
		buffers.emplace_back("scratch");
	}

	void resize(unsigned w, unsigned h) {
		col_count = w;
		row_count = h - 1; // Remove 1 for tabs bar.
	}
	
	void process(bool space_down, bool& quit, bool& toggle, unsigned key) {
		if (space_down && current().is_normal()) { process_space(quit, toggle, key); }
		else { process_normal(key); }
	}

	Characters cull() const {
		Characters characters;
		push_tabs(characters);
		current().cull(characters, col_count, row_count);
		return characters;
	}
};

