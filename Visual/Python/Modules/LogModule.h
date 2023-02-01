#pragma once

#include "Visual/Python/Module.h"

#if PYTHON_ENABLED
namespace Python
{
	namespace Modules
	{
		class LogModule : public Module
		{
		public:
			// Module Registration
			static const char* GetModuleName() { return "log"; }
			static PyObject* CreateModule();

			// Module Methods
			static PyObject* Debug( PyObject *self, PyObject *args );
			static PyObject* Info( PyObject *self, PyObject *args );
			static PyObject* Warn( PyObject *self, PyObject *args );
			static PyObject* Crit( PyObject *self, PyObject *args );
		};
	}
}
#endif