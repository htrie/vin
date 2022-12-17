#pragma once

#include "Visual/Python/Module.h"

#if PYTHON_ENABLED

namespace Python
{
	namespace Modules
	{
		namespace ImGuiModule
		{
			void Initialise( pybind11::module& mod );
		}
	}
}
#endif