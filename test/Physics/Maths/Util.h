#pragma once



const double pi	= 3.141592653;

template<typename T>
inline const T sqr(const T &val)
{
	return val * val;
}

template<typename T>
const T& Max(const T &a, const T &b)
{
	if(a > b) 
		return a;
	else
		return b;
}

template<typename T>
const T& Min(const T &a, const T &b)
{
	if(a < b) 
		return a;
	else
		return b;
}

template<typename T>
const T sgn(const T &c)
{
	return (c > 0.0f) ? T(1.0) : T(-1.0);
}
