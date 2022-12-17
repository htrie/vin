#include "Interpreter.h"

#if PYTHON_ENABLED

#pragma comment(lib, "version.lib" )
#pragma comment(lib, "python37.lib" )

#include <string>
#include <iostream>
#include <sstream>
#include <iosfwd>
#include <processenv.h>

#include "Common/Resource/Exception.h"
#include "Common/Utility/ArrayOperations.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/File/FileSystem.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_internal.h"
#include "Visual/Utility/IMGUI/ToolGUI.h"

#    ifdef _DEBUG
#        undef _DEBUG
#        include "pybind11/pybind11_inc.h"
#        define _DEBUG
#    else
#        include "pybind11/pybind11_inc.h"
#    endif

#include "ModuleRegistration.h"

namespace Python
{
	Interpreter* global_interpreter = nullptr;

	Interpreter* GetInterpreter()
	{
		return global_interpreter;
	}

	Interpreter::Interpreter( const char* name, const bool force_use_python )
		: Device::GUI::GUIInstance( true )
		, name( name )
	{
		assert( !global_interpreter );
		global_interpreter = this;

		InitialiseInterpreter( force_use_python );
		input_buffer.fill( '\0' );
	}

	Interpreter::~Interpreter()
	{
		DestroyInterpreter();
	}

	void Interpreter::DestroyInterpreter()
	{
		if( destroyed )
			return;

		stderrout = nullptr;

		if( handler )
		{
			handler->attr( "destroy" ).operator()();
			handler = nullptr;
		}

		interpreter = nullptr;
		destroyed = true;
	}

	void Interpreter::InitialiseInterpreter( const bool force_use_python )
	{
		auto cmdline = std::wstring( GetCommandLine() );
		if( !force_use_python && cmdline.find( L"--python" ) == -1 )
			return;
		if( !FileSystem::FileExists( L"Tools/common/internal/handler.py" ) )
			throw std::runtime_error( "Python interpreter failed to initialise: Missing 'Tools/common/internal/handler.py'" );

		try
		{
			// Construct path
			std::wstring python_path = L".;";
			python_path += L"Tools/common/internal;";
			python_path += Utility::safe_format( L"Tools/{};", StringToWstring( name ) );
			python_path += L"Tools/common;";
			python_path += L"Tools/python;";
		#ifdef _WIN64
			python_path += L"Tools/python/x64;";
		#else
			python_path += L"Tools/python/Win32;";
		#endif
			Py_SetPath( python_path.c_str() );

			// Create interpreter
			interpreter = std::make_unique<py::scoped_interpreter>();
			assert( interpreter );

			// Load exception handling module
			stderrout = std::make_unique<py::object>( py::module::import( "exception" ).attr( "new" ).operator()() );
			assert( stderrout.get() );

			// Search for startup modules
			std::vector<py::object> startup;
			if( FileSystem::FileExists( L"Tools/common/startup.py" ) )
			{
				Py_SetPath( L"Tools/common;" );
				if( auto m = py::module::import( "startup" ) )
					startup.push_back( std::move( m ) );
			}

			if( FileSystem::FileExists( Utility::safe_format( L"Tools/{}/startup.py", StringToWstring( name ) ) ) )
			{

				Py_SetPath( Utility::safe_format( L"Tools/{};", StringToWstring( name ) ).c_str() );
				if( auto m = py::module::import( "startup" ) )
					startup.push_back( std::move( m ) );
			}

			// Restore path
			Py_SetPath( python_path.c_str() );

			// Initialise startup modules
			for( auto& m : startup )
			{
#pragma warning( suppress : 4996 )
				if( auto init = m.attr( "init" ) )
					auto result = init.operator()();
			}

			// Post initialise startup modules
			for( auto& m : startup )
			{
#pragma warning( suppress : 4996 )
				if( auto init = m.attr( "post_init" ) )
					auto result = init.operator()();
			}

			// Load any user modules
			handler = std::make_unique<py::object>( py::module::import( "handler" ).attr( "new" ).operator()( name ) );
			assert( handler.get() );
		}
		catch( const py::error_already_set & e )
		{
			// We do this hopefully so that the destructor for <e> gets called before we actually handle the exception in main.cpp
			throw std::runtime_error( e.what() );
		}
	}

