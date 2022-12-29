#pragma once

uint64_t fnv64(const char* s, size_t count) {
	const uint64_t basis = 14695981039346656037u;
	const uint64_t prime = 1099511628211u;
	uint64_t hash = basis;
	for (int i = 0; i < count; ++i)
		hash = (hash ^ s[i]) * prime;
	hash = (hash ^ '+') * prime;
	hash = (hash ^ '+') * prime;
	return hash;
}

template<typename T> static constexpr T min(T a, T b) { return (a < b) ? a : b; }
template<typename T> static constexpr T max(T a, T b) { return (a > b) ? a : b; }
template<typename T> static constexpr T clamp(T x, T a, T b) { return min(max(x, a), b); }

