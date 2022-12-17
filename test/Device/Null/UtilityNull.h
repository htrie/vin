#pragma once


struct ExceptionNull : public std::runtime_error
{
	ExceptionNull(const std::string text) : std::runtime_error(text.c_str())
	{
		LOG_CRIT(L"[Null] " << StringToWstring(text));
		abort(); // Exit immediately to get callstack.
	}
};
