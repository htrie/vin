#pragma once

#include <string>
#include <functional>

namespace ImGuiEx::Console
{
	void LoadHistory( const std::string& filename );
	void SaveHistory( const std::string& filename );
	void SetCallback( std::function<void( const std::string& ) > callback );

	void Render();
	bool MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	void AddToHistory( const std::string& message );
}