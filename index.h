#pragma once

class Index {
	std::vector<std::string> paths;

	bool allowed_extension(const std::string_view ext) {
		if (ext == ".diff") return true;
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

public:
	void populate() {
		// go through all files
		// open and parse all words
		// store all words filenames and locations
		// avoid storing filename multiple times
		// use unordered map
	}
};