	void Interpreter::Execute( const std::string& message )
	{
		if( !interpreter )
			return;

		AddText( message );

		try
		{
			py::exec( message );
		}
		catch( const std::exception& e )
		{
			Resource::DisplayException( e );
		}
	}

	void Interpreter::Flush()
	{
		if( stderrout.get() )
		{
			py::object flush_call = stderrout->attr( "flush" );
			if( py::object ret = flush_call.operator()() )
			{
				std::string str = py::str( ret );
				AddText( TrimString( str ) );
			}
		}
	}

	void Interpreter::AddText( const std::string &text )
	{
		if( !text.empty() )
		{
			output.push_back( text );

			if( output.size() > 1000 )
				output.erase( output.begin() );
		}
	}

	void Interpreter::OnRenderMenu( const float elapsed_time )
	{
	}

	void Interpreter::OnRender( const float elapsed_time )
	{
		if( !open )
			return;

		const auto screen = ImGui::GetIO().DisplaySize;

		ImGui::Begin( "Python", &open );
		{
			const auto text_width = ImGui::GetWindowSize().x - 20;
			const auto text_height = ImGui::GetFrameHeightWithSpacing() - ImGui::GetFontSize() * 4;

			ImGui::PushItemWidth( text_width );
 
			if( ImGui::BeginChild( "Text Frame"
				, ImVec2( text_width, text_height ), true
				, ImGuiWindowFlags_AlwaysUseWindowPadding
				| ImGuiWindowFlags_HorizontalScrollbar
				| ImGuiWindowFlags_NoFocusOnAppearing
			) )
			{
				for( const auto& line : output )
					ImGui::Text( line.c_str() );

				if( !interpreter )
				{
					ImGui::Text( "Python disabled, add --python to command line to enable" );
				}

				ImGui::Text( "" );

				if( scroll_to_bottom )
				{
					ImGui::SetScrollHereY( 1.0f );
					scroll_to_bottom = false;
					at_bottom = true;
				}
			}
			ImGui::EndChild();

			at_bottom = ImGui::GetScrollY() == ImGui::GetScrollMaxY();

			auto callback = []( ImGuiInputTextCallbackData* data ) -> int
			{
				auto* app = ( Interpreter* )data->UserData;
				return app->ConsoleTextEditCallback( data );
			};

			if( ImGui::InputText( "##ConsoleInput"
				, input_buffer.data()
				, InputMax
				, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory
				, callback, this ) )
			{
				std::string text = input_buffer.data();

				history_pos = -1;
				for( int i = (int)history.size() - 1; i >= 0; i-- )
				{
					if( history[i].data() == text )
					{
						history.erase( history.begin() + i );
						break;
					}
				}

				history.push_back( input_buffer );
				input_buffer.fill( '\0' );
				Execute( text );

				ImGui::SetKeyboardFocusHere( -1 );
			}

			ImGui::PopItemWidth();
		}
		ImGui::End();
	}

	bool Interpreter::MsgProc( HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam )
	{
		if (!open)
			return false;

		bool has_keyboard_focus = ImGui::GetIO().WantTextInput;
		switch (umsg)
		{
		case WM_KEYDOWN:
		case WM_KEYUP:
			return has_keyboard_focus;
		}
		return false;
	}

	void Interpreter::ClearConsole()
	{
		output.clear();
		scroll_to_bottom = true;
	}

	int Interpreter::ConsoleTextEditCallback( ImGuiInputTextCallbackData* data )
	{
		switch( data->EventFlag )
		{
		case ImGuiInputTextFlags_CallbackHistory:
		{
			const int prev_history_pos = history_pos;
			if( data->EventKey == ImGuiKey_UpArrow )
			{
				if( history_pos == -1 )
					history_pos = (int)history.size() - 1;
				else if( history_pos > 0 )
					history_pos--;
			}
			else if( data->EventKey == ImGuiKey_DownArrow )
			{
				if( history_pos != -1 )
					if( ++history_pos >= ( int )history.size() )
						history_pos = -1;
			}

			if( prev_history_pos != history_pos )
			{
#pragma warning( suppress: 4996 )
				data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = ( int )snprintf( data->Buf, ( size_t )data->BufSize, "%s", ( history_pos >= 0 ) ? history[history_pos].data() : "" );
				data->BufDirty = true;
			}
			break;
		}
		}

		return 0;
	}
}

#include "Visual/Python/pybind11/pybind11_undef.h"
#endif //PYTHON_ENABLED