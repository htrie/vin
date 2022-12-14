#pragma once

void verify(bool expr) {
#ifndef NDEBUG
	if (!(expr)) {
		abort();
	}
#endif
}

void error(std::string_view err_msg, std::string_view err_class) {
	do {
		MessageBox(nullptr, err_msg.data(), err_class.data(), MB_OK);
		exit(1);
	} while (0);
}

std::string timestamp() {
	const auto now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()}.get_local_time();
	const auto start_of_day = std::chrono::floor<std::chrono::days>(now);
	const auto time_since_start_of_day = std::chrono::round<std::chrono::seconds>(now - start_of_day);
	const std::chrono::hh_mm_ss hms{ time_since_start_of_day };
	return std::format("{:%r}", hms);
}

std::string tolower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); } );
	return s;
}

