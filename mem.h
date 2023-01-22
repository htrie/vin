#pragma once

void verify(bool expr) {
#ifndef NDEBUG
	if (!(expr)) {
		abort();
	}
#endif
}

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

template <typename T, size_t N>
class Array {
	T values[N];
	size_t count = 0;

public:
	Array() {}
	Array(size_t size)
		: count(size) {}
	Array(const std::initializer_list<T> _values) {
		for (auto& value : _values)
			push_back(value);
	}
	template <size_t M>
	Array(const T(&_values)[M])
		: count(min(M, N)) {
		for (size_t i = 0; i < count; ++i)
			values[i] = _values[i];
	}
	template <size_t M>
	Array(const Array<T, M>& other)
		: count(min(other.size(), N)) {
		for (size_t i = 0; i < count; ++i)
			values[i] = other[i];
	}

	void clear() {
		for (size_t i = 0; i < count; ++i)
			values[i] = T();
		count = 0;
	}

	void erase(T* value) {
		verify(!empty() && "Cannot erase from empty array");
		verify((value >= begin()) && (value < end()) && "Value does not belong to array");
		*value = std::move(values[count-1]);
		values[count-1] = T();
		count--;
	}

	void push_back(const T& value) {
		if (count < N)
			values[count++] = value;
	}

	void pop_front() {
		if (count > 0) {
			for (size_t i = 0; i < count-1; ++i)
				values[i] = std::move(values[i + 1]);
			count--;
		}
	}

	void pop_back() {
		if (count > 0) {
			values[count - 1] = T();
			count--;
		}
	}

	void resize(size_t new_size) {
		verify(new_size <= N && "No more space in array");
		if (new_size > count)
			count = new_size; // Values are already default-initialised
		else while (count > new_size)
			pop_back();
	}

	template<typename... ARGS> void emplace_back(ARGS&&... args) {
		if (count < N)
			values[count++] = T(std::forward<ARGS>(args)...);
	}

	template <size_t M> bool operator==(const Array<T, M>& other) const {
		if (count != other.size())
			return false;
		for (size_t i = 0; i < count; ++i)
			if (values[i] != other[i])
				return false;
		return true;
	}

	const T& operator[](size_t index) const { verify(index < count); return values[index]; }
	T& operator[](size_t index) { verify(index < count); return values[index]; }
	const T& at(size_t index) const { verify(index < count); return values[index]; }
	T& at(size_t index) { verify(index < count); return values[index]; }

	const T& front() const { return values[0]; }
	T& front() { return values[0]; }
	const T& back() const { return values[count - 1]; }
	T& back() { return values[count - 1]; }

	const T* begin() const { return &values[0]; }
	T* begin() { return &values[0]; }
	const T* end() const { return &values[count]; }
	T* end() { return &values[count]; }

	const T* data() const { return values; }
	T* data() { return values; }

	size_t size() const { return count; }
	size_t max_size() const { return N; }

	bool empty() const { return count == 0; }
	bool full() const { return count == N; }
};
