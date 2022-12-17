#pragma once

#include <functional>
#include <initializer_list>
#include <optional>
#include <string_view>

#ifndef GGG_CUSTOM
#include "imgui.h"
#include "imgui_internal.h"
#endif

#include "Common/Utility/Finally.h"
#include "Common/Utility/StringManipulation.h"

enum
{
    ImGuiWindowFlags_NoWindow = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove,
};

namespace ImGuiEx
{
    using ListCallback_t = std::function<bool( int, const char** )>;
    using SelectableCallback_t = std::function<void( int idx, const char* )>;

    // Overrides SetNextWindowPos to be relative to the main viewport
    void SetNextWindowPosRelative( const ImVec2& pos, ImGuiCond cond = 0, const ImVec2 & pivot = ImVec2( 0, 0 ) );

    class Box
    {
        Box();
        static Box& GetBoxBuilder();
        void Reset();
        bool Validate();

        ListCallback_t list_callback;
        SelectableCallback_t selectable_callback;
        bool( *free_function )( void* data, int idx, const char** out_text );
        void* function_data = nullptr;
        int items_count = -1;
        int height_in_items = -1;
        ImVec2 size;

    public:
        // FINALIZE
        static Box& StartBox();

        Box& WithFunction( bool( *function )( void* data, int idx, const char** out_text ), void* function_data, int items_count );
        Box& WithCallback( ListCallback_t callback, int items_count );
        Box& WithVector( std::vector<std::string>* items );
        Box& WithArray( const char* items[], int items_count );

        Box& WithSelectableCallback( SelectableCallback_t callback );

        Box& WithSize( int height_in_items );
        Box& WithSize( const ImVec2& size );

        bool ShowComboBox( const std::string_view label, int* current_item );
        bool ShowListBox( const std::string_view label, int* current_item );
    };

    // Similar to CollapsingHeader, but does not expand/collapse
    void TextHeader( const std::string_view text, const ImGuiCol_ color = ImGuiCol_Header );

    // Overrides ListBoxHeader to ensure height_in_items evaluates to a consistent height for all item_counts
    bool ListBoxHeader( const std::string_view label, int items_count, int height_in_items = -1 );

    // Extend the existing button function to behave better
    bool Button( const std::string_view label, bool enabled = true, const ImVec2& size_arg = ImVec2( 0, 0 ), ImGuiButtonFlags flags = 0 );
    bool SmallButton( const std::string_view label, bool enabled = true, ImGuiButtonFlags flags = 0 );

    // Push item width with a text parameter (calculates text size)
    void PushItemWidthText( const std::string_view text, const float additional = 0.0f );

    // Wraps checkbox so that you don't need a pointer ( you will need to infer the check value manually )
    bool Checkbox( const std::string_view label, bool checked );

	// Checkbox with a third option, displayed when the optional is nullopt (no value)
	bool CheckboxTristate( const std::string_view, std::optional< bool >& tristate_value );

    //Similar to InputText, except this automatically resizes the string as the user types
    bool InputString( const std::string_view, std::string& str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr );
    bool InputStringMultiline( const std::string_view, std::string& str, const ImVec2& size, ImGuiInputTextFlags flags = 0 );

    //Convenience functions for creating Combos from vectors
    bool Combo( const std::string_view label, int* index, const std::vector< std::string >& items );
    bool Combo( const std::string_view label, int* index, const std::vector< const char* >& items );

    void TextUnformatted( const std::string_view text );

    // Wraps InputText with std::string
    bool InputText( const std::string_view label, std::string& str );
    bool InputText( const std::string_view label, std::string& str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data = nullptr );
    bool InputText( const std::string_view label, std::string& buffer, unsigned buff_size, const char* hint = nullptr, const ImVec2& size = ImVec2( 0, 0 ), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr );
    bool InputTextMultiline( const std::string_view label, std::string& buffer, unsigned buff_size, const ImVec2& size = ImVec2( 0, 0 ), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr );

    // Make a read-only InputText with std::string
    void InputTextReadOnly( const std::string_view label, const std::string& buffer, const ImVec2& size = ImVec2( 0, 0 ) );
    void InputIntReadOnly( const std::string_view label, int i, const ImVec2& size = ImVec2( 0, 0 ) );
    void InputFloatReadOnly( const std::string_view label, float f, const char* format, const ImVec2& size = ImVec2( 0, 0 ) );

