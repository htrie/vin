#include "WindowsUtility.h"

namespace Utility
{

#if !defined(PS4) && !defined( __APPLE__ ) && !defined( ANDROID )

	std::unique_ptr< HANDLE, HandleCloser< > > CreateEventHandle( const bool manual, const bool initial_state, const bool inheritable)
	{
		SECURITY_ATTRIBUTES security_attributes = { sizeof(SECURITY_ATTRIBUTES) , nullptr, inheritable };
		const auto handle = CreateEvent( &security_attributes, manual, initial_state, NULL );
		if( !handle )
			throw std::runtime_error( "Unable to create event object" );
		return std::unique_ptr< HANDLE, HandleCloser<> >( handle );
	}

#endif

}
