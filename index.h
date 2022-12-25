#pragma once

bool accept_extension(const std::string_view extension) {
	if (extension == ".cpp") return true;
	if (extension == ".hpp") return true;
	if (extension == ".c") return true;
	if (extension == ".h") return true;
	if (extension == ".txt") return true;
	if (extension == ".diff") return true;
	return false;
}

class Index {
	std::vector<std::string> paths;

public:
	void reset() {
		paths.clear();
	}

	std::string populate() {
		const Timer timer;
		paths.clear();
		process_files(".", [&](const char* path) {
			paths.push_back(path);
		});
		return std::string("populate index in ") + timer.us();
	}

	template <typename F>
	void process(F func) {
		for (const auto& path : paths) {
			if (!func(path))
				break;
		}
	}
};

class Database {
	struct File {
		std::string name;
		size_t size;
	};

	struct Location {
		size_t file_index = 0;
		uint64_t symbol_hash = 0;
		size_t position = 0;
	};

	std::vector<File> files;
	std::vector<Location> locations;

	constexpr bool accept_char(char c) {
		return
			(c >= 'a' && c <= 'z') || 
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			(c == '_'); }

	void scan(const std::string& filename) {
		map(filename, [&](const char* mem, size_t size) {
			files.emplace_back(filename, size);
			size_t location = 0;
			for (size_t i = 0; i < size; ++i) {
				if (!accept_char(mem[i])) {
					if ((location != i) && (i - location > 2)) // Ignore small words.
						locations.emplace_back(files.size() - 1, fnv64(&mem[location], i - location), location);
					location = i;
					location++;
				}
			}
		});
	}

public:
	void reset() {
		files.clear();
		locations.clear();
	}

	std::string populate() {
		const Timer timer;
		files.clear();
		files.resize(4 * 1024);
		locations.clear();
		locations.reserve(4 * 1024 * 1024);
		process_files(".", [&](const char* path) {
			scan(path);
		});
		return std::string("populate database (") + 
			std::to_string(files.size()) + " files, " + 
			std::to_string(locations.size()) + " symbols) in " + timer.us();
	}

	template <typename F>
	void process(F func) {
		for (const auto& location : locations) {
			if (!func(files[location.file_index], location))
				break;
		}
	}
};

