#pragma once

#include "Deployment/configs/Configuration.h"
#include "Common/Resource/Exception.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"

// Always disabled for non-windows and console platforms, production configurations, and any configurations with cheats disabled
#if defined( WIN32 ) && !defined(TENCENT_REALM) && !defined(CONSOLE) && DEBUG_GUI_ENABLED
#    define PYTHON_ENABLED 1
#else
#    define PYTHON_ENABLED 0
#endif

#if PYTHON_ENABLED == 1

#include <string>
#include <stack>
#include <functional>
#include <assert.h>
#include <array>

#include "Common/Utility/Format.h"
#include "Common/Utility/Memory/Memory.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/GUI2/imgui/imgui.h"

namespace pybind11
{
	class scoped_interpreter;
	class module;
	class object;
}

namespace Python
{
	class Interpreter : public Device::GUI::GUIInstance
	{
	public:
		using MessageCallback = std::function<void( const std::wstring& message )>;

		Interpreter( const char* name, const bool force_use_python = false );
		~Interpreter();

		bool IsInitialised() { return initialised; }
		void InitialiseInterpreter( const bool force_use_python = false );
		void DestroyInterpreter();

		void Execute( const std::string& message );
		void Execute( const std::wstring& message ) { Execute( WstringToString( message ) ); }

		void Flush();

		void AddText( const std::string& text );

		virtual void OnRenderMenu( const float elapsed_time ) override;
		virtual void OnRender( const float elapsed_time ) override;
		virtual bool MsgProc( HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam ) override;

		void SetCallback( MessageCallback _callback ) { assert( !callback ); callback = _callback; }
		const MessageCallback& GetCallback() { return callback; }

		void ClearConsole();

	protected:
		int ConsoleTextEditCallback( ImGuiInputTextCallbackData* data );

		std::unique_ptr<pybind11::scoped_interpreter> interpreter;
		std::unique_ptr<pybind11::object> stderrout, handler;

		MessageCallback callback = nullptr;

		bool initialised = false;
		bool destroyed = false;

		std::vector<std::string> output;

		enum { InputMax = 512 };
		const char* name;
		std::array<char, InputMax> input_buffer;
		std::vector<std::array<char, InputMax>> history;
		int history_pos = -1;
		bool scroll_to_bottom = false;
		bool at_bottom = false;
	};

	Interpreter* GetInterpreter();

	inline void Execute( const std::string& message ) { if( GetInterpreter() ) GetInterpreter()->Execute( message ); }
	inline void Execute( const std::wstring& message ) { if( GetInterpreter() ) GetInterpreter()->Execute( message ); }

	template<class... Args, unsigned size>
	void printf( const char( &str )[size], const Args&... args )
	{
		Execute( "print('" + Utility::safe_format( str, args... ) + "')" );
	}

	template<class... Args>
	void print( const Args&... args )
	{
		const int count = sizeof...( Args );
		if( !count )
			return;

		std::string format_str = "{}";
		format_str.reserve( 3 * count );
		for( int i = 1; i < count; ++i )
			format_str += " {}";

		Execute( "print('" + Utility::format( format_str, args... ) + "')" );
	}
}
#endif