#pragma once

#include "Modules/ImGuiModule.h"
#include "Modules/AppModule.h"
#include "Modules/MathModule.h"
#include "pybind11/pybind11/embed.h"

#if PYTHON_ENABLED
#define DEFINE_MODULE(x) PYBIND11_EMBEDDED_MODULE( x, mod ) { Python::Modules::x##Module::Initialise( mod ); };

namespace Python
{
	namespace Modules
	{
		DEFINE_MODULE( ImGui );
		DEFINE_MODULE( App );
		DEFINE_MODULE( Math );
	}
}
#endif