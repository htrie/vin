#pragma once

class Index {
	Array<PathString, 1024> paths;

public:
	void reset() {
		paths.clear();
	}

	SmallString populate() {
		const Timer timer;
		paths.clear();
		process_files(".", [&](const char* path) {
			paths.push_back(path);
		});
		return SmallString("populate index in ") + timer.us();
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
		PathString name;
		size_t size;
	};

	struct Location {
		size_t file_index = 0;
		uint64_t symbol_hash = 0;
		size_t position = 0;
	};

	Array<File, 1024> files;
	Array<Location, 128 * 1024> locations;

	constexpr bool accept_char(char c) {
		return
			(c >= 'a' && c <= 'z') || 
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			(c == '_'); }

	void scan(const PathString& filename) {
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

	SmallString populate() {
		const Timer timer;
		files.clear();
		locations.clear();
		process_files(".", [&](const char* path) {
			scan(path);
		});
		return SmallString("populate database (") + 
			SmallString(files.size()) + " files, " + 
			SmallString(locations.size()) + " symbols) in " + timer.us();
	}

	template <typename F>
	void process(F func) {
		for (const auto& location : locations) {
			if (!func(files[location.file_index], location))
				break;
		}
	}
};

