#pragma once

class Index {
	std::vector<std::string> paths;

public:
	std::string populate() {
		const Timer timer;
		paths.clear();
		process_files(".", [&](const auto& path) {
			paths.push_back(path);
		});
		return std::string("populate (") + 
			std::to_string(paths.size()) + " paths) in " + timer.us();
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
			std::vector<char> contents(size);
			memcpy(contents.data(), mem, contents.size());
			files.emplace_back(filename, std::move(contents));
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
	std::string populate() {
		const Timer timer;
		files.clear();
		process_files(".", [&](const auto& path) {
			add(path);
		});
		return std::string("populate (") + 
			std::to_string(files.size()) + " files) in " + timer.us();
	}

	std::string generate(const std::string_view pattern) {
		const Timer timer;
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
			list += file.name + "(" + std::to_string(pos) + ")" + make_line(cut_line(context, pos - start));
		}
		return list;
	}
};

