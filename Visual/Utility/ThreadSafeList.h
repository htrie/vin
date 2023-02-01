#pragma once

#if defined(WIN32)

#include "Common/Utility/WindowsHeaders.h"

namespace Utility
{
	///Implementation of a first in last out thread safe queue
	template< typename Element >
	class ThreadSafeList
	{
	public:
		ThreadSafeList( )
		{
			list = (PSLIST_HEADER)_aligned_malloc( sizeof( SLIST_HEADER ), MEMORY_ALLOCATION_ALIGNMENT );
			InitializeSListHead( list );
		}

		///@return true if an element was actually popped
		bool Pop( Element& e )
		{
			auto* entry = reinterpret_cast< Entry* >( InterlockedPopEntrySList( list ) );
			if( entry )
			{
				e = std::move( entry->element );
				entry->element.~Element();
				_aligned_free( entry );
				return true;
			}
			else
				return false;
		}

		void Push( const Element& e )
		{
			auto* entry = static_cast< Entry* >( _aligned_malloc( sizeof( Entry ), MEMORY_ALLOCATION_ALIGNMENT ) );
			new( &entry->element ) Element( e );
			InterlockedPushEntrySList( list, &entry->entry );
		}

		void Push( Element&& e )
		{
			auto* entry = static_cast< Entry* >( _aligned_malloc( sizeof( Entry ), MEMORY_ALLOCATION_ALIGNMENT ) );
			new( &entry->element ) Element( std::move( e ) );
			InterlockedPushEntrySList( list, &entry->entry );
		}

		~ThreadSafeList( )
		{
			Element e;
			while( Pop( e ) );
			_aligned_free( list );
		}

	private:

		struct Entry
		{
			SLIST_ENTRY entry;
			Element element;
		};

		PSLIST_HEADER list;

	};

}

#endif
