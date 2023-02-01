
#include "GamepadDeviceState.h"

#if defined(_WIN32) && !defined(_XBOX_ONE)
#	include <hidsdi.h>
#endif

namespace Utility
{
#if defined(_WIN32) && !defined(_XBOX_ONE)
	GamepadDeviceState::HIDInfo::HIDInfo( const HANDLE device_handle, const std::wstring& device_name )
		: device_handle{ device_handle }
		, device_name{ device_name }
	{
		CheckOverlapped();
	}

	GamepadDeviceState::HIDInfo::HIDInfo( HIDInfo&& rhs )
		: device_handle{ rhs.device_handle }
		, device_name{ rhs.device_name }
	{
		std::swap( output_file, rhs.output_file );
		std::swap( overlapped, rhs.overlapped );
		std::swap( output_buffer, rhs.output_buffer );
	}

	GamepadDeviceState::HIDInfo& GamepadDeviceState::HIDInfo::operator=( HIDInfo&& rhs )
	{
		if( this != &rhs )
		{
			device_handle = rhs.device_handle;
			device_name = rhs.device_name;
			std::swap( output_file, rhs.output_file );
			std::swap( overlapped, rhs.overlapped );
			std::swap( output_buffer, rhs.output_buffer );
		}
		return *this;
	}

	GamepadDeviceState::HIDInfo::~HIDInfo()
	{
		if( output_file != INVALID_HANDLE_VALUE )
		{
			DWORD bytes_transfered{};
			GetOverlappedResult( output_file, &overlapped, &bytes_transfered, true );
			CloseHandle( output_file );
			output_file = INVALID_HANDLE_VALUE;
		}
	}

	void GamepadDeviceState::HIDInfo::CheckOverlapped()
	{
		DWORD bytes_transfered{};
		if( const bool finished_last_output = (GetOverlappedResult( output_file, &overlapped, &bytes_transfered, false ) == TRUE) )
		{
			if( output_file != INVALID_HANDLE_VALUE )
			{
				const auto close_result = CloseHandle( output_file );
				assert( close_result == TRUE );
				output_file = INVALID_HANDLE_VALUE;
			}

			assert( output_file == INVALID_HANDLE_VALUE );
			output_file = CreateFileW( device_name.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL );
		}
	}
#endif
}
