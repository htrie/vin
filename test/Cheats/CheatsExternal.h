#if GGG_CHEATS == 1 && !defined( PRODUCTION_CONFIGURATION )

#pragma once

#include <string>

class CheatsExternal
{
protected:
	std::wstring result;
	std::wstring expecting_message_from;
	int result_code;

	volatile bool waiting_for_message = false;

public:
	virtual void Send( const std::wstring& message, const std::wstring& to, const int value = 0 );
	virtual void Receive( const std::wstring& message, const std::wstring& from, const int value = 0 );

	std::wstring GetResult( ) { return std::move( result ); }
	int GetResultCode( ) const { return result_code; }
};

#endif