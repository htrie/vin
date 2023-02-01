#pragma once

#include <vector>

#include "Common/Utility/NonCopyable.h"

#include "Common/Geometry/Bounding.h"

namespace Renderer
{
	namespace Scene
	{
		/// A 2D rectangle projected onto the view-plane
		struct ViewRectangle
		{
			ViewRectangle() : top( 0 ), left( 0 ), bottom( 0 ), right ( 0 ) { }
			ViewRectangle( unsigned _top, unsigned _left, unsigned _bottom, unsigned _right ) : top( std::min( _top, 0u ) ), left( std::min( _left, 0u ) ), bottom( _bottom), right( _right ) { }
			unsigned top, left, bottom, right;

			bool Contains( const unsigned x, const unsigned y ) const
			{
				return x >= left && x < right && y >= top && y < bottom;
			}

			void Clamp( const unsigned height )
			{
				unsigned width = 1;
				for( unsigned int i = 0; i < height; ++i )
				{
					width *= 2;
				}

				top = std::max( top, 0u );
				left = std::max( left, 0u );
				bottom = std::min( bottom, width - 1 );
				right = std::min( right, width - 1 );
			}

			void Combine( const ViewRectangle &other )
			{
				top = std::min( top, other.top );
				left = std::min( left, other.left );
				bottom = std::max( bottom, other.bottom );
				right = std::max( right, other.right );
			}
		};

	}
}
