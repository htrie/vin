#pragma once

bool accept(const std::string_view extension) {
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

	void populate_directory(const std::filesystem::path& dir) {
		for (const auto& path : std::filesystem::directory_iterator{ dir }) {
			if (path.is_directory()) {
				const auto dirname = path.path().generic_string();
				if (dirname.size() > 0 && (dirname.find("/.") == std::string::npos)) { // Skip hidden directories.
					populate_directory(path);
				}
			} else if (path.is_regular_file()) {
				const auto filename = path.path().generic_string();
				if (filename.size() > 0 && (filename.find("/.") == std::string::npos)) { // Skip hidden files.
					if (accept(path.path().extension().generic_string()))
						paths.push_back(path.path().generic_string());
				}
			}
		}
	}

public:
	void reset() {
		paths.clear();
	}

	std::string populate() {
		const Timer timer;
		paths.clear();
		populate_directory(".");
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

	constexpr bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'); }

	void scan(const std::string& filename) {
		map(filename, [&](const char* mem, size_t size) {
			files.emplace_back(filename, size);
			size_t location = 0;
			for (size_t i = 0; i < size; ++i) {
				if (!is_letter(mem[i])) {
					if ((location != i) && (i - location > 2)) // Ignore small words.
						locations.emplace_back(files.size() - 1, fnv64(&mem[location], i - location), location);
					location = i;
					location++;
				}
			}
		});
	}

	void populate_directory(const std::filesystem::path& dir) {
		for (const auto& path : std::filesystem::directory_iterator{ dir }) {
			if (path.is_directory()) {
				const auto dirname = path.path().generic_string();
				if (dirname.size() > 0 && (dirname.find("/.") == std::string::npos)) { // Skip hidden directories.
					populate_directory(path);
				}
			} else if (path.is_regular_file()) {
				const auto filename = path.path().generic_string();
				if (filename.size() > 0 && (filename.find("/.") == std::string::npos)) { // Skip hidden files.
					if (accept(path.path().extension().generic_string()))
						scan(path.path().generic_string());
				}
			}
		}
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
		populate_directory(".");
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

