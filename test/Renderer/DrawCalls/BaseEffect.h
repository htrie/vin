#pragma once

#include "Effect.h"

namespace Renderer
{
	namespace DrawCalls
	{

		class BaseEffect : public Effect
		{
		public:
			BaseEffect( const char* name, const D3DVERTEXELEMENT9* vertex_elements ) : Effect( name ), vertex_elements( vertex_elements ) { }

			const D3DVERTEXELEMENT9 *const vertex_elements;
		};

	}
}
