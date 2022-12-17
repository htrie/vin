#pragma once

#include "Visual/Python/Module.h"

#if PYTHON_ENABLED

namespace Python
{
	namespace Modules
	{
		namespace MathModule
		{
			void Initialise( pybind11::module& mod );
		}
	}
}
#endif