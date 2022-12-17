#pragma once

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
					paths.push_back(path.path().generic_string());
				}
			}
		}
	}

public:
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
	struct Location {
		std::string filename;
		std::string pre_context;
		std::string post_context;
		size_t position;
	};

	std::map<size_t, std::vector<Location>> locations;

	constexpr bool is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'); }

	void scan(const std::string& filename) {
		std::ifstream in(filename);
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		size_t index = 0;
		size_t location = 0;
		for (const auto& c : text) {
			if (!is_letter(c)) {
				if (location != index) {
					const auto symbol = text.substr(location, index - location);
					const auto pre_context = text.substr(location > 20 ? location - 20 : 0, location > 20 ? 20 : location);
					const auto post_context = text.substr(index, 20);
					const size_t symbol_hash = std::hash<std::string>{}(tolower(symbol));
					locations[symbol_hash].emplace_back(filename, pre_context, post_context, location);
				}
				location = index;
				location++;
			}
			index++;
		}
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
					scan(path.path().generic_string());
				}
			}
		}
	}

public:
	std::string populate() {
		const Timer timer;
		locations.clear();
		populate_directory(".");
		return std::string("populate database in ") + timer.us();
	}

	template <typename F>
	void process(F func) {
		for (const auto& location : locations) {
			if (!func(location.first, location.second))
				break;
		}
	}
};

