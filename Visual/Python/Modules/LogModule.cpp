#include "LogModule.h"

#if PYTHON_ENABLED
namespace Python
{
	namespace Modules
	{
		PyObject* LogModule::Debug( PyObject *self, PyObject *args )
		{
			const char* s = nullptr;
			if( !PyArg_ParseTuple( args, "s", &s ) )
			{
				Py_DECREF( args );
				return nullptr;
			}

			LOG_DEBUG( StringToWstring( s ) );

			Py_DECREF( args );
			return PyLong_FromLong( 1 );
		}

		PyObject* LogModule::Info( PyObject *self, PyObject *args )
		{
			const char* s = nullptr;
			if( !PyArg_ParseTuple( args, "s", &s ) )
			{
				Py_DECREF( args );
				return nullptr;
			}

			LOG_INFO( StringToWstring( s ) );

			Py_DECREF( args );
			return PyLong_FromLong( 1 );
		}

		PyObject* LogModule::Warn( PyObject *self, PyObject *args )
		{
			const char* s = nullptr;
			if( !PyArg_ParseTuple( args, "s", &s ) )
			{
				Py_DECREF( args );
				return nullptr;
			}

			LOG_WARN( StringToWstring( s ) );

			Py_DECREF( args );
			return PyLong_FromLong( 1 );
		}

		PyObject* LogModule::Crit( PyObject *self, PyObject *args )
		{
			const char* s = nullptr;
			if( !PyArg_ParseTuple( args, "s", &s ) )
			{
				Py_DECREF( args );
				return nullptr;
			}

			LOG_CRIT( StringToWstring( s ) );

			Py_DECREF( args );
			return PyLong_FromLong( 1 );
		}

		PyObject* LogModule::CreateModule()
		{
			static PyMethodDef methods[] =
			{
				DefineMethod( "debug", Debug ),
				DefineMethod( "info", Info ),
				DefineMethod( "warn", Warn ),
				DefineMethod( "crit", Crit ),
				NULL_METHOD
			};

			static PyModuleDef module = DefineModule( GetModuleName(), methods );
			return PyModule_Create( &module );
		}
	}
}
#endif