#include "AppModule.h"

#if PYTHON_ENABLED

#include "Visual/Python/pybind11/pybind11_inc.h"
#include "Visual/Python/PythonGUI.h"
#include "Visual/Utility/CommonDialogs.h"

#include "Common/Utility/Logger/Logger.h"

namespace Python
{
	namespace Modules
	{
		class Py_PythonGUI : public PythonGUI
		{
		public:
			virtual void initialise()
				implements( void, PythonGUI, initialise );
			virtual void destroy()
				implements( void, PythonGUI, destroy );
			virtual void on_render( const float elapsed_time )
				implements( void, PythonGUI, on_render, elapsed_time );
			virtual void on_render_menu( const float elapsed_time )
				implements( void, PythonGUI, on_render_menu, elapsed_time );
		};

		void Debug( const char* message )
		{
			LOG_DEBUG( StringToWstring( message ) );
		}

		void Info( const char* message )
		{
			LOG_INFO( StringToWstring( message ) );
		}

		void Warn( const char* message )
		{
			LOG_WARN( StringToWstring( message ) );
		}

		void Crit( const char* message )
		{
			LOG_CRIT( StringToWstring( message ) );
		}

		void DisplayException( const std::exception& exception )
		{
			Resource::DisplayException( exception );
		}

		void Flush()
		{
			if( auto* interpreter = Python::GetInterpreter() )
				interpreter->Flush();
		}

		void Send( const char* message )
		{
			if( auto* interpreter = Python::GetInterpreter() )
			{
				if( auto & callback = interpreter->GetCallback() )
					callback( StringToWstring( message ) );
			}
		}

		void Clear()
		{
			if( auto* interpreter = Python::GetInterpreter() )
				interpreter->ClearConsole();
		}

		std::string BrowseFile( const char* filter = nullptr )
		{
			// This is specifically to fix Python being unable to pass strings with multiple null terminators to us
			// Now scripts can use multiple null terminators when passing their strings to C++
			std::wstring filter_str = StringToWstring( filter );
			for( size_t i = 0; filter_str[i]; ++i )
				if( filter_str[i] == L'|' )
					filter_str[i] = L'\0';

			return WstringToString( Utility::GetFilenameBox( filter_str.c_str() ) );
		}

		std::string BrowseSaveFile( const char* filter = nullptr, const char* default_filename = nullptr )
		{
			// This is specifically to fix Python being unable to pass strings with multiple null terminators to us
			// Now scripts can use multiple null terminators when passing their strings to C++
			std::wstring filter_str = StringToWstring( filter );
			for( size_t i = 0; filter_str[i]; ++i )
				if( filter_str[i] == L'|' )
					filter_str[i] = L'\0';

			std::wstring filename = StringToWstring( default_filename );
			if( !Utility::SaveFilenameBox( filter_str.c_str(), filename ) )
				filename.clear();
			return WstringToString( filename );
		}

		std::string BrowseDirectory( const char* title = nullptr )
		{
			return WstringToString( Utility::GetDirectoryBox( StringToWstring( title ).c_str() ) );
		}

		void AppModule::Initialise( pybind11::module& mod )
		{
			class2( PythonGUI, Py_PythonGUI )
				.method( PythonGUI, initialise )
				.method( PythonGUI, destroy )
				.method( PythonGUI, on_render )
				.method( PythonGUI, on_render_menu );
			mod
				.func( DisplayException )
				.func( Debug )
				.func( Info )
				.func( Warn )
				.func( Crit )
				.func( Send )
				.func( Clear )
				.func( BrowseFile )
				.func( BrowseSaveFile, py::arg( "filter" ) = nullptr, py::arg("default_filename") = nullptr )
				.func( BrowseDirectory )
				.func( Flush );
		}
	}
}
#include "Visual/Python/pybind11/pybind11_undef.h"
#endif //PYTHON_ENABLED