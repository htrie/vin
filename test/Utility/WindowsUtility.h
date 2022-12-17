#pragma once


#include "Common/Utility/WindowsHeaders.h"
#include "Visual/Device/WindowDX.h"
#include <memory>
#include <string>

namespace Utility
{
#if !defined(PS4) && !defined(__APPLE__) && !defined(ANDROID)
	///Functor to release handles.
	///For use with std::unique_ptr
	template< typename T = HANDLE >
	struct HandleCloser
	{
		typedef T pointer;
		void operator()( const T& handle )
		{
			CloseHandle( handle );
		}
	};

	std::unique_ptr< HANDLE, HandleCloser< > > CreateEventHandle(const bool manual, const bool initial_state, const bool inheritable = false);

	///Fuctor to release a COM pointer.
	///For use with std::unique_ptr
	struct ComReleaser
	{
		void operator()( IUnknown* p ) { p->Release(); }
	};
#endif

	///WindowDX deleter for use with std::unique_ptr.  Sends a WM_CLOSE to the window before deleting it.
	struct WindowDXDeleter
	{
		typedef Device::WindowDX* pointer;
		void operator()( const pointer& window )
		{
			Device::WindowDX::SendMessage( window->GetWnd(), WM_CLOSE, 0, 0 );
			delete window;
		}
	};

#if !defined(PS4) && !defined(__APPLE__) && !defined(ANDROID)
	///Functor to free a library
	///For use with std::unique_ptr
	struct LibraryFree
	{
		typedef HMODULE pointer;
		void operator()( pointer handle )
		{
			FreeLibrary( handle );
		}
	};
#endif


}

