#pragma once

void verify(bool expr) {
#ifndef NDEBUG
	if (!(expr)) {
		abort();
	}
#endif
}