    // Multi-component widgets with different format strings
    bool DragScalarN( const std::string_view label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min = nullptr, const void* p_max = nullptr, const char* formats[] = nullptr, ImGuiSliderFlags flags = 0 );
    bool DragInt2( const std::string_view label, int v[ 2 ], float v_speed, int v_min, int v_max, const char* formats[ 2 ] );
    bool DragInt3( const std::string_view label, int v[ 3 ], float v_speed, int v_min, int v_max, const char* formats[ 3 ] );
    bool DragFloat2( const std::string_view label, float v[ 2 ], float v_speed, float v_min, float v_max, const char* formats[ 2 ], ImGuiSliderFlags flags = 0 );
    bool DragFloat3( const std::string_view label, float v[ 3 ], float v_speed, float v_min, float v_max, const char* formats[ 3 ], ImGuiSliderFlags flags = 0 );

    // Helper to display a little (?) mark which shows a tooltip when hovered.
    void HelpMarker( const std::string_view tooltip, const std::string_view title = {} );
    void SetItemTooltip( const std::string_view tooltip, const std::string_view title = {}, ImGuiHoveredFlags hovered_flags = ImGuiHoveredFlags_AllowWhenDisabled );

    // Optimize scrollable frames with this
    using ClipperFunction = std::function<void( int )>;
    void Clipper( int items_count, ImGuiEx::ClipperFunction clip_func );

    // Optimized versions of Combo/Listbox that use the imgui item clipper. Use this to manually roll your own boxes
    void Combo( const std::string_view label, const char* preview, const int item_count, ImGuiComboFlags flags, ClipperFunction func );
    void ListBox( const std::string_view label, int items_count, int height_in_items, ClipperFunction func );
    void ListBox( const std::string_view label, int items_count, const ImVec2& size, ClipperFunction func );
    void ToolTip( const std::string_view label, const std::string_view description = {} );

    bool Splitter( bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f );

    struct ScopedGuard
    {
      ScopedGuard(std::function<void()> begin_func, std::function<void()> end_func) : begin_func(begin_func), end_func(end_func)
      {
        begin_func();
      }
      ~ScopedGuard()
      {
        end_func();
      }
    private:
      std::function<void()> begin_func, end_func;
    };

    Finally Begin( const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0 );
    Finally BeginGroup();
    Finally BeginChild( const char* str_id, const ImVec2& size = ImVec2( 0, 0 ), bool border = false, ImGuiWindowFlags flags = 0 );
    Finally BeginChildFrame( const char* str_id, const ImVec2& size, ImGuiWindowFlags flags = 0 );
    Finally BeginMainMenuBar();
    Finally BeginMenuBar();
    Finally BeginMenu( const std::string_view label, const bool enabled = true );
    Finally BeginDragDropSource( ImGuiDragDropFlags flags = 0 );
    Finally BeginDragDropTarget();
    Finally BeginPopupModal( const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0 );
    Finally BeginPopup( const char* name, ImGuiWindowFlags flags = 0 );

    std::unique_ptr<Finally> Indent( const float width = 0.0f );
    std::unique_ptr<Finally> PushID( const std::string_view str_id );
    std::unique_ptr<Finally> PushID( const char* str_id_begin, const char* str_id_end = nullptr );
    std::unique_ptr<Finally> PushID( const void* ptr );
    std::unique_ptr<Finally> PushID( const int id );
    std::unique_ptr<Finally> PushStyleColour( const ImGuiCol type, const ImU32 colour );
    std::unique_ptr<Finally> PushStyleColours( std::initializer_list<std::pair<ImGuiCol, ImU32>> list );
    std::unique_ptr<Finally> PushItemWidth( const float width );
    std::unique_ptr<Finally> TreeNodeEx( const std::string_view id, const std::string_view label, bool* p_open, ImGuiTreeNodeFlags flags = 0 );
    std::unique_ptr<Finally> TreeNode( const std::string_view label, bool* p_open, ImGuiTreeNodeFlags flags = 0 );
    std::unique_ptr<Finally> TreeNode( const std::string_view label, ImGuiTreeNodeFlags flags );
    std::unique_ptr<Finally> TreeNode( const char* str_id, const char* fmt, ... );
    std::unique_ptr<Finally> TreeNode( const void* ptr_id, const char* fmt, ... );
    std::unique_ptr<Finally> TreeNode( const std::string_view label );
    std::unique_ptr<Finally> Columns( const int count );

