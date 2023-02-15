#pragma once

bool accept(const std::string_view filename) {
	if (filename.ends_with(".c")) return true;
	if (filename.ends_with(".h")) return true;
	if (filename.ends_with(".cpp")) return true;
	if (filename.ends_with(".hpp")) return true;
	if (filename.ends_with(".inc")) return true;
	if (filename.ends_with(".txt")) return true;
	if (filename.ends_with(".diff")) return true;
	if (filename.ends_with(".bat")) return true;
	if (filename.ends_with(".frag")) return true;
	if (filename.ends_with(".vert")) return true;
	return false;
}

class Index {
	std::vector<std::string> paths;

public:
	std::string populate() {
		const Timer timer;
		paths.clear();
		process_files(".", [&](const auto& path) {
			const std::string filename(path);
			if (accept(filename)) {
				paths.push_back(filename);
			}
		});
		return std::string("populate (") + 
			std::to_string(paths.size()) + " paths) in " + timer.us();
	}

	template <typename F>
	void process(F func) const {
		for (const auto& path : paths) {
			if (!func(path))
				break;
		}
	}
};

class Database {
	struct Location {
		size_t file_index = 0;
		size_t position = 0;
	};

	std::vector<File> files;
	std::vector<Location> locations;

	void scan(unsigned file_index, const char* mem, size_t size, const std::string_view pattern) {
		for (size_t i = 0; i < size; ++i) {
			if (mem[i] == pattern[0]) {
				if (strncmp(&mem[i], pattern.data(), pattern.size()) == 0) {
					locations.emplace_back(file_index, i);
				}
			}
		}
	}

public:
	std::string populate() {
		const Timer timer;
		files.clear();
		process_files(".", [&](const auto& path) {
			if (accept(path)) {
				files.emplace_back(path);
			}
		});
		return std::string("populate (") + 
			std::to_string(files.size()) + " files) in " + timer.us();
	}

	std::string search(const std::string_view pattern) {
		const Timer timer;
		locations.clear();
		if (!pattern.empty() && pattern.size() > 2) {
			for (unsigned i = 0; i < files.size(); i++) {
				scan(i, files[i].get_mem(), files[i].get_size(), pattern);
			}
		}
		return std::string("search (") + 
			std::to_string(locations.size()) + " symbols) in " + timer.us();
	}

	template <typename F>
	void process(F func) const {
		for (const auto& location : locations) {
			if (!func(files[location.file_index], location))
				break;
		}
	}
};

