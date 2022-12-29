#pragma once

void verify(bool expr) {
#ifndef NDEBUG
	if (!(expr)) {
		abort();
	}
#endif
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

template <size_t N>
class String {
	size_t len = 0; // Not including EOS.
	std::array<char, N> chars; // Contains EOS.

	size_t resize(size_t n) {
		const size_t old = len;
		len = std::min(N - 1, n);
		chars[len] = 0;
		return old;
	}

	void move(size_t old, size_t from, size_t to) {
		memcpy(&chars[to], &chars[from], min(old - from, len - to));
	}

public:
	String() noexcept {
		chars[len] = 0;
	}

	explicit String(const float f) {
		char buf[_CVTBUFSIZE];
		_gcvt(f, 12, buf);
		*this = String((char*)buf);
	}

	explicit String(const double f) {
		char buf[_CVTBUFSIZE];
		_gcvt(f, 12, buf);
		*this = String((char*)buf);
	}

	explicit String(const int i) {
		char buf[_MAX_ITOSTR_BASE10_COUNT];
		_itoa(i, buf, 10);
		*this = String((char*)buf);
	}

	explicit String(const unsigned u) {
		char buf[_MAX_ULTOSTR_BASE10_COUNT];
		_ultoa(u, buf, 10);
		*this = String((char*)buf);
	}

	explicit String(const int64_t i) {
		char buf[_MAX_I64TOSTR_BASE10_COUNT];
		_i64toa(i, buf, 10);
		*this = String((char*)buf);
	}

	explicit String(const uint64_t u) {
		char buf[_MAX_U64TOSTR_BASE10_COUNT];
		_ui64toa(u, buf, 10);
		*this = String((char*)buf);
	}

	String(const char* s) {
		resize(strlen(s));
		memcpy(chars.data(), s, len);
	}

	String(const char* s, size_t l) {
		resize(l);
		memcpy(chars.data(), s, len);
	}

	String(const std::string_view s) {
		resize(s.size());
		memcpy(chars.data(), s.data(), len);
	}

	template <size_t M> String(const char(&_chars)[M]) {
		resize(M);
		for (size_t i = 0; i < len; ++i)
			chars[i] = _chars[i];
	}

	template <size_t M> String(const String<M>& other) {
		resize(other.size());
		memcpy(chars.data(), other.data(), len);
	}

	static constexpr auto npos{ static_cast<size_t>(-1) };

	void clear() {
		resize(0);
	}

	void pop_back() {
		if (len > 0)
			resize(len - 1);
	}

	void insert(size_t off, const String& s) {
		if (off < len) {
			const auto old = resize(len + s.size());
			move(old, off, off + s.size());
			memcpy(&chars[off], s.data(), s.size());
		}
	}

	void erase(size_t off, size_t count) {
		if (off < len) {
			const auto n = min(len - off, count);
			move(len, min(len, off + count), off);
			resize(len - n);
		}
	}

	String substr(size_t off, size_t count) const {
		if (off < len) {
			return String(&chars[off], min(len - off, count));
		}
		return {};
	}

	size_t find(const std::string_view s, size_t pos = 0) {
		if (len >= pos + s.size())
			for (size_t i = pos; i < len; ++i)
				if (len - i >= s.size())
					if (strncmp(&chars[i], s.data(), s.size()) == 0)
						return i;
		return npos;
	}

	size_t rfind(const std::string_view s, size_t pos = 0) {
		if (pos < len && len >= s.size())
			for (size_t i = pos; i < len; --i)
				if (len - i >= s.size())
					if (strncmp(&chars[i], s.data(), s.size()) == 0)
						return i;
		return npos;
	}

	bool starts_with(const std::string_view s) {
		if (len >= s.size())
			if (strncmp(chars.data(), s.data(), s.size()) == 0)
				return true;
		return false;
	}

	String& operator+=(const String& other) {
		const auto old = resize(len + other.size());
		memcpy(&chars[old], other.data(), len - old);
		return *this;
	}

	String& operator+=(const char* s) {
		const auto old = resize(len + strlen(s));
		memcpy(&chars[old], s, len - old);
		return *this;
	}

	String operator+=(const char& c) {
		const auto old = resize(len + 1);
		chars[old] = c;
		return *this;
	}

	String operator+(const String& other) const { return String(*this) += other; }
	String operator+(const char* s) const { return String(*this) += s; }
	String operator+(const char& c) const { return String(*this) += c; }

	template <size_t M> String& operator+=(const char(&_chars)[M]) {
		const auto old = resize(len + M);
		for (size_t i = 0; i < len - old; ++i)
			chars[old + i] = _chars[i];
		return *this;
	}

	template <size_t M> String& operator+=(const String<M>& other) {
		const auto old = resize(len + other.size());
		memcpy(&chars[old], other.data(), len - old);
		return *this;
	}

	template <size_t M> String operator+(const char(&_chars)[M]) const { return String(*this) += _chars; }
	template <size_t M> String operator+(const String<M>& other) const { return String(*this) += other; }

	template <size_t M> bool operator==(const String<M>& other) const {
		if (size() != other.size())
			return false;
		return memcmp(data(), other.data(), size()) == 0;
	}

	template <size_t M> bool operator!=(const String<M>& other) const { return !operator==(other); }

	explicit operator bool() const { return len > 0; }

	operator std::basic_string_view<char>() const { return std::basic_string_view<char>(chars.data(), len); }

	String tolower() const {
		String s = *this;
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); } );
		return s;
	}

	const char& operator[](size_t index) const { verify(index < len); return chars[index]; }
	char& operator[](size_t index) { verify(index < len); return chars[index]; }

	const char* c_str() const { return chars.data(); }
	char* c_str() { return chars.data(); }

	const char* begin() const { return &chars[0]; }
	char* begin() { return &chars[0]; }
	const char* end() const { return &chars[len]; }
	char* end() { return &chars[len]; }

	const char* data() const { return chars.data(); }
	char* data() { return chars.data(); }

	size_t length() const { return len; }
	size_t size() const { return len; }

	static constexpr size_t max_size() { return N - 1; }

	bool empty() const { return len == 0; }
};

typedef String<64> SmallString;
typedef String<MAX_PATH> PathString;
typedef String<128 * 1024> HugeString;