    std::unique_ptr<Finally> BeginColumns( const char* id, const int count, ImGuiOldColumnFlags flags = 0 );

    template< typename T >
    std::unique_ptr<Finally > PushStyleVar( const ImGuiStyleVar idx, const T value )
    {
        ImGui::PushStyleVar( idx, value );
        return make_finally( &ImGui::PopStyleVar, 1 );
    }

    bool CollapsingHeader( const char* id, const char* text, ImGuiTreeNodeFlags flags = 0 );

    void PushDisable( const bool disable = true );
    void PopDisable();

    // Trims text from the left to fit the available space (width - padding), prepending "..." if any characters were removed.
    std::string TrimTextFromLeft( const std::string& text, const bool advance_cursor, const float width = ImGui::CalcItemWidth(), const float padding = ImGui::GetStyle().FramePadding.x * 2 );

    template< typename... Args >
    void Text( const std::wstring_view wfmt, Args&&... args )
    {
        const std::string fmt = WstringToString( wfmt );
        ImGui::Text( fmt.data(), std::forward< Args >( args )... );
    }

    template< typename... Args >
    void Text( const std::string_view fmt, Args&&... args )
    {
        ImGui::Text( fmt.data(), std::forward< Args >( args )... );
    }

    template< typename... Args >
    void SetTooltip( const std::wstring_view wfmt, Args&&... args )
    {
        const std::string fmt = WstringToString( wfmt );
        ImGui::SetTooltip( fmt.data(), std::forward< Args >( args )... );
    }

    template< typename... Args >
    void SetTooltip( const std::string_view fmt, Args&&... args )
    {
        ImGui::SetTooltip( fmt.data(), std::forward< Args >( args )... );
    }

    // Colour Utility
    ImU32 GetStyleColor( ImGuiCol colour );

    constexpr ImU32 Alpha( unsigned char value ) { return (ImU32)value << 24; }
    constexpr ImU32   Red( unsigned char value ) { return (ImU32)value << 16; }
    constexpr ImU32 Green( unsigned char value ) { return (ImU32)value << 8; }
    constexpr ImU32  Blue( unsigned char value ) { return (ImU32)value; }
    constexpr ImU32  ARGB( unsigned char A, unsigned char R, unsigned char G, unsigned char B ) { return Alpha( A ) | Red( R ) | Green( G ) | Blue( B ); }

    constexpr ImU32  ARGB( float A, float R, float G, float B )
    {
        return Alpha( static_cast<unsigned char>( A * 255 ) )
             |   Red( static_cast<unsigned char>( R * 255 ) )
             | Green( static_cast<unsigned char>( G * 255 ) )
             |  Blue( static_cast<unsigned char>( B * 255 ) );
    }

	// ColorEdit that automatically chooses ColorEdit3 or ColorEdit4 depending on dimensions
	template<unsigned N>
	bool ColorEdit(const std::string_view label, float col[N], ImGuiColorEditFlags flags = 0)
	{
		bool res = false;
		if (N == 3)
			res = ImGui::ColorEdit3(label.data(), col, flags);
		else if (N == 4)
			res = ImGui::ColorEdit4(label.data(), col, flags);
	
		return res;
	}
	
	// ColorEdit that outputs the linearized version of the selected color for gamma correction
	template<unsigned N>
	bool ColorEditSRGB(const std::string_view label, float linear_col[N], ImGuiColorEditFlags flags = 0)
	{
		if (N < 3)
			return false;
	
        float srgb_col[ N ] = {};
		for (int i = 0; i < N; i++)
			srgb_col[i] = std::pow(std::max(0.0f, linear_col[i]), 1.0f / 2.2f);
	
		const bool res = ColorEdit<N>(label, srgb_col, flags);
	
		for (int i = 0; i < N; i++)
			linear_col[i] = std::pow(std::max(0.0f, srgb_col[i]), 2.2f);
	
		return res;
	}

	void StyleColorsGemini( ImGuiStyle* dst = nullptr );

    class PopupHandler
    {
        struct Popup
        {
            std::function<bool()> func;
            std::string id;
            ImGuiWindowFlags flags;
            bool modal;
            bool opened;
        };

        std::vector<Popup> popups;

    public:
        void AddPopup( const std::string& id, const bool modal, const ImGuiWindowFlags flags, std::function<bool()> func );
        void Render();
    };

