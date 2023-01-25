#pragma once

void verify(bool expr) {
#ifndef NDEBUG
	if (!(expr)) {
		abort();
	}
#endif
}

inline constexpr size_t B = 1;
inline constexpr size_t KB = 1024 * B;
inline constexpr size_t MB = 1024 * KB;
inline constexpr size_t GB = 1024 * MB;

template <typename T>
std::string to_string_with_precision(const T a, const int n) {
	std::ostringstream out;
	out.precision(n);
	out << std::fixed << a;
	return out.str();
}

std::string readable_size(size_t size) {
	if (size > GB) return to_string_with_precision((float)size / (float)GB, 2) + "GB";
	else if (size > MB) return to_string_with_precision((float)size / (float)MB, 1) + "MB";
	else if (size > KB) return to_string_with_precision((float)size / (float)KB, 1) + "KB";
	else return std::to_string(size) + "B";
}
