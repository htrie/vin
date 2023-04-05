#pragma once

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

std::string get_error_string() {
	const DWORD error = GetLastError();
	std::string res = "(error " + std::to_string((unsigned)error);
	char* message = NULL;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL)) {
		res += ": " + std::string(message);
		LocalFree(message);
	}
	res += ")";
	return res;
}

std::wstring convert(const std::string_view url) {
	if (const auto len = mbstowcs(nullptr, url.data(), 0); len == url.length()) {
		std::wstring res;
		res.resize(len);
		if (const auto converted = mbstowcs(res.data(), url.data(), len + 1); converted == len)
			return res;
	}
	return {};
}

std::string download(const HINTERNET request) {
	std::string contents;
	DWORD size = 0;
	do {
		if (WinHttpQueryDataAvailable(request, &size)) {
			std::vector<char> partial(size);
			DWORD downloaded = 0;
			if (WinHttpReadData(request, (LPVOID)partial.data(), (DWORD)partial.size(), &downloaded) && downloaded == size) {
				contents += std::string(partial.begin(), partial.end());
			}
		}
	} while (size > 0);
	return contents;
}

std::string request(const std::string_view url) {
	std::string contents;
	const auto server = convert(extract_server(url));
	const auto payload = convert(extract_payload(url));
	if (const auto session = WinHttpOpen(L"Vin", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)) {
		if (const auto connection = WinHttpConnect(session, server.data(), INTERNET_DEFAULT_HTTPS_PORT, 0)) {
			if (const auto request = WinHttpOpenRequest(connection, L"GET", payload.data(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)) {
				if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
					if (WinHttpReceiveResponse(request, NULL))
						contents = download(request);
				}
				else { contents = std::string(url) + " Failed to send request " + get_error_string(); }
				WinHttpCloseHandle(request);
			}
			else { contents = std::string(url) + " Failed to open request " + get_error_string(); }
			WinHttpCloseHandle(connection);
		}
		else { contents = std::string(url) + " Failed to establish connection " + get_error_string(); }
		WinHttpCloseHandle(session);
	}
	else { contents = std::string(url) + " Failed to open session " + get_error_string(); }
	return contents;
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

std::string prettify_html(const std::string_view contents) {
	std::string out = std::string(contents);
	// TODO parse html <p> and <a>
	return out;
}

std::string load(const std::string_view filename) {
	std::string text = "\n";
	if (filename.starts_with("www")) {
		text = prettify_html(request(filename)); }
	else if (filename.starts_with("https")) {
		text = prettify_html(request(filename.substr(sizeof("https://") - 1))); }
	else if (filename.starts_with("http")) {
		text = prettify_html(request(filename.substr(sizeof("http://") - 1))); }
	else if (std::filesystem::exists(filename)) {
		map(filename, [&](const char* mem, size_t size) {
			text = std::string(mem, size);
		});
	}
	return text;
}

std::string get_user_font_path() {
	char path[MAX_PATH];
	if (const auto res = SHGetSpecialFolderPathA(NULL, path, CSIDL_LOCAL_APPDATA, FALSE))
		return std::string(path) + "\\Microsoft\\Windows\\Fonts\\";
	return "";
}

std::string get_system_font_path() {
	char win_dir[MAX_PATH];
	GetWindowsDirectoryA(win_dir, MAX_PATH);
	std::stringstream ss;
	ss << win_dir << "\\Fonts\\";
	return ss.str();
}

std::string get_system_font_name(const std::string& face_name) {
	HKEY hkey;
	static const char* registry = "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
	auto result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, registry, 0, KEY_READ, &hkey);
	if (result != ERROR_SUCCESS)
		return "";

	DWORD name_max_size, data_max_size;
	result = RegQueryInfoKey(hkey, 0, 0, 0, 0, 0, 0, 0, &name_max_size, &data_max_size, 0, 0);
	if (result != ERROR_SUCCESS)
		return "";

	DWORD index = 0;
	std::vector<char> name(name_max_size);
	std::vector<BYTE> data(data_max_size);
	std::string font;

	do {
		DWORD data_size = data_max_size;
		DWORD name_size = name_max_size;
		DWORD type;
		const auto result = RegEnumValueA(hkey, index++, name.data(), &name_size, 0, &type, data.data(), &data_size);
		if (result != ERROR_SUCCESS || type != REG_SZ)
			continue;

		std::string font_name(name.data(), name_size);
		if (_strnicmp(face_name.c_str(), font_name.c_str(), face_name.length()) == 0) {
			font.assign((LPSTR)data.data(), data_size);
			break;
		}
	} while (result != ERROR_NO_MORE_ITEMS);

	RegCloseKey(hkey);

	return font;
}

