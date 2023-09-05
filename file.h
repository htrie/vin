#pragma once

static inline constexpr size_t B = 1;
static inline constexpr size_t KB = 1024 * B;
static inline constexpr size_t MB = 1024 * KB;
static inline constexpr size_t GB = 1024 * MB;

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6)
{
	std::ostringstream out;
	out.precision(n);
	out << std::fixed << a_value;
	return out.str();
}

std::string readable_size(size_t size) {
	if (size > GB) return to_string_with_precision((float)size / (float)GB, 2) + "gb";
	else if (size > MB) return to_string_with_precision((float)size / (float)MB, 1) + "mb";
	else if (size > KB) return to_string_with_precision((float)size / (float)KB, 1) + "kb";
	else return std::to_string(size) + "b";
}

class Timer {
	long long performance_frequency = 0;
	int64_t start_time = 0;

	int64_t get_time_ms() const {
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);
		return (counter.QuadPart * 1000) / performance_frequency;
	}

public:
	Timer() {
		LARGE_INTEGER perf_freq;
		QueryPerformanceFrequency(&perf_freq);
		performance_frequency = perf_freq.QuadPart;
		start_time = get_time_ms();
	}

	int64_t get_elapsed_time_ms() const {
		return get_time_ms() - start_time;
	}

};

bool ignore_file(const std::string_view path) {
	if (path.ends_with(".db")) return true;
	if (path.ends_with(".aps")) return true;
	if (path.ends_with(".bin")) return true;
	if (path.ends_with(".dat")) return true;
	if (path.ends_with(".exe")) return true;
	if (path.ends_with(".idb")) return true;
	if (path.ends_with(".ilk")) return true;
	if (path.ends_with(".iobj")) return true;
	if (path.ends_with(".ipdb")) return true;
	if (path.ends_with(".jpg")) return true;
	if (path.ends_with(".lib")) return true;
	if (path.ends_with(".obj")) return true;
	if (path.ends_with(".pch")) return true;
	if (path.ends_with(".pdb")) return true;
	if (path.ends_with(".png")) return true;
	if (path.ends_with(".tlog")) return true;
	return false;
}

template<typename F>
void process_files(const std::string& path, F func) {
	const auto filter = path + "/*";
	WIN32_FIND_DATA find_data;
	HANDLE handle = FindFirstFile(filter.c_str(), &find_data);
	do {
		if (handle != INVALID_HANDLE_VALUE) {
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0) {
				if (find_data.cFileName[0] != '.') {
					const auto dir = path + "/" + find_data.cFileName;
					process_files(dir, func);
				}
			}
			else if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				const auto file = path + "/" + find_data.cFileName;
				func(file);
			}
		}
	} while (FindNextFile(handle, &find_data) != 0);
	FindClose(handle);
}

template <typename F>
void map(const std::string_view filename, F func) {
	if (const auto file = CreateFileA(filename.data(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr); file != INVALID_HANDLE_VALUE) {
		if (size_t size = 0; GetFileSizeEx(file, (PLARGE_INTEGER)&size)) {
			if (const auto mapping = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr); mapping != INVALID_HANDLE_VALUE) {
				if (const auto mem = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0); mem != nullptr) {
					func((char*)mem, size);
					UnmapViewOfFile(mem);
				}
				CloseHandle(mapping);
			}
		}
		CloseHandle(file);
	}
}

bool write(const std::string_view filename, const std::string_view text) {
	bool res = false;
	if (const auto file = CreateFileA(filename.data(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); file != INVALID_HANDLE_VALUE) {
		res = WriteFile(file, text.data(), (DWORD)text.size(), nullptr, nullptr);
		CloseHandle(file);
	}
	return res;
}

std::string list() {
	std::string list;
	process_files(".", [&](const auto& path) {
		if (!ignore_file(path))
			list += path + "\n";
		});
	return list;
}

std::string_view cut_line(const std::string_view text, size_t pos) {
	Line line(text, pos);
	return line.to_string(text);
}

std::string make_line(const std::string_view text) {
	if (text.size() > 0)
		if (text[text.size() - 1] == '\n')
			return std::string(text);
	return std::string(text) + "\n";
}

std::string make_entry(size_t pos, const char* mem, size_t size) {
	const auto start = pos > (size_t)100 ? pos - (size_t)100 : (size_t)0;
	const auto count = std::min((size_t)200, size - start);
	const auto context = std::string(&mem[start], count);
	return "(" + std::to_string(pos) + ") " + make_line(cut_line(context, pos - start));
}

std::string scan(const std::string_view path, const std::string_view pattern) {
	std::string list;
	map(path, [&](const char* mem, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			if (mem[i] == pattern[0]) {
				if (strncmp(&mem[i], pattern.data(), std::min(pattern.size(), size - i)) == 0) {
					list += std::string(path) + make_entry(i, mem, size);
				}
			}
		}
		});
	return list;
}

std::string find(const std::string_view pattern) {
	std::string list;
	if (!pattern.empty() && pattern.size() > 2) {
		process_files(".", [&](const auto& path) {
			if (!ignore_file(path)) {
				list += scan(path, pattern);
			}
			});
	}
	return list;
}

std::string load(const std::string_view filename) {
	std::string text = "\n";
	if (std::filesystem::exists(filename)) {
		map(filename, [&](const char* mem, size_t size) {
			text = std::string(mem, size);
		});
	}
	return text;
}

class File {
	const uint8_t* memory = nullptr;
	uint_fast32_t size = 0;
	HANDLE mapping = nullptr;

public:
	File() {}
	File(const std::string_view filename) {
		const auto file = CreateFileA(filename.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (file != INVALID_HANDLE_VALUE) {
			DWORD high = 0;
			const auto low = GetFileSize(file, &high);
			if (low != INVALID_FILE_SIZE) {
				size = (size_t)high << (8 * sizeof(DWORD)) | low;
				mapping = CreateFileMapping(file, NULL, PAGE_READONLY, high, low, NULL);
				if (mapping) {
					memory = (uint8_t*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
				}
			}
		}
		CloseHandle(file);
	}

	~File() {
		if (memory) {
			UnmapViewOfFile(memory);
		}
		if (mapping) {
			CloseHandle(mapping);
		}
	}

	uint_fast32_t get_size() const { return size; }
	const uint8_t* get_memory() const { return memory; }
};

