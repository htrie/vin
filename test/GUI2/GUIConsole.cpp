#include "Visual/Device/Constants.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"
#if DEBUG_GUI_ENABLED
#include "Visual/GUI2/imgui/imgui_ex.h"	
#endif
#include "Visual/Python/Interpreter.h"

#include "GUIConsole.h"
#include "Common/Utility/WrapVector.h"

namespace ImGuiEx::Console
{
	enum { InputMax = 256 };
	std::function<void( const std::string& ) > callback;
	std::string input_buffer;
	Utility::WrapVector<std::string> history( 256 );
	int history_pos = -1;
	bool console_open = false;
	bool new_console = false;
	bool close_console = false;

	void SetCallback( std::function<void( const std::string& ) > func )
	{
		callback = func;
	}

	void LoadHistory( const std::string& filename )
	{
		std::ifstream file( filename );
		if( file.is_open() )
		{
			std::string line;
			while( std::getline( file, line ) )
				history.emplace_back(line.substr(0, InputMax - 1));
		}
	}

	void SaveHistory( const std::string& filename )
	{
		if( !history.empty() )
		{
			std::ofstream file( filename );
			if( file.is_open() )
			{
				unsigned total = (unsigned)history.size();
				unsigned to_write = std::min( 1000u, total );
				for( unsigned i = ( total - to_write ); i < total; ++i )
				{
					if( !history[i].empty() )
						file << history[i].data() << std::endl;
				}
			}
		}
	}

	void ToggleConsole()
	{
		if( console_open )
		{
			close_console = true;
			return;
		}

		new_console = true;
		input_buffer.clear();
		input_buffer.resize( InputMax, '\0' );
	}

	void CloseConsole()
	{
#if DEBUG_GUI_ENABLED
		ImGui::CloseCurrentPopup();
#endif
		close_console = false;
		console_open = false;
		new_console = false;
		input_buffer.clear();
		input_buffer.resize( InputMax, '\0' );
	}

#if DEBUG_GUI_ENABLED
	int ConsoleTextEditCallback( ImGuiInputTextCallbackData* data )
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
					if( ++history_pos >= (int)history.size() )
						history_pos = -1;
			}

			if( prev_history_pos != history_pos )
			{
				data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen = (int)snprintf( data->Buf, (size_t)data->BufSize, "%s", ( history_pos >= 0 ) ? history[history_pos].data() : "" );
				data->BufDirty = true;
			}
			break;
		}
		}

		return 0;
	}
#endif

	void RenderConsole()
	{
#if DEBUG_GUI_ENABLED
#	ifndef __APPLE__
		auto* vp = ImGui::GetMainViewport();
		if( !vp )
			return;

		ImGui::SetNextWindowSize( ImVec2( vp->Size.x * 0.35f, 0 ), ImGuiCond_Always );
		ImGui::SetNextWindowPos( vp->Pos + vp->Size * ImVec2( 0.5f, 0.1f ), ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowViewport( vp->ID );

		const auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar;
		auto colour = ImGuiEx::PushStyleColour( ImGuiCol_FrameBg, ImGuiEx::Alpha( 0xFF ) | ImGuiEx::GetStyleColor( ImGuiCol_FrameBg ) );
		auto lock = ImGuiEx::BeginPopupModal( "Console", nullptr, flags );
		if( !lock )
		{
			console_open = false;
			return;
		}

		console_open = true;

		auto width_lock = ImGuiEx::PushItemWidth( -1 );

		if( ImGuiEx::InputText( "##Input", input_buffer, InputMax, "", ImVec2( 0, 0 ),
			ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EnterReturnsTrue,
			ConsoleTextEditCallback, nullptr ) )
		{
			std::string text = input_buffer.data();

			history_pos = -1;
			for( int i = (int)history.size() - 1; i >= 0; i-- )
			{
				if( history[i].data() == text )
				{
					history.erase( history._begin() + i );
					break;
				}
			}

			history.push_back( input_buffer );

			if( !text.empty() )
			{
			#if PYTHON_ENABLED == 1
				if( text[0] != L'/' )
					Python::Execute( text );
				else
			#endif
					callback( text );
			}

			ImGui::CloseCurrentPopup();
			ImGui::SetWindowFocus( nullptr );
			close_console = false;
		}

		if( new_console )
			ImGui::SetKeyboardFocusHere( -1 );
		else if( ImGui::IsItemDeactivated() )
			ImGui::CloseCurrentPopup();
#	endif
#endif
	}

	void Render()
	{
#if DEBUG_GUI_ENABLED
		if( new_console )
			ImGui::OpenPopup( "Console" );

		if( close_console )
			CloseConsole();
		else
			RenderConsole();

		new_console = false;
#	endif
	}

	bool MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
		if( uMsg == WM_KEYDOWN )
		{
			if( wParam == VK_RETURN )
			{
				ToggleConsole();
				return true;
			}
		}

		return false;
	}

	void AddToHistory( const std::string& message )
	{
		history.emplace_back( message );
	}
}
