#pragma once

#include "Interpreter.h"

#if PYTHON_ENABLED == 1
#include "Visual/GUI2/GUIInstance.h"

namespace Python
{
	class PythonGUI : Device::GUI::GUIInstance
	{
	public:
		PythonGUI();
		~PythonGUI();

		// C++
		virtual void Initialise() override;
		virtual void Destroy() override;
		virtual void OnRenderMenu( const float elapsed_time ) override;
		virtual void OnRender( const float elapsed_time ) override;

		// PYTHON
		virtual void initialise() {}
		virtual void destroy() {}
		virtual void on_render_menu( const float elapsed_time ) {}
		virtual void on_render( const float elapsed_time ) {}
	};
}
#endif