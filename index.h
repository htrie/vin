#pragma once

bool allowed_extension(const std::string_view ext) {
	if (ext == ".diff") return true;
	else if (ext == ".bat") return true;
	else if (ext == ".cpp") return true;
	else if (ext == ".c") return true;
	else if (ext == ".h") return true;
	else if (ext == ".cpp") return true;
	else if (ext == ".hpp") return true;
	else if (ext == ".inc") return true;
	else if (ext == ".frag") return true;
	else if (ext == ".vert") return true;
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
					if (allowed_extension(path.path().extension().generic_string())) {
						paths.push_back(path.path().generic_string());
					}
				}
			}
		}
	}

public:
	void populate() {
		paths.clear();
		populate_directory(".");
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
		std::string context;
		size_t position;
	};

	std::map<std::string, std::vector<Location>> locations;

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
					const auto context = text.substr(location > 20 ? location - 20 : 0, index - location + 20 * 2);
					locations[symbol].emplace_back(filename, context, location);
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
					if (allowed_extension(path.path().extension().generic_string())) {
						scan(path.path().generic_string());
					}
				}
			}
		}
	}

public:
	void populate() {
		locations.clear();
		populate_directory(".");
	}

	template <typename F>
	void process(F func) {
		for (const auto& location : locations) {
			if (!func(location.first, location.second))
				break;
		}
	}
};

