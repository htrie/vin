#pragma once

#include "Modules/GameModule.h"
#include "Modules/LogModule.h"

#define DEFINE_MODULE(x) PYBIND11_EMBEDDED_MODULE( x, mod ) { x##Module::Initialise( mod ); };

namespace Python
{
	namespace Modules
	{
		DEFINE_MODULE( Game );
		DEFINE_MODULE( Log );
	}
}