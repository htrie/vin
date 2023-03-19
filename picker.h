#pragma once

static bool match_pattern(const std::string_view path, const std::string_view lowercase_pattern) {
	const auto s = to_lower(path);
	size_t pos = 0;
	for (auto& c : lowercase_pattern) {
		if (pos = s.find(c, pos); pos == std::string::npos)
			return false;
	}
	return true;
}

class Picker { // TODO Not a class?
public:
	std::string generate(const Index& index) {
		std::vector<std::string> filtered;
		std::string pattern;
		const auto lowercase_pattern = to_lower(pattern); // TODO Update pattern somehow.
		index.process([&](const auto& path) {
			if (match_pattern(path, lowercase_pattern))
				filtered.push_back(path.c_str());
			return true;
		});
		std::string list;
		for (auto& filename : filtered) {
			list += filename + "\n";
		}
		return list;
	}
};

