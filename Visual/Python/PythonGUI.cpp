#include "PythonGUI.h"

#if PYTHON_ENABLED

#pragma warning( push )
#pragma warning( disable : 4522 ) // multiple assignment operators specified
#include "pybind11/pybind11/pytypes.h"
#pragma warning( pop )

#include "Common/Resource/Exception.h"

#define CATCH catch( const pybind11::error_already_set& e ) \
{ \
	Resource::DisplayException( e ); \
	if( auto* interpreter = Python::GetInterpreter() )\
		interpreter->AddText( e.what() ); \
}

namespace Python
{
	PythonGUI::PythonGUI()
		: GUIInstance( true )
	{
	}

	PythonGUI::~PythonGUI()
	{
	}

	void PythonGUI::Initialise()
	{
		try { initialise(); } CATCH
	}

	void PythonGUI::Destroy()
	{
		try { destroy(); } CATCH
	}

	void PythonGUI::OnRenderMenu( const float elapsed_time )
	{
		try { on_render_menu( elapsed_time ); } CATCH
	}

	void PythonGUI::OnRender( const float elapsed_time )
	{
		try { on_render( elapsed_time ); } CATCH
	}
}
#endif