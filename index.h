#pragma once

class Index { // [TODO] Add global symbol database (file id + symbol character index).
	std::vector<std::string> paths;
	std::mutex paths_mutex;
	std::atomic_bool populating;
	std::future<void> paths_future;

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
						std::unique_lock lock(paths_mutex);
						paths.push_back(path.path().generic_string());
					}
				}
			}
		}
	}

	void populate() {
		paths.clear();
		populating = true;
		paths_future = std::async(std::launch::async, [&]() {
			populate_directory(".");
			populating = false;
		});
	}

public:
	Index() {
		populate();
	}

	void wait() {
		if (paths_future.valid()) {
			paths_future.wait();
		}
	}

	bool is_populating() const { return populating; }

	template <typename F>
	void process(F func) {
		std::unique_lock lock(paths_mutex);
		for (const auto& path : paths) {
			if (!func(path))
				break;
		}
	}
};

