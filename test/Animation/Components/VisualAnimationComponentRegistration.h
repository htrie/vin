#pragma once

#include <memory>

struct RenderFrameMoveState;

namespace Animation
{
	namespace Components
	{
		struct VisualRegistrars;

		struct RegisterVisualComponents
		{
			RegisterVisualComponents( );
			~RegisterVisualComponents( );
		private:
			std::unique_ptr< VisualRegistrars > registrars;
		};

	}
}
