#pragma once

#include <memory>

namespace Animation
{
	namespace Components
	{
		struct VisualRegistrars;

		struct RegisterVisualComponents
		{
			RegisterVisualComponents();
			~RegisterVisualComponents();

			void Deregister();
		private:
			std::unique_ptr< VisualRegistrars > registrars;
		};

	}
}
