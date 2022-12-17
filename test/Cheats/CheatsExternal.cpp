#if GGG_CHEATS == 1 && !defined( PRODUCTION_CONFIGURATION )

#ifdef WIN32
#	include <Windows.h>
#endif
#include <iostream>
#include <sstream>

#include "CheatsExternal.h"

#include "Visual/Device/WindowDX.h"

namespace
{
	CheatsExternal* inter = nullptr;

	LRESULT CALLBACK CheatMsgProc( HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param,
		bool* no_further_processing, void* user_context )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		if ( msg == WM_COPYDATA )
		{
			COPYDATASTRUCT* pData = ( COPYDATASTRUCT* ) l_param;

			const auto len = pData->cbData;
			const std::wstring raw_message( ( const wchar_t* ) pData->lpData, ( const wchar_t* ) ( ( char* ) pData->lpData + len ) );
			
			std::wstringstream stream( raw_message );
			std::wstring read, from;

			stream >> read;
			from = read;
			stream >> read;

			const std::wstring separator = L":MSG:";

			while ( read != separator )
			{
				from += L" " + read;
				stream >> read;
			}

			// Path Of Exile:MSG:This is a test message.
			std::wstring message = raw_message.substr( from.size() + separator.size() + 2 );
			inter->Receive( message, from, static_cast< int >( pData->dwData ) );
		}
#endif
		return 0;
	}
}

void CheatsExternal::Send( const std::wstring& message, const std::wstring& to, const int value )
{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	result_code = -1;

	HWND hwnd = FindWindow( to.c_str( ), NULL );
	if ( hwnd == NULL )
		return;

	inter = this;

	waiting_for_message = true;
	expecting_message_from = to;

	std::wstring sent_message;

	Device::WindowDX* window = Device::WindowDX::GetWindow( );
	auto thisHwnd = window->GetWnd( );

	auto default_msg_proc = window->GetMsgProcCallBack( );
	auto user = window->GetUserContextPtr();
	window->SetMsgProcCallback( CheatMsgProc, user );

	wchar_t buffer[500];
	const int num = GetClassName( thisHwnd, buffer, 500 );

	sent_message.assign( buffer, buffer + num );
	sent_message += L" :MSG: ";
	sent_message += message;

	COPYDATASTRUCT cds;
	cds.dwData = value;
	cds.cbData = static_cast< DWORD >( sent_message.size() * sizeof( sent_message[ 0 ] ) );
	cds.lpData = ( void* ) sent_message.c_str( );

	result = L"";
	SendMessageTimeout( hwnd, WM_COPYDATA, ( WPARAM ) hwnd, ( LPARAM ) ( LPVOID ) &cds, 0, 0, nullptr );
	window->SetMsgProcCallback( default_msg_proc, user );
#endif
}

void CheatsExternal::Receive( const std::wstring& message, const std::wstring& from, const int value )
{
	if ( from != expecting_message_from )
		return;

	waiting_for_message = value == 0;
	result += message + L"\n";
	result_code = value;
}

#endif
