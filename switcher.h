#pragma once

bool ignore_file(const std::string_view path) {
	if (path.ends_with(".db")) return true;
	if (path.ends_with(".exe")) return true;
	if (path.ends_with(".ilk")) return true;
	if (path.ends_with(".iobj")) return true;
	if (path.ends_with(".ipdb")) return true;
	if (path.ends_with(".obj")) return true;
	if (path.ends_with(".pch")) return true;
	if (path.ends_with(".pdb")) return true;
	return false;
}

class Index {
	std::vector<std::string> paths;

public:
	void populate() {
		paths.clear();
		process_files(".", [&](const auto& path) {
			if (!ignore_file(path))
				paths.push_back(path);
		});
	}

	std::string generate() {
		std::string list;
		for (const auto& path : paths) {
			list += path + "\n";
		}
		return list;
	}
};

class Database {
	struct File {
		std::string name;
		std::vector<char> contents;
	};

	struct Location {
		size_t file_index = 0;
		size_t position = 0;
	};

	std::vector<File> files;
	std::vector<Location> locations;

	void add(const std::string& filename) {
		map(filename, [&](const char* mem, size_t size) {
			if (!ignore_file(filename)) {
				std::vector<char> contents(size);
				memcpy(contents.data(), mem, contents.size());
				files.emplace_back(filename, std::move(contents));
			}
		});
	}

	void scan(unsigned file_index, const std::vector<char>& contents, const std::string_view pattern) {
		for (size_t i = 0; i < contents.size(); ++i) {
			if (contents[i] == pattern[0]) {
				if (strncmp(&contents[i], pattern.data(), pattern.size()) == 0) {
					locations.emplace_back(file_index, i);
				}
			}
		}
	}

	static std::string_view cut_line(const std::string_view text, size_t pos) {
		Line line(text, pos);
		return line.to_string(text);
	}

	std::string make_line(const std::string_view text) {
		if (text.size() > 0)
			if (text[text.size() - 1] == '\n')
				return std::string(text);
		return std::string(text) + "\n";
	}

public:
	void populate() {
		files.clear();
		process_files(".", [&](const auto& path) {
			add(path);
		});
	}

	std::string generate(const std::string_view pattern) {
		if (!pattern.empty() && pattern.size() > 2) {
			for (unsigned i = 0; i < files.size(); i++) {
				scan(i, files[i].contents, pattern);
			}
		}
		std::string list;
		for (const auto& location : locations) {
			const auto& file = files[location.file_index];
			const auto pos = location.position;
			const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
			const auto count = std::min((size_t)200, file.contents.size() - start);
			const auto context = std::string(&file.contents[start], count);
			list += file.name + "(" + std::to_string(pos) + ") " + make_line(cut_line(context, pos - start));
		}
		return list;
	}
};

class Switcher {
	std::vector<Buffer> buffers;
	size_t active = 0;
	
	std::string url;
	std::string clipboard;

	Buffer& current() { return buffers[active]; }
	const Buffer& current() const { return buffers[active]; }

	void select_index(unsigned index) { active = index >= 0 && index < buffers.size() ? index : active; }
	void select_previous() { active = active > 0 ? active - 1 : buffers.size() - 1; }
	void select_next() { active = (active + 1) % buffers.size(); }

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

	void process_space_e() {
		Index index;
		index.populate();
		load("open");
		current().init(index.generate());
	}

	void process_space_f() {
		Database database;
		database.populate();
		const auto seed = current().get_word();
		load(std::string("find ") + seed);
		current().init(database.generate(seed));
		current().set_highlight(seed);
	}

	void process_space(bool& quit, bool& toggle, unsigned key, unsigned row_count) {
		if (key == 'q') { quit = true; }
		else if (key == 'm') { toggle = true; }
		else if (key == 'w') { close(); }
		else if (key == 'r') { reload(); }
		else if (key == 's') { save(); }
		else if (key == 'e') { process_space_e(); }
		else if (key == 'f') { process_space_f(); }
		else if (key == 'i') { current().state().window_down(row_count - 1); }
		else if (key == 'o') { current().state().window_up(row_count - 1); }
		else if (key >= '0' && key <= '9') { select_index(key - '0'); }
		else if (key == 'h') { select_previous(); }
		else if (key == 'l') { select_next(); }
		else if (key == 'n') { current().clear_highlight(); }
	}

	void process_normal(unsigned key, unsigned col_count, unsigned row_count) {
		current().process(clipboard, url, key, col_count, row_count- 1);
		if (!url.empty()) {
			load(extract_filename(url));
			current().jump(extract_location(url), row_count);
			url.clear();
		}
	}

	void push_tabs(Characters& characters, unsigned col_count, unsigned row_count) const {
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
	
	void process(bool space_down, bool& quit, bool& toggle, unsigned key, unsigned col_count, unsigned row_count) {
		if (space_down && current().is_normal()) { process_space(quit, toggle, key, row_count); }
		else { process_normal(key, col_count, row_count); }
	}

	void cull(Characters& characters, unsigned col_count, unsigned row_count) const {
		push_tabs(characters, col_count, row_count);
		current().cull(characters, col_count, row_count);
	}
};

