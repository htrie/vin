#pragma once

#include "Visual/Python/Interpreter.h"

#if PYTHON_ENABLED
namespace Python
{
	namespace Modules
	{
		namespace AppModule
		{
			void Initialise( pybind11::module& mod );
		}
	}
}
#endif