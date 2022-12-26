#pragma once

template <typename T, size_t N>
class Array
{
	T values[N];
	size_t count = 0;

public:
	Array() {}
	Array(const std::initializer_list<T> _values)
	{
		for (auto& value : _values)
			push_back(value);
	}
	template <size_t M>
	Array(const T(&_values)[M])
		: count(std::min(M, N))
	{
		for (size_t i = 0; i < count; ++i)
			values[i] = _values[i];
	}
	template <size_t M>
	Array(const Array<T, M>& other)
		: count(std::min(other.size(), N))
	{
		for (size_t i = 0; i < count; ++i)
			values[i] = other[i];
	}

	void clear()
	{
		for (size_t i = 0; i < count; ++i)
			values[i] = T();
		count = 0;
	}

	void erase(T* value)
	{
		verify(!empty() && "Cannot erase from empty array");
		verify((value >= begin()) && (value < end()) && "Value does not belong to array");
		*value = std::move(values[count-1]);
		values[count-1] = T();
		count--;
	}

	void push_back(const T& value)
	{
		if (count < N)
			values[count++] = value;
	}

	void pop_back()
	{
		if (count > 0)
		{
			values[count - 1] = T();
			count--;
		}
	}

	void resize(size_t new_size)
	{
		verify(new_size <= N && "No more space in array");
		if (new_size > count)
			count = new_size; // Values are already default-initialised
		else while (count > new_size)
			pop_back();
	}

	template<typename... ARGS>
	void emplace_back(ARGS&&... args)
	{
		if (count < N)
			values[count++] = T(std::forward<ARGS>(args)...);
	}

	template <size_t M>
	bool operator==(const Array<T, M>& other) const
	{
		if (count != other.size())
			return false;
		for (size_t i = 0; i < count; ++i)
			if (values[i] != other[i])
				return false;
		return true;
	}

	const T& operator[](size_t index) const { return values[index]; }
	T& operator[](size_t index) { return values[index]; }
	const T& at(size_t index) const { return values[index]; }
	T& at(size_t index) { return values[index]; }

	T& front() { return values[0]; }
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
