#pragma once

bool accept(const PathString& filename) {
	if (filename.ends_with(".c")) return true;
	if (filename.ends_with(".h")) return true;
	if (filename.ends_with(".cpp")) return true;
	if (filename.ends_with(".hpp")) return true;
	if (filename.ends_with(".inc")) return true;
	if (filename.ends_with(".txt")) return true;
	if (filename.ends_with(".bat")) return true;
	if (filename.ends_with(".frag")) return true;
	if (filename.ends_with(".vert")) return true;
	return false;
}

class Index {
	Array<PathString, 1024> paths;

public:
	SmallString populate() {
		const Timer timer;
		paths.clear();
		process_files(".", [&](const auto& path) {
			const PathString filename(path);
			if (accept(filename))
				paths.push_back(filename);
		});
		return SmallString("populate index (") + 
			SmallString(paths.size()) + " paths) in " + timer.us();
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
	struct File {
		PathString name;
		size_t size;
	};

	struct Location {
		size_t file_index = 0;
		size_t position = 0;
	};

	Array<File, 1024> files;
	Array<Location, 128 * 1024> locations;

	void scan(const PathString& filename, const SmallString& pattern) {
		map(filename, [&](const char* mem, size_t size) {
			files.emplace_back(filename, size);
			for (size_t i = 0; i < size; ++i) {
				if (mem[i] == pattern[0]) {
					if (strncmp(&mem[i], pattern.data(), pattern.size()) == 0) {
						locations.emplace_back(files.size() - 1, i);
					}
				}
			}
		});
	}

public:
	SmallString search(const SmallString& pattern) {
		const Timer timer;
		files.clear();
		locations.clear();
		if (!pattern.empty()) {
			process_files(".", [&](const auto& path) {
				const PathString filename(path);
				if (accept(filename))
					scan(filename, pattern);
			});
		}
		return SmallString("search database (") + 
			SmallString(files.size()) + " files, " + 
			SmallString(locations.size()) + " symbols) in " + timer.us();
	}

	template <typename F>
	void process(F func) const {
		for (const auto& location : locations) {
			if (!func(files[location.file_index], location))
				break;
		}
	}
};

