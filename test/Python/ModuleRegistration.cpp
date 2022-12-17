
#include "ModuleRegistration.h"

#if PYTHON_ENABLED


#include "Modules/GameModule.h"
#include "Modules/LogModule.h"

// Register new modules here...
#define MODULE_REGISTER \
	REGISTER_MODULE( GameModule ) \
	REGISTER_MODULE( LogModule ) \

namespace Python
{
	namespace Modules
	{
		void RegisterModules()
		{
		#define REGISTER_MODULE( ModuleClass ) PyImport_AppendInittab( ModuleClass::GetModuleName(), &ModuleClass::CreateModule );
			MODULE_REGISTER
		#undef REGISTER_MODULE
		}

		void ImportModules( PyObject* main )
		{
		#define REGISTER_MODULE( ModuleClass ) \
			PyObject* ModuleClass##module = PyImport_ImportModule( ModuleClass::GetModuleName() ); \
			PyObject_SetAttrString( main, ModuleClass::GetModuleName(), ModuleClass##module ); \
			Py_XDECREF( ModuleClass##module );

			MODULE_REGISTER

		#undef REGISTER_MODULE
		}
	}
}
#endif