    template<typename A, size_t N> class PlotData
    {
    private:
        A data[N];
        size_t start = 0;
        size_t count = 0;

    public:
        PlotData() noexcept {}

        template<typename... Args>
        A& Push(Args&&... args)
        {
            const auto i = (start + count) % N;
            data[i] = A(std::forward<Args>(args)...);
            if (count < N)
                count++;
            else
                start++;

            return data[i];
        }

        const A* Data() const { return &data[0]; }
        int Offset() const { return int(start); }
        int Count() const { return int(count); }

        const A& Last() const { return data[(start + count + N - 1) % N]; }

        void PlotLines(const std::string_view label, size_t data_offset = 0, const char* overlay_text = nullptr, float min_scale = FLT_MAX, float max_scale = FLT_MAX, const ImVec2& size = ImVec2(0, 0))
        {
            ImGui::PlotLines(label.data(), (float*)(uintptr_t(Data()) + data_offset), Count(), Offset(), overlay_text, min_scale, max_scale, size, sizeof(A));
        }
    };

    size_t FormatStringEllipsis(float max_width, bool ellipsis_at_end, char* buffer, size_t buffer_size);

    inline size_t FormatStringEllipsis(float max_width, bool ellipsis_at_end, char* buffer, size_t buffer_size, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        ImFormatStringV(buffer, buffer_size, fmt, args);
        va_end(args);
        return FormatStringEllipsis(max_width, ellipsis_at_end, buffer, buffer_size);
    }

    char* FormatMemory(char* buffer, size_t size, uint64_t value);
    template<size_t N> char* FormatMemory(char(&buffer)[N], uint64_t value) { return FormatMemory(buffer, N, value); }

    // Similar to ImGui::Text, but the text will scroll if running out of space
    void TextAnimated(const char* text, float time);

	// Similar to ImGui::VSliderScalar, except that it supports CTRL+Click to open an Test Input
	bool VSliderScalar(const std::string_view label, const ImVec2& size, ImGuiDataType data_type, void* v, const void* v_min, const void* v_max, const char* format = NULL, ImGuiSliderFlags flags = 0);

	inline bool VSliderFloat(const std::string_view label, const ImVec2& size, float* v, float v_min, float v_max, const char* format = NULL, ImGuiSliderFlags flags = 0)
	{
		if (*v != *v)
			*v = 0;

		return VSliderScalar(label, size, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
	}

	// Similar to ImGui::VDragScalar, but disables interaction if v_max <= v_min
	bool VDragScalar(const std::string_view label, const ImVec2& size, ImGuiDataType data_type, void* v, float v_speed, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags = 0, bool has_min_max = true);

	inline bool VDragFloat(const std::string_view label, const ImVec2& size, float* v, float v_speed, float v_min, float v_max, const char* format = NULL, ImGuiSliderFlags flags = 0)
	{
		if (*v != *v)
			*v = 0;

		return VDragScalar(label, size, ImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags, v_min < v_max);
	}

	// Same as ImGui::TreeNodeBehavior, but respects size constraits from PushItemWidth
	bool TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end = NULL);
    inline bool TreeNodeBehavior( ImGuiID id, ImGuiTreeNodeFlags flags, const std::string_view label ) { return TreeNodeBehavior( id, flags, label.data(), label.data() + label.size() ); }

	inline bool CollapsingHeader(const std::string_view label, ImGuiTreeNodeFlags flags = 0)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		return TreeNodeBehavior(window->GetID(label.data()), flags | ImGuiTreeNodeFlags_CollapsingHeader, label);
	}

    void PlotMultiHistogram(const std::string_view label, const float* values, int value_count, int row_count, const ImColor* row_colors, int color_count, int values_offset = 0, const char* tooltip_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0));

    template<size_t A, size_t B, size_t C> void PlotMultiHistogram(const std::string_view label, const float(&values)[A][B], const ImColor(&colors)[C], int values_offset = 0, const char* tooltip_text = NULL, float scale_min = FLT_MAX, float scale_max = FLT_MAX, ImVec2 graph_size = ImVec2(0, 0))
    {
        PlotMultiHistogram(label, (const float*)values, int(A) - values_offset, int(B), colors, int(C), values_offset, tooltip_text, scale_min, scale_max, graph_size);
    }

    bool ToggleButton(const char* label, bool* v);
}
