
#include "Module.h"

#if PYTHON_ENABLED
namespace Python
{
	namespace Modules
	{
		PyMethodDef Module::DefineMethod( const char* name, PyCFunction function, int args /*= METH_VARARGS*/, const char* doc /*= nullptr */ )
		{
			return { name, function, args, doc };
		}

		PyModuleDef Module::DefineModule( const char* name, PyMethodDef* methods, const char* doc )
		{
			return {
				PyModuleDef_HEAD_INIT,
				name,
				doc,
				-1,
				methods,
				nullptr,
				nullptr,
				nullptr,
				nullptr
			};
		}

		const char* Module::GetModuleName()
		{
			throw std::runtime_error( "GetModuleName not implemented" );
		}

		PyObject* Module::CreateModule()
		{
			throw std::runtime_error( "CreateModule not implemented" );
		}
	}
}
#endif