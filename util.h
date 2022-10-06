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

