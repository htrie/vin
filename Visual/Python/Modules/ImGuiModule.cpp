#include "ImGuiModule.h"

#if PYTHON_ENABLED
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/Python/pybind11/pybind11_inc.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "Visual/GUI2/imgui/imgui_internal.h"

#include <sstream>
#include <stdarg.h>
#include <tuple>

using std::function;
using std::string;

#define DOC( ... )

namespace Python
{
	namespace Modules
	{
		/*
			brief = "Starts a new ImGui window."
			return = "Returns 'True' if you should render the window, do not otherwise."

			[parameters]
				name = "The name of the window, and the unique hash used to identify the window"
				on_close = "Event signaling the window should close. When specified, a 'X' button is added to the window allowing the user to close it, triggering this event."
				flags = "Modify the window behaviour by piping together flag values with the '|' operator."
		*/
		GENERATE bool Begin( const char* name, function<void()> on_close = nullptr,
							 const ImGuiWindowFlags flags = 0 )
		{
			if( on_close == nullptr )
				return ImGui::Begin( name, nullptr, flags );

			bool open = true;
			bool result = ImGui::Begin( name, &open, flags );
			if( !open )
				on_close();
			return result;

		}
		/*
			brief = "Render some unformatted text to the screen"

			[parameters]
				text = "The text to be rendered to the screen"
		*/
		GENERATE void Text( const char* text )
		{
			ImGui::TextUnformatted( text );
		}
		/*
			brief = "Render some coloured text to the screen."
			
			[parameters]
				col = "Colour of the text. The format of the colour should be RGBA, in the range of 0 .. 1"
				text = "The text to be rendered to the screen."
		*/
		GENERATE void TextColored( const ImVec4& col, const char* text )
		{
			ImGui::TextColored( col, text );
		}
		GENERATE void TextDisabled( const char* text )
		{
			ImGui::TextDisabled( text );
		}
		GENERATE void TextWrapped( const char* text )
		{
			ImGui::TextWrapped( text );
		}
		GENERATE void LabelText( const char* text )
		{
			ImGui::TextWrapped( text );
		}
		GENERATE void BulletText( const char* text )
		{
			ImGui::BulletText( text );
		}
		/*
			brief = "Render the 'Demo' window, containing a large variety of ImGui functionality for you to browse and learn."
			
			[parameters]
				on_close = "Callback for if you would like the user to be able to close this window."
		*/
		GENERATE void ShowDemoWindow( function<void()> on_close = nullptr )
		{
			std::vector<int> hi;
			bool open = true;
			ImGui::ShowDemoWindow( &open );
			if( !open && on_close )
				on_close();
		}
		/*
			brief = "Render the 'About' window, containing information about the current ImGui build."
			
			[parameters]
				on_close = "Callback for if you would like the user to be able to close this window."
		*/
		GENERATE void ShowAboutWindow( function<void()> on_close = nullptr )
		{
			bool open = true;
			ImGui::ShowAboutWindow( &open );
			if( !open && on_close )
				on_close();
		}
		/*
			brief = "Render the 'Metrics' window, containing a mass of debugging information about the current state of the application. Use this window to understand why your application is behaving strangely."
			
			[parameters]
				on_close = "Callback for if you would like the user to be able to close this window."
		*/
		GENERATE void ShowMetricsWindow( function<void()> on_close = nullptr )
		{
			bool open = true;
			ImGui::ShowMetricsWindow( &open );
			if( !open && on_close )
				on_close();
		}
		/*
			brief = "Render a checkbox to the screen."
			return = "Returns true when the checkbox was clicked by the user. Implies that the check value will change from true->false or false->true."
			
			[parameters]
				label = "The name of the checkbox, and the unique hash. Use the '##' operator to append additional information hidden from the user."
				checked = "Whether or not the checkbox should be rendered as 'checked' or 'unchecked'"
				on_check = "Callback function, triggered when the user clicks on the checkbox."
		*/
		GENERATE bool Checkbox( const char* label, bool checked, function<void( bool )> on_check = false )
		{
			if( ImGui::Checkbox( label, &checked ) )
			{
				if( on_check )
					on_check( checked );
				return true;
			}

			return false;
		}
		GENERATE bool RadioButton( const char* label, const bool selected )
		{
			return ImGui::RadioButton( label, selected );
		}
		GENERATE bool BeginChild( const char* str_id, const ImVec2& size = ImVec2( 0, 0 ),
								  const bool border = false, ImGuiWindowFlags flags = 0 )
		{
			return ImGui::BeginChild( str_id, size, border, flags );
		}
		GENERATE py::object GetIO()
		{
			return py::cast( &ImGui::GetIO() );
		}
		/// DC
		//GENERATE py::object GetCurrentContext()
		//{
		//	return py::cast( ImGui::GetCurrentContext() );
		//}
		GENERATE py::object GetMainViewport()
		{
			return py::cast( ImGui::GetMainViewport() );
		}
		GENERATE py::object GetMonitor( const int idx = 0 )
		{
			return py::cast( ImGui::GetPlatformIO().Monitors[idx] );
		}
		GENERATE py::object GetStyle()
		{
			return py::cast( &ImGui::GetStyle() );
		}
		GENERATE void PushStyleColor( const ImGuiCol idx, const ImU32 col )
		{
			ImGui::PushStyleColor( idx, col );
		}
		GENERATE py::object GetStyleColorsLight()
		{
			ImGuiStyle dst;
			ImGui::StyleColorsLight( &dst );
			return py::cast( dst );
		}
		GENERATE py::object GetStyleColorsDark()
		{
			ImGuiStyle dst;
			ImGui::StyleColorsDark( &dst );
			return py::cast( dst );
		}
		GENERATE py::object GetStyleColorsClassic()
		{
			ImGuiStyle dst;
			ImGui::StyleColorsClassic( &dst );
			return py::cast( dst );
		}
		GENERATE void SetStyleColorsLight()
		{
			ImGui::StyleColorsLight();
		}
		GENERATE void SetStyleColorsDark()
		{
			ImGui::StyleColorsDark();
		}
		GENERATE void SetStyleColorsClassic()
		{
			ImGui::StyleColorsClassic();
		}
		GENERATE void PushStyleColor_2( const ImGuiCol idx, const ImVec4 col )
		{
			ImGui::PushStyleColor( idx, col );
		}
		GENERATE void PushStyleVar( const ImGuiStyleVar idx, const float val )
		{
			ImGui::PushStyleVar( idx, val );
		}
		GENERATE void PushStyleVar_2( const ImGuiStyleVar idx, const ImVec2& val )
		{
			ImGui::PushStyleVar( idx, val );
		}
		GENERATE ImU32 GetColorU32( const ImGuiCol idx, const float alpha_mul = 1.0f )
		{
			return ImGui::GetColorU32( idx, alpha_mul );
		}
		GENERATE ImU32 GetColorU32_2( const ImVec4& col )
		{
			return ImGui::GetColorU32( col );
		}
		GENERATE ImU32 GetColorU32_3( const ImU32 col )
		{
			return ImGui::GetColorU32( col );
		}
		GENERATE void PushID( const char* id )
		{
			ImGui::PushID( id );
		}
		GENERATE void PushID_2( const int id )
		{
			ImGui::PushID( id );
		}
		GENERATE ImGuiID GetID( const char* id )
		{
			return ImGui::GetID( id );
		}
		GENERATE void SetNextWindowSizeConstraints( const ImVec2& size_min, const ImVec2& size_max )
		{
			ImGui::SetNextWindowSizeConstraints( size_min, size_max );
		}
		GENERATE void SetWindowFocus()
		{
			ImGui::SetWindowFocus();
		}
		GENERATE void SetWindowFocus_2( const char* name )
		{
			ImGui::SetWindowFocus( name );
		}
		GENERATE void SetWindowPos( const ImVec2& pos, const ImGuiCond cond = 0 )
		{
			ImGui::SetWindowPos( pos, cond );
		}
		GENERATE void SetWindowPos_2( const char* name, const ImVec2& pos, const ImGuiCond cond = 0 )
		{
			ImGui::SetWindowPos( name, pos, cond );
		}
		GENERATE void SetWindowSize( const ImVec2& size, const ImGuiCond cond = 0 )
		{
			ImGui::SetWindowSize( size, cond );
		}
		GENERATE void SetWindowSize_2( const char* name, const ImVec2& size, const ImGuiCond cond = 0 )
		{
			ImGui::SetWindowSize( name, size, cond );
		}
		GENERATE void SetWindowCollapsed( const bool collapsed, const ImGuiCond cond = 0 )
		{
			ImGui::SetWindowCollapsed( collapsed, cond );
		}
		GENERATE void SetWindowCollapsed_2( const char* name, const bool collapsed,
											const ImGuiCond cond = 0 )
		{
			ImGui::SetWindowCollapsed( name, collapsed, cond );
		}
		GENERATE bool TreeNode( const char* name )
		{
			return ImGui::TreeNode( name );
		}
		GENERATE bool TreeNode_2( const char* name, const char* format )
		{
			return ImGui::TreeNode( name, format );
		}
		GENERATE bool TreeNode_3( const void* ptr, const char* format )
		{
			return ImGui::TreeNode( ptr, format );
		}
		GENERATE bool TreeNode_4( const ImGuiID id, const char* format )
		{
			return ImGui::TreeNode( (void*)(intptr_t)id, format );
		}
		GENERATE void TreePush( const char* name )
		{
			ImGui::TreePush( name );
		}
		/*
			brief = "Start a new collapsing section. Use CollapsingHeader over TreeNode when you wish to make the column header appear very bold to the user."
			return = "Returns true when you should render information under this header."
			
			[parameters]
				label = "The name of the header, and the unique hash that describes the widget. Use the '##' operator to append additional hash information you wish to hide from the user."
				flags = "Modify the behaviour of the CollapsingHeader."
		*/
		GENERATE bool CollapsingHeader( const char* label, const ImGuiTreeNodeFlags flags = 0 )
		{
			return ImGui::CollapsingHeader( label, flags );
		}
		/*
			brief = "Start a new collapsing section. This overload of CollapsingHeader comes with an 'x' button for the user to close the section. Use this to create dynamic lists that the user can add/delete to at will."
			return = "Returns true when you should render information under this header."
			
			[parameters]
				label = "The name of the header, and the unique hash that describes the widget. Use the '##' operator to append additional hash information you wish to hide from the user."
				closed_callback = "Callback function. This event will be triggered when the user tries to close this header."
				flags = "Modify the behaviour of the CollapsingHeader."
		*/
		GENERATE bool CollapsingHeader_2( const char* label, function<void()> closed_callback,
										  const ImGuiTreeNodeFlags flags = 0 )
		{
			bool open = true;
			const bool result = ImGui::CollapsingHeader( label, &open, flags );

			if( !open && closed_callback )
				closed_callback();

			return result;
		}
		GENERATE bool Selectable( const char* label, const bool selected = false,
								  const ImGuiSelectableFlags flags = 0,
								  const ImVec2& size_arg = ImVec2( 0, 0 ) )
		{
			return ImGui::Selectable( label, selected, flags, size_arg );
		}
		GENERATE bool MenuItem( const char* label, const char* shortcut = nullptr, bool selected = false, function<void( const bool )> select_changed = nullptr, const bool enabled = true )
		{
			const bool click = ImGui::MenuItem( label, shortcut, &selected, enabled );

			if( select_changed && click )
				select_changed( selected );

			return click;
		}
		GENERATE bool ListBoxHeader( const char* label, const ImVec2& size_arg = ImVec2( 0, 0 ) )
		{
			return ImGui::ListBoxHeader( label, size_arg );
		}
		static int InputTextCallback( ImGuiInputTextCallbackData* data )
		{
			if( data->EventFlag == ImGuiInputTextFlags_CallbackResize )
			{
				std::string* str = ( std::string* )data->UserData;
				IM_ASSERT( data->Buf == str->c_str() );
				str->resize( data->BufTextLen );
				data->Buf = ( char* )str->c_str();
			}
			return 0;
		}
		GENERATE bool InputText( const char* label, const char* text, const ImGuiInputTextFlags flags = 0,
								 function<void( const string& )> text_changed = nullptr )
		{
			string buffer = text;
			if( ImGui::InputText( label,
								  (char*)buffer.data(),
								  buffer.capacity() + 1,
								  flags | ImGuiInputTextFlags_CallbackResize,
								  InputTextCallback,
								  &buffer ) )
			{
				if( text_changed )
					text_changed( buffer.c_str() );
				return true;
			}
			return false;
		}
		GENERATE bool InputTextMultiline( const char* label, const char* text,
										  const ImVec2& size = ImVec2( 0, 0 ),
										  const ImGuiInputTextFlags flags = 0,
										  function<void( const string& )> text_changed = nullptr )
		{
			string buffer = text;
			if( ImGui::InputTextMultiline( label,
										   (char*)buffer.data(),
										   buffer.capacity() + 1,
										   size,
										   flags | ImGuiInputTextFlags_CallbackResize,
										   InputTextCallback,
										   &buffer ) )
			{
				if( text_changed )
					text_changed( buffer.c_str() );
				return true;
			}
			return false;
		}
		GENERATE bool InputTextWithHint( const char* label, const char* hint, const char* text,
										 const ImGuiInputTextFlags flags = 0,
										 function<void( const string& )> text_changed = nullptr )
		{
			string buffer = text;
			if( ImGui::InputTextWithHint( label,
										  hint,
										  (char*)buffer.data(),
										  buffer.capacity() + 1,
										  flags | ImGuiInputTextFlags_CallbackResize,
										  InputTextCallback,
										  &buffer ) )
			{
				if( text_changed )
					text_changed( buffer.c_str() );
				return true;
			}
			return false;
		}
		GENERATE bool InputFloat( const char* label, float v, float step = 0.0f, float step_fast = 0.0f,
								  const char* format = "%.3f", const ImGuiInputTextFlags flags = 0,
								  function<void( float )> value_changed = nullptr )
		{
			if( ImGui::InputFloat( label, &v, step, step_fast, format, flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputFloat2( const char* label, simd::vector2& v, const char* format = "%.3f",
								   const ImGuiInputTextFlags flags = 0,
								   function<void( simd::vector2 )> value_changed = nullptr )
		{
			if( ImGui::InputFloat2( label, v.as_array(), format, flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputFloat3( const char* label, simd::vector3& v, const char* format = "%.3f",
								   const ImGuiInputTextFlags flags = 0,
								   function<void( simd::vector3 )> value_changed = nullptr )
		{
			if( ImGui::InputFloat3( label, v.as_array(), format, flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputFloat4( const char* label, simd::vector4& v, const char* format = "%.3f",
								   const ImGuiInputTextFlags flags = 0,
								   function<void( simd::vector4 )> value_changed = nullptr )
		{
			if( ImGui::InputFloat4( label, v.v, format, flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputInt( const char* label, int v, const int step = 1, const int step_fast = 100,
								const ImGuiInputTextFlags flags = 0,
								function<void( int )> value_changed = nullptr )
		{
			if( ImGui::InputInt( label, &v, step, step_fast, flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputInt2( const char* label, simd::vector2_int& v, const ImGuiInputTextFlags flags = 0,
								 function<void( simd::vector2_int )> value_changed = nullptr )
		{
			if( ImGui::InputInt2( label, v.as_array(), flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputInt3( const char* label, simd::vector3_int& v, const ImGuiInputTextFlags flags = 0,
								 function<void( simd::vector3_int )> value_changed = nullptr )
		{
			if( ImGui::InputInt3( label, v.as_array(), flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool InputInt4( const char* label, simd::vector4_int& v, const ImGuiInputTextFlags flags = 0,
								 function<void( simd::vector4_int )> value_changed = nullptr )
		{
			if( ImGui::InputInt4( label, v.as_array(), flags ) )
			{
				if( value_changed )
					value_changed( v );
				return true;
			}
			return false;
		}
		GENERATE bool ColorPicker3( const char* label, ImVec4 col, const ImGuiColorEditFlags flags = 0,
									function<void( ImVec4 )> value_changed = nullptr )
		{
			if( ImGui::ColorPicker3( label, &col.x, flags ) )
			{
				if( value_changed )
					value_changed( col );
				return true;
			}
			return false;
		}
		GENERATE bool ColorPicker4( const char* label, ImVec4 col, const ImGuiColorEditFlags flags = 0,
									function<void( ImVec4 )> value_changed = nullptr )
		{
			if( ImGui::ColorPicker4( label, &col.x, flags ) )
			{
				if( value_changed )
					value_changed( col );
				return true;
			}
			return false;
		}
		GENERATE bool ColorEdit3( const char* label, ImVec4 col, const ImGuiColorEditFlags flags = 0,
								  function<void( ImVec4 )> value_changed = nullptr )
		{
			if( ImGui::ColorEdit3( label, &col.x, flags ) )
			{
				if( value_changed )
					value_changed( col );
				return true;
			}
			return false;
		}
		GENERATE bool ColorEdit4( const char* label, ImVec4 col, const ImGuiColorEditFlags flags = 0,
								  function<void( ImVec4 )> value_changed = nullptr )
		{
			if( ImGui::ColorEdit4( label, &col.x, flags ) )
			{
				if( value_changed )
					value_changed( col );
				return true;
			}
			return false;
		}
		GENERATE bool IsPopupOpen( const char* str_id )
		{
			return ImGui::IsPopupOpen( str_id );
		}
		GENERATE const char* GetVersion()
		{
			return ImGui::GetVersion();
		}
		GENERATE int GetVersionNumber()
		{
			return IMGUI_VERSION_NUM;
		}
		/*
			brief = "Call when you are ready to finish rendering your ImGui window. Call this even if Begin(...) returned false!"
		*/
		GENERATE void End()
		{
			ImGui::End();
		}
		/*
			brief = "Call when you are ready to finish rendering your ImGui child window. Call this even if BeginChild(...) returned false!"
		*/
		GENERATE void EndChild()
		{
			ImGui::EndChild();
		}
		GENERATE bool IsWindowAppearing()
		{
			return ImGui::IsWindowAppearing();
		}
		GENERATE bool IsWindowCollapsed()
		{
			return ImGui::IsWindowCollapsed();
		}
		GENERATE bool IsWindowFocused()
		{
			return ImGui::IsWindowFocused();
		}
		GENERATE bool IsWindowHovered()
		{
			return ImGui::IsWindowHovered();
		}
		GENERATE ImDrawList* GetWindowDrawList()
		{
			return ImGui::GetWindowDrawList();
		}
		GENERATE float GetWindowDpiScale()
		{
			return ImGui::GetWindowDpiScale();
		}
		GENERATE ImGuiViewport* GetWindowViewport()
		{
			return ImGui::GetWindowViewport();
		}
		GENERATE ImVec2 GetWindowPos()
		{
			return ImGui::GetWindowPos();
		}
		GENERATE ImVec2 GetWindowSize()
		{
			return ImGui::GetWindowSize();
		}
		GENERATE float GetWindowWidth()
		{
			return ImGui::GetWindowWidth();
		}
		GENERATE float GetWindowHeight()
		{
			return ImGui::GetWindowHeight();
		}
		GENERATE ImVec2 GetContentRegionMax()
		{
			return ImGui::GetContentRegionMax();
		}
		GENERATE ImVec2 GetContentRegionAvail()
		{
			return ImGui::GetContentRegionAvail();
		}
		GENERATE float GetContentRegionAvailWidth()
		{
			return ImGui::GetContentRegionAvail().x;
		}
		GENERATE ImVec2 GetWindowContentRegionMin()
		{
			return ImGui::GetWindowContentRegionMin();
		}
		GENERATE ImVec2 GetWindowContentRegionMax()
		{
			return ImGui::GetWindowContentRegionMax();
		}
		GENERATE float GetWindowContentRegionWidth()
		{
			return ImGui::GetWindowContentRegionWidth();
		}
		GENERATE void SetNextWindowPos( const ImVec2& pos, const ImGuiCond cond = 0,
										const ImVec2& pivot = ImVec2( 0, 0 ) )
		{
			ImGui::SetNextWindowPos( pos, cond, pivot );
		}
		GENERATE void SetNextWindowSize( const ImVec2& size, const ImGuiCond cond = 0 )
		{
			ImGui::SetNextWindowSize( size, cond );
		}
		GENERATE void SetNextWindowContentSize( const ImVec2& size )
		{
			ImGui::SetNextWindowContentSize( size );
		}
		GENERATE void SetNextWindowCollapsed( const bool collapsed, const ImGuiCond cond = 0 )
		{
			ImGui::SetNextWindowCollapsed( cond );
		}
		GENERATE void SetNextWindowFocus()
		{
			ImGui::SetNextWindowFocus();
		}
		GENERATE void SetNextWindowBgAlpha( const float alpha )
		{
			ImGui::SetNextWindowBgAlpha( alpha );
		}
		GENERATE void SetNextWindowViewport( const ImGuiID id )
		{
			ImGui::SetNextWindowViewport( id );
		}
		GENERATE void SetWindowFontScale( const float scale )
		{
			ImGui::SetWindowFontScale( scale );
		}
		GENERATE float GetScrollX()
		{
			return ImGui::GetScrollX();
		}
		GENERATE float GetScrollY()
		{
			return ImGui::GetScrollY();
		}
		GENERATE float GetScrollMaxX()
		{
			return ImGui::GetScrollMaxX();
		}
		GENERATE float GetScrollMaxY()
		{
			return ImGui::GetScrollMaxY();
		}
		GENERATE void SetScrollX( const float scroll_x )
		{
			ImGui::SetScrollX( scroll_x );
		}
		GENERATE void SetScrollY( const float scroll_y )
		{
			ImGui::SetScrollY( scroll_y );
		}
		GENERATE void SetScrollHereY( const float centre_y_ratio = 0.5f )
		{
			ImGui::SetScrollHereY( centre_y_ratio );
		}
		GENERATE void SetScrollFromPosY( const float local_y, const float centre_y_ratio = 0.5f )
		{
			ImGui::SetScrollFromPosY( local_y, centre_y_ratio );
		}
		GENERATE void PushFont( ImFont* font )
		{
			ImGui::PushFont( font );
		}
		GENERATE void PopFont()
		{
			ImGui::PopFont();
		}
		GENERATE void PopStyleColor( const int count = 1 )
		{
			ImGui::PopStyleColor( count );
		}
		GENERATE void PopStyleVar( const int count = 1 )
		{
			ImGui::PopStyleVar( count );
		}
		GENERATE const ImVec4& GetStyleColorVec4( const ImGuiCond cond )
		{
			return ImGui::GetStyleColorVec4( cond );
		}
		GENERATE float GetFontSize()
		{
			return ImGui::GetFontSize();
		}
		GENERATE ImVec2 GetFontTexUvWhitePixel()
		{
			return ImGui::GetFontTexUvWhitePixel();
		}
		GENERATE void PushItemWidth( const float width )
		{
			ImGui::PushItemWidth( width );
		}
		GENERATE void PopItemWidth()
		{
			ImGui::PopItemWidth();
		}
		GENERATE void SetNextItemWidth( const float width )
		{
			ImGui::SetNextItemWidth( width );
		}
		GENERATE float CalcItemWidth()
		{
			return ImGui::CalcItemWidth();
		}
		GENERATE void PushTextWrapPos( const float wrap_pos_x = 0.0f )
		{
			ImGui::PushTextWrapPos( wrap_pos_x );
		}
		GENERATE void PopTextWrapPos()
		{
			ImGui::PopTextWrapPos();
		}
		GENERATE void PushAllowKeyboardFocus( const bool allow_keyboard_focus )
		{
			ImGui::PushAllowKeyboardFocus( allow_keyboard_focus );
		}
		GENERATE void PopAllowKeyboardFocus()
		{
			ImGui::PopAllowKeyboardFocus();
		}
		GENERATE void PushButtonRepeat( const bool repeat )
		{
			ImGui::PushButtonRepeat( repeat );
		}
		GENERATE void PopButtonRepeat()
		{
			ImGui::PopButtonRepeat();
		}
		GENERATE void Separator()
		{
			ImGui::Separator();
		}
		GENERATE void SameLine( const float local_pos_x = 0.0f, const float spacing_w = -1.0f )
		{
			ImGui::SameLine( local_pos_x, spacing_w );
		}
		GENERATE void NewLine()
		{
			ImGui::NewLine();
		}
		GENERATE void Spacing()
		{
			ImGui::Spacing();
		}
		GENERATE void Dummy( const ImVec2& size )
		{
			ImGui::Dummy( size );
		}
		GENERATE void Indent( const float indent_w = 0.0f )
		{
			ImGui::Indent( indent_w );
		}
		GENERATE void Unindent( const float indent_w = 0.0f )
		{
			ImGui::Unindent( indent_w );
		}
		GENERATE void BeginGroup()
		{
			ImGui::BeginGroup();
		}
		GENERATE void EndGroup()
		{
			ImGui::EndGroup();
		}
		GENERATE ImVec2 GetCursorPos()
		{
			return ImGui::GetCursorPos();
		}
		GENERATE float GetCursorPosX()
		{
			return ImGui::GetCursorPosX();
		}
		GENERATE float GetCursorPosY()
		{
			return ImGui::GetCursorPosY();
		}
		GENERATE void SetCursorPos( const ImVec2& local_pos )
		{
			ImGui::SetCursorPos( local_pos );
		}
		GENERATE void SetCursorPosX( const float x )
		{
			ImGui::SetCursorPosX( x );
		}
		GENERATE void SetCursorPosY( const float y )
		{
			ImGui::SetCursorPosY( y );
		}
		GENERATE ImVec2 GetCursorStartPos()
		{
			return ImGui::GetCursorStartPos();
		}
		GENERATE ImVec2 GetCursorScreenPos()
		{
			return ImGui::GetCursorScreenPos();
		}
		GENERATE void SetCursorScreenPos( const ImVec2& pos )
		{
			ImGui::SetCursorScreenPos( pos );
		}
		GENERATE void AlignTextToFramePadding()
		{
			ImGui::AlignTextToFramePadding();
		}
		GENERATE float GetTextLineHeight()
		{
			return ImGui::GetTextLineHeight();
		}
		GENERATE float GetTextLineHeightWithSpacing()
		{
			return ImGui::GetTextLineHeightWithSpacing();
		}
		GENERATE float GetFrameHeight()
		{
			return ImGui::GetFrameHeight();
		}
		GENERATE float GetFrameHeightWithSpacing()
		{
			return ImGui::GetFrameHeightWithSpacing();
		}
		GENERATE void PopID()
		{
			ImGui::PopID();
		}
		/*
			brief = "Render a labelled button to the screen."
			return = "Returns true when the button was clicked by the user."
			
			[parameters]
				label = "The name of the button, and the unique hash describing the button. Use the '##' operator to append additional hash information hidden from the user."
				size_arg = "Custom size for the button. Use zero on an axis to let ImGui handle the size of an axis for you."
		*/
		GENERATE bool Button( const char* label, const ImVec2& size_arg = ImVec2( 0, 0 ) )
		{
			return ImGui::Button( label, size_arg );
		}
		GENERATE bool SmallButton( const char* label )
		{
			return ImGui::SmallButton( label );
		}
		GENERATE bool InvisibleButton( const char* label, const ImVec2& size_arg )
		{
			return ImGui::InvisibleButton( label, size_arg );
		}
		GENERATE bool ArrowButton( const char* label, const ImGuiDir dir )
		{
			return ImGui::ArrowButton( label, dir );
		}
		GENERATE void Image( const ImTextureID id, const ImVec2& size, const ImVec2& uv0 = ImVec2( 0, 0 ),
							 const ImVec2& uv1 = ImVec2( 1, 1 ),
							 const ImVec4& tint_col = ImVec4( 1, 1, 1, 1 ),
							 const ImVec4& border_col = ImVec4( 0, 0, 0, 0 ) )
		{
			ImGui::Image( id, size, uv0, uv1, tint_col, border_col );
		}
		GENERATE bool ImageButton( const ImTextureID id, const ImVec2& size,
								   const ImVec2& uv0 = ImVec2( 0, 0 ), const ImVec2& uv1 = ImVec2( 1, 1 ),
								   const int frame_padding = -1, const ImVec4& bg_col = ImVec4( 0, 0, 0, 0 ),
								   const ImVec4& tint_col = ImVec4( 1, 1, 1, 1 ) )
		{
			return ImGui::ImageButton( id, size, uv0, uv1, frame_padding, bg_col, tint_col );
		}
		GENERATE void ProgressBar( const float fraction, const ImVec2& size_arg = ImVec2( -1, 0 ),
								   const char* overlay = nullptr )
		{
			ImGui::ProgressBar( fraction, size_arg, overlay );
		}
		GENERATE void Bullet()
		{
			ImGui::Bullet();
		}
		GENERATE bool BeginCombo( const char* label, const char* preview,
								  const ImGuiComboFlags flags = 0 )
		{
			return ImGui::BeginCombo( label, preview, flags );
		}
		GENERATE void EndCombo()
		{
			ImGui::EndCombo();
		}
		GENERATE bool DragFloat( const char* label, float v, function<void( float )> on_changed_callback,
								 const float v_speed = 1.0f, const float v_min = 0.0f,
								 const float v_max = 0.0f, const char* format = "%.3f",
								 const float power = 1.0f )
		{
			if( ImGui::DragFloat( label, &v, v_speed, v_min, v_max, format, power ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragFloat2( const char* label, simd::vector2& v,
								  function<void( simd::vector2 )> on_changed_callback, const float v_speed = 1.0f,
								  const float v_min = 0.0f, const float v_max = 0.0f,
								  const char* format = "%.3f", const float power = 1.0f )
		{
			if( ImGui::DragFloat2( label, v.as_array(), v_speed, v_min, v_max, format, power ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragFloat3( const char* label, simd::vector3& v,
								  function<void( simd::vector3 )> on_changed_callback, const float v_speed = 1.0f,
								  const float v_min = 0.0f, const float v_max = 0.0f,
								  const char* format = "%.3f", const float power = 1.0f )
		{
			if( ImGui::DragFloat3( label, v.as_array(), v_speed, v_min, v_max, format, power ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragFloat4( const char* label, simd::vector4& v,
								  function<void( simd::vector4 )> on_changed_callback, const float v_speed = 1.0f,
								  const float v_min = 0.0f, const float v_max = 0.0f,
								  const char* format = "%.3f", const float power = 1.0f )
		{
			if( ImGui::DragFloat4( label, v.v, v_speed, v_min, v_max, format, power ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragFloatRange2( const char* label, float v_current_min, float v_current_max,
									   function<void( float, float )> on_changed_callback,
									   const float v_speed = 1.0f, const float v_min = 0.0f,
									   const float v_max = 0.0f, const char* format = "%.3f",
									   const char* format_max = nullptr, const ImGuiSliderFlags flags = 0 )
		{
			if( ImGui::DragFloatRange2( label,
										&v_current_min,
										&v_current_max,
										v_speed,
										v_min,
										v_max,
										format,
										format_max,
										flags ) )
			{
				on_changed_callback( v_current_min, v_current_max );
				return true;
			}
			return false;
		}
		GENERATE bool DragInt( const char* label, int v, function<void( int )> on_changed_callback,
							   const float v_speed = 1.0f, const int v_min = 0, const int v_max = 0,
							   const char* format = "%d" )
		{
			if( ImGui::DragInt( label, &v, v_speed, v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragInt2( const char* label, simd::vector2_int& v, function<void( simd::vector2_int )> on_changed_callback,
								const float v_speed = 1.0f, const int v_min = 0, const int v_max = 0,
								const char* format = "%d" )
		{
			if( ImGui::DragInt2( label, v.as_array(), v_speed, v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragInt3( const char* label, simd::vector3_int& v, function<void( simd::vector3_int )> on_changed_callback,
								const float v_speed = 1.0f, const int v_min = 0.0f, const int v_max = 0.0f,
								const char* format = "%d" )
		{
			if( ImGui::DragInt3( label, v.as_array(), v_speed, v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragInt4( const char* label, simd::vector4_int& v, function<void( simd::vector4_int )> on_changed_callback,
								const float v_speed = 1.0f, const int v_min = 0.0f, const int v_max = 0.0f,
								const char* format = "%d" )
		{
			if( ImGui::DragInt4( label, v.as_array(), v_speed, v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool DragIntRange2( const char* label, int v_current_min, int v_current_max,
									 function<void( int, int )> on_changed_callback,
									 const float v_speed = 1.0f, const int v_min = 0.0f,
									 const int v_max = 0.0f, const char* format = "%d",
									 const char* format_max = nullptr )
		{
			if( ImGui::DragIntRange2( label,
									  &v_current_min,
									  &v_current_max,
									  v_speed,
									  v_min,
									  v_max,
									  format,
									  format_max ) )
			{
				on_changed_callback( v_current_min, v_current_max );
				return true;
			}
			return false;
		}
		GENERATE bool SliderFloat( const char* label, float v, const float v_min, const float v_max,
								   function<void( float )> on_changed_callback, const char* format = "%.3f",
								   const float power = 1.0f )
		{
			if( ImGui::SliderFloat( label, &v, v_min, v_max, format, power ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool SliderFloat2( const char* label, simd::vector2& v, const float v_min, const float v_max,
									const char* format = "%.3f", const float power = 1.0f )
		{
			if( ImGui::SliderFloat2( label, v.as_array(), v_min, v_max, format, power ) )
			{
				return true;
			}
			return false;
		}
		GENERATE bool SliderFloat3( const char* label, simd::vector3& v, const float v_min, const float v_max,
									const char* format = "%.3f", const float power = 1.0f )
		{
			// TODO
			return ImGui::SliderFloat3( label, v.as_array(), v_min, v_max, format, power );
		}
		GENERATE bool SliderFloat4( const char* label, simd::vector4& v,
									function<void( simd::vector4 )> on_changed_callback, const float v_min,
									const float v_max, const char* format = "%.3f",
									const float power = 1.0f )
		{
			if( ImGui::SliderFloat4( label, v.v, v_min, v_max, format, power ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		/*
			brief = "Create a slider widget with the intention of allowing the user to customize an angle value."
			return = "Returns true when the user has interacted with this widget, changing the angle value."
			
			[parameters]
				label = "The name of the slider, and the unique hash."
				v_rad = "The current angle value, in radians."
				on_changed_callback = "Callback event. Triggered when the user changes the value."
				v_degrees_min = "Minimum angle, in degrees."
				v_degrees_max = "Maximum angle, in degrees."
				format = "Use this to customize the format of the text displayed on the slider."
		*/
		GENERATE bool SliderAngle( const char* label, float v_rad,
								   function<void( float )> on_changed_callback,
								   const float v_degrees_min = -360.0f, const float v_degrees_max = 360.0f,
								   const char* format = "%.0f deg" )
		{
			if( ImGui::SliderAngle( label, &v_rad, v_degrees_min, v_degrees_max, format ) )
			{
				on_changed_callback( v_rad );
				return true;
			}
			return false;
		}

		GENERATE bool SliderInt( const char* label, int v, function<void( int )> on_changed_callback,
								 const int v_min, const int v_max, const char* format = "%d" )
		{
			if( ImGui::SliderInt( label, &v, v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool SliderInt2( const char* label, simd::vector2_int& v, function<void( simd::vector2_int )> on_changed_callback,
								  const int v_min, const int v_max, const char* format = "%d" )
		{
			if( ImGui::SliderInt2( label, v.as_array(), v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool SliderInt3( const char* label, simd::vector3_int& v, function<void( simd::vector3_int )> on_changed_callback,
								  const int v_min, const int v_max, const char* format = "%d" )
		{
			if( ImGui::SliderInt3( label, v.as_array(), v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool SliderInt4( const char* label, simd::vector4_int& v, function<void( simd::vector4_int )> on_changed_callback,
								  const int v_min, const int v_max, const char* format = "%d" )
		{
			if( ImGui::SliderInt4( label, v.as_array(), v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool VSliderFloat( const char* label, const ImVec2& size, float v,
									function<void( float )> on_changed_callback, float v_min,
									const float v_max, const char* format = "%.3f",
									const ImGuiSliderFlags flags = 0 )
		{
			if( ImGui::VSliderFloat( label, size, &v, v_min, v_max, format, flags ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool VSliderInt( const char* label, const ImVec2& size, int v,
								  function<void( int )> on_changed_callback, const int v_min, const int v_max,
								  const char* format = "%d" )
		{
			if( ImGui::VSliderInt( label, size, &v, v_min, v_max, format ) )
			{
				on_changed_callback( v );
				return true;
			}
			return false;
		}
		GENERATE bool ColorButton( const char* desc_id, const ImVec4& col,
								   const ImGuiColorEditFlags flags = 0, const ImVec2 size = ImVec2( 0, 0 ) )
		{
			return ImGui::ColorButton( desc_id, col, flags, size );
		}
		GENERATE void SetColorEditOptions( const ImGuiColorEditFlags flags )
		{
			ImGui::SetColorEditOptions( flags );
		}
		GENERATE void TreePop()
		{
			ImGui::TreePop();
		}
		GENERATE void TreeAdvanceToLabelPos()
		{
			ImGui::SetCursorPosX( ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing() );
		}
		GENERATE float GetTreeNodeToLabelSpacing()
		{
			return ImGui::GetTreeNodeToLabelSpacing();
		}
		GENERATE void SetNextTreeNodeOpen( bool is_open, const ImGuiCond cond = 0 )
		{
			ImGui::SetNextItemOpen( is_open, cond );
		}
		GENERATE void ListBoxFooter()
		{
			ImGui::ListBoxFooter();
		}
		GENERATE bool BeginMainMenuBar()
		{
			return ImGui::BeginMainMenuBar();
		}
		GENERATE void EndMainMenuBar()
		{
			ImGui::EndMainMenuBar();
		}
		GENERATE bool BeginMenuBar()
		{
			return ImGui::BeginMenuBar();
		}
		GENERATE void EndMenuBar()
		{
			ImGui::EndMenuBar();
		}
		GENERATE bool BeginMenu( const char* label, const bool enabled = true )
		{
			return ImGui::BeginMenu( label, enabled );
		}
		GENERATE void EndMenu()
		{
			ImGui::EndMenu();
		}
		GENERATE void BeginTooltip()
		{
			ImGui::BeginTooltip();
		}
		GENERATE void EndTooltip()
		{
			ImGui::EndTooltip();
		}
		GENERATE void OpenPopup( const char* id )
		{
			ImGui::OpenPopup( id );
		}
		GENERATE bool BeginPopup( const char* str_id, const ImGuiWindowFlags flags = 0 )
		{
			return ImGui::BeginPopup( str_id, flags | ImGuiWindowFlags_Popup );
		}
		GENERATE bool BeginPopupContextItem( const char* str_id = nullptr, const int mouse_button = 1 )
		{
			return ImGui::BeginPopupContextItem( str_id, mouse_button );
		}
		GENERATE bool BeginPopupContextWindow( const char* str_id = nullptr, const int mouse_button = 1,
											   const bool also_over_items = true )
		{
			return ImGui::BeginPopupContextWindow( str_id, mouse_button, also_over_items );
		}
		GENERATE bool BeginPopupContextVoid( const char* str_id = nullptr, const int mouse_button = 1 )
		{
			return ImGui::BeginPopupContextVoid( str_id, mouse_button );
		}
		GENERATE bool BeginPopupModal( const char* name, function<void()> closed_callback = nullptr,
									   const ImGuiWindowFlags flags = 0 )
		{
			if( closed_callback )
			{
				bool open = true;
				const auto result =
				  ImGui::BeginPopupModal( name, &open, flags | ImGuiWindowFlags_Popup );

				if( !open )
					closed_callback();

				return result;
			}
			return ImGui::BeginPopupModal( name, nullptr, flags | ImGuiWindowFlags_Popup );
		}
		GENERATE void EndPopup()
		{
			ImGui::EndPopup();
		}
		GENERATE void OpenPopupOnItemClick( const char* str_id = nullptr, const int mouse_button = 1 )
		{
			ImGui::OpenPopupOnItemClick( str_id, mouse_button );
		}
		GENERATE void CloseCurrentPopup()
		{
			ImGui::CloseCurrentPopup();
		}
		/*
			brief = "Create a table of rows/columns. Use NextColumn() to step through columns. Rows are created implicitly with NextColumn(). Use Columns(1) to return to normal."
			
			[parameters]
				columns_count = "The number of columns in the table."
				id = "Used to hash the columns, if you need to differentiate one set of columns from another."
				border = "Whether or not you should render a border to the columns."
		*/
		GENERATE void Columns( const int columns_count = 1, const char* id = nullptr,
							   const bool border = true )
		{
			ImGui::Columns( columns_count, id, border );
		}
		GENERATE void NextColumn()
		{
			ImGui::NextColumn();
		}
		GENERATE int GetColumnIndex()
		{
			return ImGui::GetColumnIndex();
		}
		GENERATE float GetColumnWidth()
		{
			return ImGui::GetColumnWidth();
		}
		GENERATE void SetColumnWidth( const int column_index, const float width )
		{
			ImGui::SetColumnWidth( column_index, width );
		}
		GENERATE float GetColumnOffset()
		{
			return ImGui::GetColumnOffset();
		}
		GENERATE void SetColumnOffset( const int column_index, const float offset )
		{
			ImGui::SetColumnOffset( column_index, offset );
		}
		GENERATE int GetColumnsCount()
		{
			return ImGui::GetColumnsCount();
		}
		GENERATE bool BeginTabBar( const char* str_id, const ImGuiTabBarFlags flags = 0 )
		{
			return ImGui::BeginTabBar( str_id, flags );
		}
		GENERATE void EndTabBar()
		{
			ImGui::EndTabBar();
		}
		GENERATE bool BeginTabItem( const char* name, function<void()> on_close = nullptr, ImGuiTabItemFlags flags = 0 )
		{
			if( on_close )
			{
				bool open = true;
				const bool result = ImGui::BeginTabItem( name, &open, flags );

				if( !open )
					on_close();

				return result;
			}
			else
			{
				return ImGui::BeginTabItem( name, nullptr, flags );
			}
		}
		GENERATE void EndTabItem()
		{
			ImGui::EndTabItem();
		}
		GENERATE void SetTabItemClosed( const char* label )
		{
			ImGui::SetTabItemClosed( label );
		}
		GENERATE void DockSpace( const ImGuiID id, const ImVec2& size_arg = ImVec2( 0, 0 ),
								 const ImGuiDockNodeFlags flags = 0,
								 const ImGuiWindowClass* window_class = nullptr )
		{
			ImGui::DockSpace( id, size_arg, flags, window_class );
		}
		GENERATE ImGuiID DockSpaceOverViewport( ImGuiViewport* viewport = nullptr,
												const ImGuiDockNodeFlags dockspace_flags = 0,
												const ImGuiWindowClass* window_class = nullptr )
		{
			return ImGui::DockSpaceOverViewport( viewport, dockspace_flags, window_class );
		}
		GENERATE void SetNextWindowDockID( const ImGuiID id, const ImGuiCond cond = 0 )
		{
			ImGui::SetNextWindowDockID( id, cond );
		}
		GENERATE void SetNextWindowClass( const ImGuiWindowClass* window_class )
		{
			ImGui::SetNextWindowClass( window_class );
		}
		GENERATE ImGuiID GetWindowDockID()
		{
			return ImGui::GetWindowDockID();
		}
		GENERATE bool IsWindowDocked()
		{
			return ImGui::IsWindowDocked();
		}
		GENERATE void LogToTTY( int auto_open_depth = -1 )
		{
			ImGui::LogToTTY( auto_open_depth );
		}
		GENERATE void LogToFile( int auto_open_depth = -1, const char* filename = NULL )
		{
			ImGui::LogToFile( auto_open_depth, filename );
		}
		GENERATE void LogToClipboard( int auto_open_depth = -1 )
		{
			ImGui::LogToClipboard( auto_open_depth );
		}
		GENERATE void LogFinish()
		{
			ImGui::LogFinish();
		}
		GENERATE void LogButtons()
		{
			ImGui::LogButtons();
		}
		GENERATE void LogText( const char* text )
		{
			ImGui::LogText( text );
		}
		GENERATE bool BeginDragDropSource( const ImGuiDragDropFlags flags = 0 )
		{
			return ImGui::BeginDragDropSource( flags );
		}
		GENERATE bool SetDragDropPayload( const char* type, const void* data, const size_t data_size,
										  const ImGuiCond cond = 0 )
		{
			// TODO
			return ImGui::SetDragDropPayload( type, data, data_size, cond );
		}
		GENERATE void EndDragDropSource()
		{
			ImGui::EndDragDropSource();
		}
		GENERATE bool BeginDragDropTarget()
		{
			return ImGui::BeginDragDropTarget();
		}
		GENERATE const ImGuiPayload* AcceptDragDropPayload( const char* type,
															const ImGuiDragDropFlags flags = 0 )
		{
			return ImGui::AcceptDragDropPayload( type, flags );
		}
		GENERATE void EndDragDropTarget()
		{
			ImGui::EndDragDropTarget();
		}
		GENERATE const ImGuiPayload* GetDragDropPayload()
		{
			return ImGui::GetDragDropPayload();
		}
		GENERATE void PushClipRect( const ImVec2& cr_min, const ImVec2& cr_max,
									const bool intersect_with_current_clip_rect )
		{
			ImGui::PushClipRect( cr_min, cr_max, intersect_with_current_clip_rect );
		}
		GENERATE void PopClipRect()
		{
			ImGui::PopClipRect();
		}
		GENERATE void SetItemDefaultFocus()
		{
			ImGui::SetItemDefaultFocus();
		}
		GENERATE void SetKeyboardFocusHere( const int offset = 0 )
		{
			ImGui::SetKeyboardFocusHere( offset );
		}
		GENERATE bool IsItemHovered( const ImGuiHoveredFlags flags = 0 )
		{
			return ImGui::IsItemHovered( flags );
		}
		GENERATE bool IsItemActive()
		{
			return ImGui::IsItemActive();
		}
		GENERATE bool IsItemFocused()
		{
			return ImGui::IsItemFocused();
		}
		GENERATE bool IsItemClicked()
		{
			return ImGui::IsItemClicked();
		}
		GENERATE bool IsItemVisible()
		{
			return ImGui::IsItemVisible();
		}
		GENERATE bool IsItemEdited()
		{
			return ImGui::IsItemEdited();
		}
		GENERATE bool IsItemActivated()
		{
			return ImGui::IsItemActivated();
		}
		GENERATE bool IsItemDeactivated()
		{
			return ImGui::IsItemDeactivated();
		}
		GENERATE bool IsItemDeactivatedAfterEdit()
		{
			return ImGui::IsItemDeactivatedAfterEdit();
		}
		GENERATE bool IsAnyItemHovered()
		{
			return ImGui::IsAnyItemHovered();
		}
		GENERATE bool IsAnyItemActive()
		{
			return ImGui::IsAnyItemActive();
		}
		GENERATE bool IsAnyItemFocused()
		{
			return ImGui::IsAnyItemFocused();
		}
		GENERATE ImVec2 GetItemRectMin()
		{
			return ImGui::GetItemRectMin();
		}
		GENERATE ImVec2 GetItemRectMax()
		{
			return ImGui::GetItemRectMax();
		}
		GENERATE ImVec2 GetItemRectSize()
		{
			return ImGui::GetItemRectSize();
		}
		GENERATE void SetItemAllowOverlap()
		{
			ImGui::SetItemAllowOverlap();
		}
		GENERATE double GetTime()
		{
			return ImGui::GetTime();
		}
		GENERATE int GetFrameCount()
		{
			return ImGui::GetFrameCount();
		}
		GENERATE const char* GetStyleColorName( const ImGuiCol idx )
		{
			return ImGui::GetStyleColorName( idx );
		}
		GENERATE ImVec2 CalcTextSize( const string& text, const bool hide_text_after_double_hash = false,
									  const float wrap_width = -1.0f )
		{
			return ImGui::CalcTextSize( text.c_str(),
										text.c_str() + text.size(),
										hide_text_after_double_hash,
										wrap_width );
		}
		GENERATE bool BeginChildFrame( const char* id, const ImVec2& size = ImVec2( 0, 0 ),
									   const ImGuiWindowFlags extra_flags = 0 )
		{
			return ImGui::BeginChildFrame( ImGui::GetID( id ), size, extra_flags );
		}
		GENERATE bool BeginChildFrame_2( const ImGuiID id, const ImVec2& size = ImVec2( 0, 0 ),
										 const ImGuiWindowFlags extra_flags = 0 )
		{
			return ImGui::BeginChildFrame( id, size, extra_flags );
		}
		GENERATE void EndChildFrame()
		{
			ImGui::EndChildFrame();
		}
		GENERATE ImVec4 ColorConvertU32ToFloat4( const ImU32 in )
		{
			return ImGui::ColorConvertU32ToFloat4( in );
		}
		GENERATE ImU32 ColorConvertFloat4ToU32( const ImVec4& in )
		{
			return ImGui::ColorConvertFloat4ToU32( in );
		}
		GENERATE std::tuple<float, float, float> ColorConvertHSVtoRGB( const float h, const float s,
																	   const float v )
		{
			float out_r, out_g, out_b;
			ImGui::ColorConvertHSVtoRGB( h, s, v, out_r, out_g, out_b );
			return std::tie( out_r, out_g, out_b );
		}
		GENERATE std::tuple<float, float, float> ColorConvertRGBtoHSV( const float r, const float g,
																	   const float b )
		{
			float out_h, out_s, out_v;
			ImGui::ColorConvertRGBtoHSV( r, g, b, out_h, out_s, out_v );
			return std::tie( out_h, out_s, out_v );
		}
		GENERATE int GetKeyIndex( const ImGuiKey imgui_key )
		{
			return ImGui::GetKeyIndex( imgui_key );
		}
		GENERATE bool IsKeyDown( const int user_key_index )
		{
			return ImGui::IsKeyDown( user_key_index );
		}
		GENERATE bool IsKeyPressed( const int user_key_index, const bool repeat = true )
		{
			return ImGui::IsKeyPressed( user_key_index, repeat );
		}
		GENERATE bool IsKeyReleased( const int user_key_index )
		{
			return ImGui::IsKeyReleased( user_key_index );
		}
		GENERATE int GetKeyPressedAmount( const int key_index, const float repeat_delay,
										  const float repeat_rate )
		{
			return ImGui::GetKeyPressedAmount( key_index, repeat_delay, repeat_rate );
		}
		GENERATE bool IsMouseDown( const int button )
		{
			return ImGui::IsMouseDown( button );
		}
		GENERATE bool IsAnyMouseDown()
		{
			return ImGui::IsAnyMouseDown();
		}
		GENERATE bool IsMouseClicked( const int button, const bool repeat = false )
		{
			return ImGui::IsMouseClicked( button, repeat );
		}
		GENERATE bool IsMouseDoubleClicked( const int button )
		{
			return ImGui::IsMouseDoubleClicked( button );
		}
		GENERATE bool IsMouseReleased( const int button )
		{
			return ImGui::IsMouseReleased( button );
		}
		GENERATE bool IsMouseDragging( const int button, const float lock_threshold = -1.0f )
		{
			return ImGui::IsMouseDragging( button, lock_threshold );
		}
		GENERATE bool IsMouseHoveringRect( const ImVec2& r_min, const ImVec2& r_max, const bool clip = true )
		{
			return ImGui::IsMouseHoveringRect( r_min, r_max, clip );
		}
		GENERATE bool IsMousePosValid( const ImVec2* mouse_pos = nullptr )
		{
			return ImGui::IsMousePosValid( mouse_pos );
		}
		GENERATE ImVec2 GetMousePos()
		{
			return ImGui::GetMousePos();
		}
		GENERATE ImVec2 GetMousePosOnOpeningCurrentPopup()
		{
			return ImGui::GetMousePosOnOpeningCurrentPopup();
		}
		GENERATE ImVec2 GetMouseDragDelta( const int button = 0, const float lock_threshold = -1.0f )
		{
			return ImGui::GetMouseDragDelta( button, lock_threshold );
		}
		GENERATE void ResetMouseDragDelta( const int button = 0 )
		{
			ImGui::ResetMouseDragDelta( button );
		}
		GENERATE ImGuiMouseCursor GetMouseCursor()
		{
			return ImGui::GetMouseCursor();
		}
		GENERATE void SetMouseCursor( const ImGuiMouseCursor cursor_type )
		{
			ImGui::SetMouseCursor( cursor_type );
		}
		GENERATE void CaptureKeyboardFromApp( const bool capture = true )
		{
			ImGui::CaptureKeyboardFromApp( capture );
		}
		GENERATE void CaptureMouseFromApp( const bool capture = true )
		{
			ImGui::CaptureMouseFromApp( capture );
		}
		GENERATE const char* GetClipboardText()
		{
			return ImGui::GetClipboardText();
		}
		GENERATE void SetClipboardText( const char* text )
		{
			ImGui::SetClipboardText( text );
		}
		GENERATE py::object LoadFont( const char* font_file, const float size = 12.0f,
									  const int oversample_h = 3, const int oversample_v = 1,
									  const bool pixel_snap = false )
		{
			auto& io = ImGui::GetIO();

			ImFontConfig config;
			config.OversampleH = oversample_h;
			config.OversampleV = oversample_v;
			config.PixelSnapH = pixel_snap;

			ImFont* font = io.Fonts->AddFontFromFileTTF( font_file, size, &config );
			assert( font );
			return py::cast( font );
		}

		template<typename T>
		void _DeclareVector( py::module& m, const char* typestr )
		{
			{
				std::string pyclass_name = std::string( "ImVector<" ) + typestr + ">";
				py::class_<ImVector<T>>( m, pyclass_name.c_str(), py::buffer_protocol(), py::dynamic_attr() )
				  .read( ImVector<T>, Capacity )
				  .read( ImVector<T>, Size )
				  .def( "__getitem__",
						[]( const ImVector<T>& v, int idx ) -> const T& { return v[idx]; },
						py::is_operator() );
			}

			{
				std::string pyclass_name = std::string( "ImVector<" ) + typestr + "*>";
				py::class_<ImVector<T*>>( m, pyclass_name.c_str(), py::buffer_protocol(), py::dynamic_attr() )
				  .read( ImVector<T*>, Capacity )
				  .read( ImVector<T*>, Size )
				  .def( "__getitem__",
						[]( const ImVector<T*>& v, int idx ) -> const T& { return *v[idx]; },
						py::is_operator() );
			}
		}

#define DECLARE_VECTOR( type ) _DeclareVector<type>( mod, #type );

		void ImGuiModule::Initialise( pybind11::module& mod )
		{
			DECLARE_VECTOR( ImFont );
			DECLARE_VECTOR( ImGuiWindow );
			DECLARE_VECTOR( ImGuiViewportP );
			DECLARE_VECTOR( ImGuiViewport );
			DECLARE_VECTOR( ImGuiPopupData );
			DECLARE_VECTOR( ImGuiPlatformMonitor );
			DECLARE_VECTOR( ImDrawList );

			class1( ImFont )
			  .read( ImFont, IndexAdvanceX )
			  .read( ImFont, FallbackAdvanceX )
			  .read( ImFont, FontSize )
			  .read( ImFont, IndexLookup )
			  .read( ImFont, Glyphs )
			  .read( ImFont, FallbackGlyph )
			  .read( ImFont, ContainerAtlas )
			  .read( ImFont, ConfigData )
			  .read( ImFont, ConfigDataCount )
			  .read( ImFont, FallbackChar )
			  .read( ImFont, Scale )
			  .read( ImFont, Ascent )
			  .read( ImFont, Descent )
			  .read( ImFont, MetricsTotalSurface )
			  .read( ImFont, DirtyLookupTables )
			  .method( ImFont, FindGlyph, pyarg( c ) )
			  .method( ImFont, FindGlyphNoFallback, pyarg( c ) )
			  .method( ImFont, GetCharAdvance, pyarg( c ) )
			  .method( ImFont, IsLoaded )
			  .method( ImFont, GetDebugName );

			class1( ImColor, float, float, float, float )
				.def( "__eq__", []( const ImColor& a, const ImColor& b ) { return a == b; }, py::is_operator() )
				.def( "__mul__", []( float a, const ImColor& b ) { return b * a; }, py::is_operator() )
				.def( "__repr__", []( const ImColor& v ) {
					std::stringstream str;
					str << v.Value.x << ", " << v.Value.y << ", " << v.Value.z << ", " << v.Value.w;
					return str.str();
				} )
				.write( ImColor, Value );

			class1( ImVec2, float, float )
			  .def( py::self + py::self )
			  .def( py::self - py::self )
			  .def( py::self * py::self )
			  .def( py::self / py::self )
			  .def( py::self += py::self )
			  .def( py::self -= py::self )
			  .def( py::self * float() )
			  .def( py::self *= float() )
			  .def( py::self / float() )
			  .def( py::self /= float() )
			  .def( "__mul__", []( float a, const ImVec2& b ) { return b * a; }, py::is_operator() )
			  .def( "__eq__", []( const ImVec2& a, const ImVec2& b ) { return a.x == b.x && a.y == b.y; }, py::is_operator() )
			  .def( "__repr__",
					[]( const ImVec2& v ) {
						std::stringstream str;
						str << v.x << ", " << v.y;
						return str.str();
					} )
			  .write( ImVec2, x )
			  .write( ImVec2, y );

			class1( ImVec4, float, float, float, float )
			  .def( py::self + py::self )
			  .def( py::self - py::self )
			  .def( py::self * py::self )
			  .def( "__div__",
					[]( const ImVec4& a, const ImVec4& b ) {
						return ImVec4( a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w );
					},
					py::is_operator() )
			  .def( "__isub__",
					[]( const ImVec4& a, const ImVec4& b ) {
						return ImVec4( a.x - b.x, a.y - b.x, a.z - b.z, a.w - b.w );
					},
					py::is_operator() )
			  .def( "__iadd__",
					[]( const ImVec4& a, const ImVec4& b ) {
						return ImVec4( a.x + b.x, a.y + b.x, a.z + b.z, a.w + b.w );
					},
					py::is_operator() )
			  .def( "__mul__",
					[]( const ImVec4& a, float b ) { return ImVec4( a.x * b, a.y * b, a.z * b, a.w * b ); },
					py::is_operator() )
			  .def( "__imul__",
					[]( const ImVec4& a, float b ) { return ImVec4( a.x * b, a.y * b, a.z * b, a.w * b ); },
					py::is_operator() )
			  .def( "__div__",
					[]( const ImVec4& a, float b ) { return ImVec4( a.x / b, a.y / b, a.z / b, a.w / b ); },
					py::is_operator() )
			  .def( "__idiv__",
					[]( const ImVec4& a, float b ) { return ImVec4( a.x / b, a.y / b, a.z / b, a.w / b ); },
					py::is_operator() )
			  .def( "__mul__",
					[]( float a, const ImVec4& b ) { return ImVec4( a * b.x, a * b.y, a * b.z, a * b.w ); },
					py::is_operator() )
			  .def( "__repr__",
					[]( const ImVec4& v ) {
						std::stringstream str;
						str << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
						return str.str();
					} )
			  .def( "__eq__", []( const ImVec4& a, const ImVec4& b ) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }, py::is_operator() )
			  .write( ImVec4, x )
			  .write( ImVec4, y )
			  .write( ImVec4, z )
			  .write( ImVec4, w );

			class1( ImDrawListSharedData );

			class1( ImDrawList, const ImDrawListSharedData* )
			  .read( ImDrawList, CmdBuffer )
			  .read( ImDrawList, IdxBuffer )
			  .read( ImDrawList, VtxBuffer )
			  .read( ImDrawList, Flags )
			  .read( ImDrawList, Flags )
			  .method( ImDrawList,
					   PushClipRect,
					   pyarg( clip_rect_min ),
					   pyarg( clip_rect_max ),
					   pyarg( intersect_with_current_clip_rect ) = false )
			  .method( ImDrawList, PushClipRectFullScreen )
			  .method( ImDrawList, PopClipRect )
			  .method( ImDrawList, PushTextureID, pyarg( texture_id ) )
			  .method( ImDrawList, PopTextureID )
			  .method( ImDrawList, GetClipRectMin )
			  .method( ImDrawList, GetClipRectMax )
			  .method( ImDrawList, AddLine, pyarg( a ), pyarg( b ), pyarg( col ), pyarg( thickness ) = 1.0f )
			  .method( ImDrawList,
					   AddRect,
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( col ),
					   pyarg( rounding ) = 0.0f,
					   pyarg( rounding_corners_flags ) = (int)ImDrawCornerFlags_All,
					   pyarg( thickness ) = 1.0f )
			  .method( ImDrawList,
					   AddRectFilled,
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( col ),
					   pyarg( rounding ) = 0.0f,
					   pyarg( rounding_corners_flags ) = (int)ImDrawCornerFlags_All )
			  .method( ImDrawList,
					   AddRectFilledMultiColor,
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( col_upr_left ),
					   pyarg( col_upr_right ),
					   pyarg( col_bot_right ),
					   pyarg( col_bot_left ) )
			  .method( ImDrawList,
					   AddQuad,
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( c ),
					   pyarg( d ),
					   pyarg( col ),
					   pyarg( thickness ) = 1.0f )
			  .method( ImDrawList,
					   AddQuadFilled,
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( c ),
					   pyarg( d ),
					   pyarg( col ) )
			  .method( ImDrawList,
					   AddTriangle,
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( c ),
					   pyarg( col ),
					   pyarg( thickness ) = 1.0f )
			  .method( ImDrawList, AddTriangleFilled, pyarg( a ), pyarg( b ), pyarg( c ), pyarg( col ) )
			  .method( ImDrawList,
					   AddCircle,
					   pyarg( centre ),
					   pyarg( radius ),
					   pyarg( col ),
					   pyarg( num_segments ) = 12,
					   pyarg( thickness ) = 1.0f )
			  .method( ImDrawList,
					   AddCircleFilled,
					   pyarg( centre ),
					   pyarg( radius ),
					   pyarg( col ),
					   pyarg( num_segments ) = 12 )
			  .def( "AddText",
					( void ( ImDrawList::* )( const ImFont*,
											  float,
											  const ImVec2&,
											  ImU32,
											  const char*,
											  const char*,
											  float,
											  const ImVec4* ) ) &
					  ImDrawList::AddText,
					pyarg( font ),
					pyarg( font_size ),
					pyarg( pos ),
					pyarg( col ),
					pyarg( text_begin ),
					pyarg( text_end ) = nullptr,
					pyarg( wrap_width ) = 0.0f,
					pyarg( cpu_fine_clip_rect ) = nullptr )

			  .def( "AddText",
					( void ( ImDrawList::* )( const ImVec2& pos,
											  ImU32 col,
											  const char* text_begin,
											  const char* text_end ) ) &
					  ImDrawList::AddText,
					pyarg( pos ),
					pyarg( col ),
					pyarg( text_begin ),
					pyarg( text_end ) = nullptr )
			  .method( ImDrawList,
					   AddImage,
					   pyarg( user_texture_id ),
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( uv_a ) = ImVec2( 0, 0 ),
					   pyarg( uv_b ) = ImVec2( 1, 1 ),
					   pyarg( col ) = IM_COL32_WHITE )
			  .method( ImDrawList,
					   AddImageQuad,
					   pyarg( user_texture_id ),
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( c ),
					   pyarg( d ),
					   pyarg( uv_a ) = ImVec2( 0, 0 ),
					   pyarg( uv_b ) = ImVec2( 1, 1 ),
					   pyarg( uv_c ) = ImVec2( 0, 0 ),
					   pyarg( uv_d ) = ImVec2( 1, 1 ),
					   pyarg( col ) = IM_COL32_WHITE )
			  .method( ImDrawList,
					   AddImageRounded,
					   pyarg( user_texture_id ),
					   pyarg( a ),
					   pyarg( b ),
					   pyarg( uv_a ),
					   pyarg( uv_b ),
					   pyarg( col ),
					   pyarg( rounding ),
					   pyarg( rounding_corners ) = (int)ImDrawCornerFlags_All )
			  .method( ImDrawList,
					   AddPolyline,
					   pyarg( points ),
					   pyarg( num_points ),
					   pyarg( col ),
					   pyarg( closed ),
					   pyarg( thickness ) )
			  .method( ImDrawList, AddConvexPolyFilled, pyarg( points ), pyarg( num_points ), pyarg( col ) )
			  .method( ImDrawList,
					   AddBezierCurve,
					   pyarg( pos0 ),
					   pyarg( cp0 ),
					   pyarg( cp1 ),
					   pyarg( pos1 ),
					   pyarg( col ),
					   pyarg( thickness ),
					   pyarg( num_segments ) )
			  .method( ImDrawList, PathClear )
			  .method( ImDrawList, PathLineTo, pyarg( pos ) )
			  .method( ImDrawList, PathLineToMergeDuplicate, pyarg( pos ) )
			  .method( ImDrawList, PathFillConvex, pyarg( coll ) )
			  .method( ImDrawList, PathStroke, pyarg( col ), pyarg( closed ), pyarg( thickness ) = 1.0f )
			  .method( ImDrawList,
					   PathArcTo,
					   pyarg( centre ),
					   pyarg( radius ),
					   pyarg( a_min ),
					   pyarg( a_max ),
					   pyarg( num_segments ) = 10 )
			  .method( ImDrawList,
					   PathArcToFast,
					   pyarg( centre ),
					   pyarg( radius ),
					   pyarg( a_min_of_12 ),
					   pyarg( a_max_of_12 ) )
			  .method( ImDrawList,
					   PathBezierCurveTo,
					   pyarg( p1 ),
					   pyarg( p2 ),
					   pyarg( p3 ),
					   pyarg( num_segments ) = 0 )
			  .method( ImDrawList,
					   PathRect,
					   pyarg( rect_min ),
					   pyarg( rect_max ),
					   pyarg( rounding ),
					   pyarg( rounding_corners_flags ) = (int)ImDrawCornerFlags_All )
			  .method( ImDrawList, ChannelsSplit, pyarg( channels_count ) )
			  .method( ImDrawList, ChannelsMerge )
			  .method( ImDrawList, ChannelsSetCurrent, pyarg( channel_index ) );

			class1( ImGuiViewport )
			  .read( ImGuiViewport, ID )
			  .read( ImGuiViewport, Flags )
			  .read( ImGuiViewport, Pos )
			  .read( ImGuiViewport, Size )
			  .read( ImGuiViewport, DpiScale )
			  .read( ImGuiViewport, DrawData )
			  .read( ImGuiViewport, ParentViewportId );

			class1( ImGuiViewportP )
			  .read( ImGuiViewportP, ID )
			  .read( ImGuiViewportP, Flags )
			  .read( ImGuiViewportP, Pos )
			  .read( ImGuiViewportP, Size )
			  .read( ImGuiViewportP, DpiScale )
			  .read( ImGuiViewportP, DrawData )
			  .read( ImGuiViewportP, ParentViewportId )
			  .read( ImGuiViewportP, Idx )
			  .read( ImGuiViewportP, LastFrameActive )
			  .read( ImGuiViewportP, LastFrontMostStampCount )
			  .read( ImGuiViewportP, LastNameHash )
			  .read( ImGuiViewportP, LastPos )
			  .read( ImGuiViewportP, Alpha )
			  .read( ImGuiViewportP, LastAlpha )
			  .read( ImGuiViewportP, PlatformMonitor )
			  .read( ImGuiViewportP, PlatformWindowCreated )
			  .read( ImGuiViewportP, Window )
			  //.read( ImGuiViewportP, DrawLists )
			  .read( ImGuiViewportP, DrawDataP )
			  .read( ImGuiViewportP, DrawDataBuilder )
			  .read( ImGuiViewportP, LastPlatformPos )
			  .read( ImGuiViewportP, LastPlatformSize )
			  .read( ImGuiViewportP, LastRendererSize );

			class1( ImDrawDataBuilder )
			  .read( ImDrawDataBuilder, Layers )
			  .method( ImDrawDataBuilder, Clear )
			  .method( ImDrawDataBuilder, ClearFreeMemory )
			  .method( ImDrawDataBuilder, FlattenIntoSingleLayer );

			class1( ImGuiPlatformMonitor )
			  .read( ImGuiPlatformMonitor, MainPos )
			  .read( ImGuiPlatformMonitor, MainSize )
			  .read( ImGuiPlatformMonitor, WorkPos )
			  .read( ImGuiPlatformMonitor, WorkSize )
			  .read( ImGuiPlatformMonitor, DpiScale );

			class1( ImGuiPlatformIO )
			  .read( ImGuiPlatformIO, Monitors )
			  .read( ImGuiPlatformIO, Viewports );

			class1( ImGuiWindow, ImGuiContext*, const char* )
			  .read( ImGuiWindow, Name )
			  .read( ImGuiWindow, ID )
			  .read( ImGuiWindow, Flags )
			  .read( ImGuiWindow, FlagsPreviousFrame )
			  .read( ImGuiWindow, WindowClass )
			  .read( ImGuiWindow, Viewport )
			  .read( ImGuiWindow, ViewportId )
			  .read( ImGuiWindow, ViewportPos )
			  .read( ImGuiWindow, ViewportAllowPlatformMonitorExtend )
			  .read( ImGuiWindow, Pos )
			  .read( ImGuiWindow, Size )
			  .read( ImGuiWindow, SizeFull )
			  .read( ImGuiWindow, WindowPadding )
			  .read( ImGuiWindow, WindowRounding )
			  .read( ImGuiWindow, WindowBorderSize )
			  .read( ImGuiWindow, NameBufLen )
			  .read( ImGuiWindow, MoveId )
			  .read( ImGuiWindow, ChildId )
			  .read( ImGuiWindow, Scroll )
			  .read( ImGuiWindow, ScrollTarget )
			  .read( ImGuiWindow, ScrollTargetCenterRatio )
			  .read( ImGuiWindow, ScrollbarSizes )
			  .read( ImGuiWindow, ScrollbarX )
			  .read( ImGuiWindow, ScrollbarY )
			  .read( ImGuiWindow, ViewportOwned )
			  .read( ImGuiWindow, Active )
			  .read( ImGuiWindow, WasActive )
			  .read( ImGuiWindow, WriteAccessed )
			  .read( ImGuiWindow, Collapsed )
			  .read( ImGuiWindow, WantCollapseToggle )
			  .read( ImGuiWindow, SkipItems )
			  .read( ImGuiWindow, Appearing )
			  .read( ImGuiWindow, Hidden )
			  .read( ImGuiWindow, HasCloseButton )
			  .read( ImGuiWindow, ResizeBorderHeld )
			  .read( ImGuiWindow, BeginCount )
			  .read( ImGuiWindow, BeginOrderWithinParent )
			  .read( ImGuiWindow, BeginOrderWithinContext )
			  .read( ImGuiWindow, PopupId )
			  .read( ImGuiWindow, AutoFitFramesX )
			  .read( ImGuiWindow, AutoFitFramesY )
			  .read( ImGuiWindow, AutoFitOnlyGrows )
			  .read( ImGuiWindow, AutoFitChildAxises )
			  .read( ImGuiWindow, AutoPosLastDirection )
			  //.read( ImGuiWindow, HiddenFramesCanSkipItems )
			  //.read( ImGuiWindow, HiddenFramesCannotSkipItems )
			  //.read( ImGuiWindow, SetWindowPosAllowFlags )
			  //.read( ImGuiWindow, SetWindowSizeAllowFlags )
			  //.read( ImGuiWindow, SetWindowCollapsedAllowFlags )
			  //.read( ImGuiWindow, SetWindowDockAllowFlags )
			  .read( ImGuiWindow, SetWindowPosVal )
			  .read( ImGuiWindow, SetWindowPosPivot )
			  .read( ImGuiWindow, DC )
			  .read( ImGuiWindow, IDStack )
			  .read( ImGuiWindow, ClipRect )
			  .read( ImGuiWindow, OuterRectClipped )
			  .read( ImGuiWindow, InnerRect )
			  .read( ImGuiWindow, InnerClipRect )
			  .read( ImGuiWindow, HitTestHoleSize )
			  .read( ImGuiWindow, HitTestHoleOffset )
			  .read( ImGuiWindow, ContentSize )
			  .read( ImGuiWindow, LastFrameActive )
			  //.read( ImGuiWindow, LastFrameJustFocused )
			  .read( ImGuiWindow, ItemWidthDefault )
			  //.read( ImGuiWindow, MenuColumns )
			  .read( ImGuiWindow, StateStorage )
			  .read( ImGuiWindow, ColumnsStorage )
			  .read( ImGuiWindow, FontWindowScale )
			  .read( ImGuiWindow, FontDpiScale )
			  .read( ImGuiWindow, SettingsOffset )
			  .read( ImGuiWindow, DrawList )
			  .read( ImGuiWindow, DrawListInst )
			  .read( ImGuiWindow, ParentWindow )
			  .read( ImGuiWindow, RootWindow )
			  .read( ImGuiWindow, RootWindowForTitleBarHighlight )
			  .read( ImGuiWindow, RootWindowForNav )
			  .read( ImGuiWindow, NavLastChildNavWindow )
			  .read( ImGuiWindow, DockNode )
			  .read( ImGuiWindow, DockNodeAsHost )
			  .read( ImGuiWindow, DockId )
			  .read( ImGuiWindow, DockTabItemStatusFlags )
			  .read( ImGuiWindow, DockTabItemRect )
			  .read( ImGuiWindow, DockOrder )
			  //.read( ImGuiWindow, DockIsActive )
			  //.read( ImGuiWindow, DockTabIsVisible )
			  //.read( ImGuiWindow, DockTabWantClose )
			  .read( ImGuiWindow, NavLastIds )
			  .read( ImGuiWindow, NavRectRel )
			  .method( ImGuiWindow, Rect )
			  .method( ImGuiWindow, CalcFontSize )
			  .method( ImGuiWindow, TitleBarHeight )
			  .method( ImGuiWindow, TitleBarRect )
			  .method( ImGuiWindow, MenuBarHeight )
			  .method( ImGuiWindow, MenuBarRect );

			class1( ImGuiContext, ImFontAtlas* )
			  .read( ImGuiContext, Initialized )
			  .read( ImGuiContext, WithinFrameScope )
			  .read( ImGuiContext, WithinFrameScopeWithImplicitWindow )
			  .read( ImGuiContext, FontAtlasOwnedByContext )
			  .read( ImGuiContext, IO )
			  .read( ImGuiContext, PlatformIO )
			  .read( ImGuiContext, Style )
			  .read( ImGuiContext, ConfigFlagsCurrFrame )
			  .read( ImGuiContext, ConfigFlagsLastFrame )
			  .read( ImGuiContext, Font )
			  .read( ImGuiContext, FontSize )
			  .read( ImGuiContext, FontBaseSize )
			  .read( ImGuiContext, DrawListSharedData )
			  .read( ImGuiContext, Time )
			  .read( ImGuiContext, FrameCount )
			  .read( ImGuiContext, FrameCountEnded )
			  .read( ImGuiContext, FrameCountPlatformEnded )
			  .read( ImGuiContext, FrameCountRendered )
			  .read( ImGuiContext, Windows )
			  .read( ImGuiContext, WindowsFocusOrder )
			  .read( ImGuiContext, WindowsTempSortBuffer )
			  .read( ImGuiContext, CurrentWindowStack )
			  .read( ImGuiContext, WindowsById )
			  .read( ImGuiContext, WindowsActiveCount )
			  .read( ImGuiContext, CurrentWindow )
			  .read( ImGuiContext, HoveredWindow )
			  .read( ImGuiContext, HoveredWindowUnderMovingWindow )
			  .read( ImGuiContext, HoveredId )
			  .read( ImGuiContext, HoveredIdAllowOverlap )
			  .read( ImGuiContext, HoveredIdPreviousFrame )
			  .read( ImGuiContext, HoveredIdTimer )
			  .read( ImGuiContext, HoveredIdNotActiveTimer )
			  .read( ImGuiContext, ActiveId )
			  .read( ImGuiContext, ActiveIdPreviousFrame )
			  .read( ImGuiContext, ActiveIdIsAlive )
			  .read( ImGuiContext, ActiveIdTimer )
			  .read( ImGuiContext, ActiveIdIsJustActivated )
			  .read( ImGuiContext, ActiveIdAllowOverlap )
			  .read( ImGuiContext, ActiveIdHasBeenPressedBefore )
			  .read( ImGuiContext, ActiveIdHasBeenEditedBefore )
			  .read( ImGuiContext, ActiveIdPreviousFrameIsAlive )
			  .read( ImGuiContext, ActiveIdPreviousFrameHasBeenEditedBefore )
			  .read( ImGuiContext, ActiveIdUsingNavDirMask )
			  .read( ImGuiContext, ActiveIdUsingNavInputMask )
			  .read( ImGuiContext, ActiveIdClickOffset )
			  .read( ImGuiContext, ActiveIdWindow )
			  .read( ImGuiContext, ActiveIdPreviousFrameWindow )
			  .read( ImGuiContext, ActiveIdSource )
			  .read( ImGuiContext, LastActiveId )
			  .read( ImGuiContext, LastActiveIdTimer )
			  .read( ImGuiContext, MouseLastValidPos )
			  .read( ImGuiContext, MovingWindow )
			  .read( ImGuiContext, ColorStack )
			  .read( ImGuiContext, StyleVarStack )
			  .read( ImGuiContext, FontStack )
			  .read( ImGuiContext, OpenPopupStack )
			  .read( ImGuiContext, BeginPopupStack )
			  .read( ImGuiContext, NextWindowData )
			  .read( ImGuiContext, Viewports )
			  .read( ImGuiContext, CurrentViewport )
			  .read( ImGuiContext, MouseViewport )
			  .read( ImGuiContext, MouseLastHoveredViewport )
			  //.read( ImGuiContext, ViewportFrontMostStampCount )
			  //.read( ImGuiContext, ViewportFrontMostStampCount )
			  .read( ImGuiContext, NavWindow )
			  .read( ImGuiContext, NavId )
			  .read( ImGuiContext, NavActivateId )
			  .read( ImGuiContext, NavActivateDownId )
			  .read( ImGuiContext, NavActivatePressedId )
			  .read( ImGuiContext, NavActivateInputId )
			  //.read( ImGuiContext, NavJustTabbedId )
			  .read( ImGuiContext, NavJustMovedToId )
			  .read( ImGuiContext, NavNextActivateId )
			  .read( ImGuiContext, NavInputSource )
			  .read( ImGuiContext, NavScoringRect )
			  .read( ImGuiContext, NavScoringDebugCount )
			  .read( ImGuiContext, NavWindowingTarget )
			  .read( ImGuiContext, NavWindowingTargetAnim )
			  .read( ImGuiContext, NavWindowingListWindow )
			  .read( ImGuiContext, NavWindowingTimer )
			  .read( ImGuiContext, NavWindowingHighlightAlpha )
			  .read( ImGuiContext, NavWindowingToggleLayer )
			  .read( ImGuiContext, NavLayer )
			  //.read( ImGuiContext, NavIdTabCounter )
			  .read( ImGuiContext, NavIdIsAlive )
			  .read( ImGuiContext, NavMousePosDirty )
			  .read( ImGuiContext, NavDisableHighlight )
			  .read( ImGuiContext, NavDisableMouseHover )
			  .read( ImGuiContext, NavAnyRequest )
			  .read( ImGuiContext, NavInitRequest )
			  .read( ImGuiContext, NavInitRequestFromMove )
			  .read( ImGuiContext, NavInitResultId )
			  .read( ImGuiContext, NavInitResultRectRel )
			  //.read( ImGuiContext, NavMoveRequest )
			  //.read( ImGuiContext, NavMoveRequestFlags )
			  //.read( ImGuiContext, NavMoveRequestForward )
			  .read( ImGuiContext, NavMoveDir )
			  //.read( ImGuiContext, NavMoveDirLast )
			  .read( ImGuiContext, NavMoveClipDir )
			  .read( ImGuiContext, NavMoveResultLocal )
			  //.read( ImGuiContext, NavMoveResultLocalVisibleSet )
			  .read( ImGuiContext, NavMoveResultOther )
			  .read( ImGuiContext, DimBgRatio )
			  .read( ImGuiContext, MouseCursor )
			  .read( ImGuiContext, DragDropActive )
			  .read( ImGuiContext, DragDropWithinSource )
			  .read( ImGuiContext, DragDropWithinTarget )
			  .read( ImGuiContext, DragDropSourceFlags )
			  .read( ImGuiContext, DragDropSourceFrameCount )
			  .read( ImGuiContext, DragDropMouseButton )
			  .read( ImGuiContext, DragDropPayload )
			  .read( ImGuiContext, DragDropTargetRect )
			  .read( ImGuiContext, DragDropTargetId )
			  .read( ImGuiContext, DragDropAcceptFlags )
			  .read( ImGuiContext, DragDropAcceptIdCurrRectSurface )
			  .read( ImGuiContext, DragDropAcceptIdCurr )
			  .read( ImGuiContext, DragDropAcceptIdPrev )
			  .read( ImGuiContext, DragDropAcceptFrameCount )
			  .read( ImGuiContext, DragDropPayloadBufHeap )
			  .read( ImGuiContext, DragDropPayloadBufLocal )
			  .read( ImGuiContext, TabBars )
			  .read( ImGuiContext, CurrentTabBar )
			  .read( ImGuiContext, CurrentTabBarStack )
			  .read( ImGuiContext, InputTextState )
			  .read( ImGuiContext, InputTextPasswordFont )
			  .read( ImGuiContext, ColorEditOptions )
			  .read( ImGuiContext, ColorPickerRef )
			  .read( ImGuiContext, DragCurrentAccumDirty )
			  .read( ImGuiContext, DragCurrentAccum )
			  .read( ImGuiContext, DragSpeedDefaultRatio )
			  .read( ImGuiContext, ScrollbarClickDeltaToGrabCenter )
			  .read( ImGuiContext, TooltipOverrideCount )
			  .read( ImGuiContext, ClipboardHandlerData )
			  .read( ImGuiContext, MenusIdSubmittedThisFrame )
			  //.read( ImGuiContext, PlatformImePos )
			  //.read( ImGuiContext, PlatformImeLastPos )
			  //.read( ImGuiContext, PlatformImePosViewport )
			  //.read( ImGuiContext, DockContext )
			  .read( ImGuiContext, SettingsLoaded )
			  .read( ImGuiContext, SettingsDirtyTimer )
			  .read( ImGuiContext, SettingsIniData )
			  .read( ImGuiContext, SettingsHandlers )
			  .read( ImGuiContext, SettingsWindows )
			  .read( ImGuiContext, LogEnabled )
			  .read( ImGuiContext, LogType )
			  .read( ImGuiContext, LogFile )
			  .read( ImGuiContext, LogBuffer )
			  .read( ImGuiContext, LogLinePosY )
			  .read( ImGuiContext, LogLineFirstItem )
			  .read( ImGuiContext, LogDepthRef )
			  .read( ImGuiContext, LogDepthToExpand )
			  .read( ImGuiContext, LogDepthToExpandDefault )
			  .read( ImGuiContext, FramerateSecPerFrame )
			  .read( ImGuiContext, FramerateSecPerFrameIdx )
			  .read( ImGuiContext, FramerateSecPerFrameAccum )
			  .read( ImGuiContext, WantCaptureMouseNextFrame )
			  .read( ImGuiContext, WantCaptureKeyboardNextFrame )
			  .read( ImGuiContext, WantTextInputNextFrame )
			  .read( ImGuiContext, TempBuffer );

			class1( ImGuiIO )
			  .read( ImGuiIO, ConfigFlags )
			  .read( ImGuiIO, BackendFlags )
			  .read( ImGuiIO, DisplaySize )
			  .read( ImGuiIO, DeltaTime )
			  .read( ImGuiIO, IniSavingRate )
			  .write( ImGuiIO, IniFilename )
			  .write( ImGuiIO, LogFilename )
			  .write( ImGuiIO, MouseDoubleClickTime )
			  .write( ImGuiIO, MouseDoubleClickMaxDist )
			  .write( ImGuiIO, MouseDragThreshold )
			  //.read( ImGuiIO, KeyMap )
			  .write( ImGuiIO, KeyRepeatDelay )
			  .write( ImGuiIO, KeyRepeatRate )
			  .read( ImGuiIO, UserData )
			  .read( ImGuiIO, Fonts )
			  .write( ImGuiIO, FontGlobalScale )
			  .write( ImGuiIO, FontAllowUserScaling )
			  .write( ImGuiIO, FontDefault )
			  .read( ImGuiIO, DisplayFramebufferScale )
			  .write( ImGuiIO, ConfigDockingNoSplit )
			  .write( ImGuiIO, ConfigDockingAlwaysTabBar )
			  .write( ImGuiIO, ConfigDockingTransparentPayload )
			  .write( ImGuiIO, ConfigViewportsNoAutoMerge )
			  .write( ImGuiIO, ConfigViewportsNoTaskBarIcon )
			  .write( ImGuiIO, ConfigViewportsNoDecoration )
			  .write( ImGuiIO, ConfigViewportsNoDefaultParent )
			  .write( ImGuiIO, MouseDrawCursor )
			  .read( ImGuiIO, ConfigMacOSXBehaviors )
			  .read( ImGuiIO, ConfigInputTextCursorBlink )
			  .read( ImGuiIO, ConfigWindowsResizeFromEdges )
			  .read( ImGuiIO, ConfigWindowsMoveFromTitleBarOnly )
			  .write( ImGuiIO, BackendPlatformName )
			  .write( ImGuiIO, BackendRendererName )
			  .write( ImGuiIO, BackendPlatformUserData )
			  .write( ImGuiIO, BackendLanguageUserData )
			  .read( ImGuiIO, ClipboardUserData )
			  .read( ImGuiIO, MousePos )
			  .read( ImGuiIO, MouseDown )
			  .read( ImGuiIO, MouseWheel )
			  .read( ImGuiIO, MouseWheelH )
			  .read( ImGuiIO, MouseHoveredViewport )
			  .read( ImGuiIO, KeyCtrl )
			  .read( ImGuiIO, KeyShift )
			  .read( ImGuiIO, KeyAlt )
			  .read( ImGuiIO, KeySuper )
			  //.read( ImGuiIO, KeysDown )
			  .read( ImGuiIO, NavInputs )
			  .read( ImGuiIO, WantCaptureMouse )
			  .read( ImGuiIO, WantCaptureKeyboard )
			  .read( ImGuiIO, WantTextInput )
			  .read( ImGuiIO, WantSetMousePos )
			  .read( ImGuiIO, WantSaveIniSettings )
			  .read( ImGuiIO, NavActive )
			  .read( ImGuiIO, NavVisible )
			  .read( ImGuiIO, Framerate )
			  .read( ImGuiIO, MetricsRenderVertices )
			  .read( ImGuiIO, MetricsRenderIndices )
			  .read( ImGuiIO, MetricsRenderWindows )
			  .read( ImGuiIO, MetricsActiveWindows )
			  .read( ImGuiIO, MetricsActiveAllocations )
			  .read( ImGuiIO, MouseDelta )
			  .read( ImGuiIO, MousePosPrev )
			  .read( ImGuiIO, MouseClickedPos )
			  .read( ImGuiIO, MouseClickedTime )
			  .read( ImGuiIO, MouseClicked )
			  .read( ImGuiIO, MouseDoubleClicked )
			  .read( ImGuiIO, MouseReleased )
			  .read( ImGuiIO, MouseDownOwned )
			  .read( ImGuiIO, MouseDownDuration )
			  .read( ImGuiIO, MouseDownDurationPrev )
			  .read( ImGuiIO, MouseDragMaxDistanceAbs )
			  .read( ImGuiIO, MouseDragMaxDistanceSqr )
			  //.read( ImGuiIO, KeysDownDuration )
			  //.read( ImGuiIO, KeysDownDurationPrev )
			  .read( ImGuiIO, NavInputsDownDuration )
			  .read( ImGuiIO, NavInputsDownDurationPrev )
			  .read( ImGuiIO, InputQueueCharacters );

			class1( ImFontAtlas )
			  .method( ImFontAtlas, AddFont )
			  .method( ImFontAtlas, AddFontDefault )
			  .method( ImFontAtlas, AddFontFromFileTTF )
			  .method( ImFontAtlas, AddFontFromMemoryTTF )
			  .method( ImFontAtlas, AddFontFromMemoryCompressedTTF )
			  .method( ImFontAtlas, AddFontFromMemoryCompressedBase85TTF )
			  .method( ImFontAtlas, ClearInputData )
			  .method( ImFontAtlas, ClearTexData )
			  .method( ImFontAtlas, ClearFonts )
			  .method( ImFontAtlas, Clear )
			  .method( ImFontAtlas, Build )
			  //.method( ImFontAtlas, GetTexDataAsAlpha8 )
			  //.method( ImFontAtlas, GetTexDataAsRGBA32 )
			  .method( ImFontAtlas, IsBuilt )
			  .method( ImFontAtlas, SetTexID )
			  .method( ImFontAtlas, GetGlyphRangesDefault )
			  .method( ImFontAtlas, GetGlyphRangesKorean )
			  .method( ImFontAtlas, GetGlyphRangesJapanese )
			  .method( ImFontAtlas, GetGlyphRangesChineseFull )
			  .method( ImFontAtlas, GetGlyphRangesChineseSimplifiedCommon )
			  .method( ImFontAtlas, GetGlyphRangesCyrillic )
			  .method( ImFontAtlas, GetGlyphRangesThai )
			  .method( ImFontAtlas, GetGlyphRangesVietnamese )
			  .read( ImFontAtlas, Locked )
			  .read( ImFontAtlas, Flags )
			  .read( ImFontAtlas, TexID )
			  .read( ImFontAtlas, TexDesiredWidth )
			  .read( ImFontAtlas, TexGlyphPadding )
			  .read( ImFontAtlas, TexPixelsAlpha8 )
			  .read( ImFontAtlas, TexPixelsRGBA32 )
			  .read( ImFontAtlas, TexWidth )
			  .read( ImFontAtlas, TexHeight )
			  .read( ImFontAtlas, TexUvScale )
			  .read( ImFontAtlas, TexUvWhitePixel )
			  .read( ImFontAtlas, Fonts )
			  .read( ImFontAtlas, CustomRects )
			  .read( ImFontAtlas, ConfigData );
			  //.read( ImFontAtlas, CustomRectIds );

			class1( ImGuiStyle )
			  .write( ImGuiStyle,
					  Alpha )					  // Global alpha applies to everything in ImGui.
			  .write( ImGuiStyle, WindowPadding ) // Padding within a window.
			  .write( ImGuiStyle,
					  WindowRounding ) // Radius of window corners rounding. Set
									   // to 0.0f to have rectangular windows.
			  .write( ImGuiStyle,
					  WindowBorderSize ) // Thickness of border around windows.
										 // Generally set to 0.0f or 1.0f.
										 // (Other values are not well tested
										 // and more CPU/GPU costly).
			  .write( ImGuiStyle,
					  WindowMinSize ) // Minimum window size. This is a global
									  // setting. If you want to constraint
									  // individual windows, use
									  // SetNextWindowSizeConstraints().
			  .write( ImGuiStyle,
					  WindowTitleAlign ) // Alignment for title bar text.
										 // Defaults to (0.0f,0.5f) for
										 // left-aligned,vertically centered.
			  .write( ImGuiStyle,
					  ChildRounding ) // Radius of child window corners
									  // rounding. Set to 0.0f to have
									  // rectangular windows.
			  .write( ImGuiStyle,
					  ChildBorderSize ) // Thickness of border around child
										// windows. Generally set to 0.0f
										// or 1.0f. (Other values are not well
										// tested and more CPU/GPU costly).
			  .write( ImGuiStyle,
					  PopupRounding ) // Radius of popup window corners
									  // rounding. (Note that tooltip windows
									  // use WindowRounding)
			  .write( ImGuiStyle,
					  PopupBorderSize ) // Thickness of border around
										// popup/tooltip windows. Generally set
										// to 0.0f or 1.0f. (Other values are
										// not well tested and more CPU/GPU
										// costly).
			  .write( ImGuiStyle,
					  FramePadding ) // Padding within a framed rectangle
									 // (used by most widgets).
			  .write( ImGuiStyle,
					  FrameRounding ) // Radius of frame corners rounding. Set
									  // to 0.0f to have rectangular frame
									  // (used by most widgets).
			  .write( ImGuiStyle,
					  FrameBorderSize ) // Thickness of border around frames.
										// Generally set to 0.0f or 1.0f.
										// (Other values are not well tested
										// and more CPU/GPU costly).
			  .write( ImGuiStyle,
					  ItemSpacing ) // Horizontal and vertical spacing
			  // between widgets/lines.
			  .write( ImGuiStyle,
					  ItemInnerSpacing ) // Horizontal and vertical spacing
										 // between within elements of a
										 // composed widget (e.g. a slider and
										 // its label).
			  .write( ImGuiStyle,
					  TouchExtraPadding ) // Expand reactive bounding box for
										  // touch-based system where touch
										  // position is not accurate enough.
										  // Unfortunately we don't sort widgets
										  // so priority on overlap will always
										  // be given to the first widget. So
										  // don't grow this too much!
			  .write( ImGuiStyle,
					  IndentSpacing ) // Horizontal indentation when e.g.
									  // entering a tree node. Generally ==
									  // (FontSize + FramePadding.x*2).
			  .write( ImGuiStyle,
					  ColumnsMinSpacing ) // Minimum horizontal spacing
										  // between two columns.
			  .write( ImGuiStyle,
					  ScrollbarSize ) // Width of the vertical scrollbar,
									  // Height of the horizontal scrollbar.
			  .write( ImGuiStyle,
					  ScrollbarRounding ) // Radius of grab corners for
										  // scrollbar.
			  .write( ImGuiStyle,
					  GrabMinSize ) // Minimum width/height of a grab
			  // box for slider/scrollbar.
			  .write( ImGuiStyle,
					  GrabRounding ) // Radius of grabs corners rounding. Set to
									 // 0.0f to have rectangular slider grabs.
			  .write( ImGuiStyle,
					  TabRounding ) // Radius of upper corners of a tab. Set
			  // to 0.0f to have rectangular tabs.
			  .write( ImGuiStyle,
					  TabBorderSize ) // Thickness of border around tabs.
			  .write( ImGuiStyle,
					  ButtonTextAlign ) // Alignment of button text when button
										// is larger than text. Defaults to
										// (0.5f, 0.5f) (centered).
			  .write( ImGuiStyle,
					  SelectableTextAlign ) // Alignment of selectable text
											// when selectable is larger than
											// text. Defaults to (0.0f, 0.0f)
											// (top-left aligned).
			  .write( ImGuiStyle,
					  DisplayWindowPadding ) // Window position are clamped to
											 // be visible within the display
											 // area or monitors by at least
											 // this amount. Only applies to
											 // regular windows.
			  .write( ImGuiStyle,
					  DisplaySafeAreaPadding ) // If you cannot see the edges of
											   // your screen (e.g. on a TV)
											   // increase the safe area
											   // padding. Apply to
											   // popups/tooltips as well
											   // regular windows. NB: Prefer
											   // configuring your TV sets
											   // correctly!
			  .write( ImGuiStyle,
					  MouseCursorScale ) // Scale software rendered mouse
										 // cursor (when io.MouseDrawCursor is
										 // enabled). May be removed later.
			  .write( ImGuiStyle,
					  AntiAliasedLines ) // Enable anti-aliasing on
										 // lines/borders. Disable if you are
										 // really tight on CPU/GPU.
			  .write( ImGuiStyle,
					  AntiAliasedFill ) // Enable anti-aliasing on filled shapes
										// (rounded rectangles, circles, etc.)
			  .write( ImGuiStyle,
					  CurveTessellationTol ) // Tessellation tolerance when
											 // using PathBezierCurveTo()
											 // without a specific number of
											 // segments. Decrease for highly
											 // tessellated curves (higher
											 // quality, more polygons),
											 // increase to reduce quality.
			  .read( ImGuiStyle, Colors );

			/*
				BEGIN IMGUI

					Reasons why a function might be commented out /
			   unimplemented:

						DC: Doesn't compile, will need custom implementation
						NN: Not needed, handled by backed
						NR: Not ready, waiting on more backend to be implemented
						OF: Obsolete function, no need to support it
			*/
			mod.doc() = "ImGui Python Binding using pybind11";
#include "ImGuiModule.inc"
		}
	} // namespace Modules
} // namespace Python
#include "Visual/Python/pybind11/pybind11_undef.h"
#endif //PYTHON_ENABLED
