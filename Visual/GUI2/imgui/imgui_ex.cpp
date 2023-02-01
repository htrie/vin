#include "imgui_ex.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Common/Utility/ArrayOperations.h"
#include "Common/Utility/ContainerOperations.h"
#include "Visual/GUI2/Icons.h"
#include <functional>

namespace detail
{
    constexpr unsigned disabled_colour = 0xFF555555;

    // Getter for the old Combo() API: const char*[]
    static bool Items_ArrayGetter( void* data, int idx, const char** out_text )
    {
        const char* const* items = (const char* const*)data;
        if( out_text )
            * out_text = items[idx];
        return true;
    }

    static bool Callback_ArrayGetter( void* data, int idx, const char** out_text )
    {
        if( !data || !out_text )
            return false;

        auto& callback = *( ImGuiEx::ListCallback_t* ) data;
        return callback( idx, out_text );
    };

    static bool Vector_ArrayGetter( void* data, int idx, const char** out_text )
    {
        if( !data || !out_text )
            return false;

        auto& names = *( std::vector<std::string>* ) data;
        if( idx < 0 || idx >= (int)names.size() )
            return false;

        *out_text = names[idx].c_str();
        return true;
    };

    bool TreeNodeClose( const std::string_view id_str, const std::string_view label, bool* p_open, ImGuiTreeNodeFlags flags )
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if( window->SkipItems )
            return false;

        if( p_open && !*p_open )
            return false;

        const auto lock = ImGuiEx::PushID( id_str );

        const ImGuiID id = window->GetID( id_str.data(), id_str.data() + id_str.size() );
        flags |= ( p_open ? ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton : 0 );
        const bool is_open = ImGui::TreeNodeBehavior( id, flags, label.data(), label.data() + label.size() );
        if( p_open )
        {
            auto& style = ImGui::GetStyle();

            ImGuiContext& g = *GImGui;
            ImGuiLastItemData last_item_backup = g.LastItemData;
            float button_size = g.FontSize;
            float button_x = ImMax( g.LastItemData.Rect.Min.x, g.LastItemData.Rect.Max.x - g.Style.FramePadding.x * 2.0f - button_size );
            float button_y = g.LastItemData.Rect.Min.y;
            ImGuiID close_button_id = ImGui::GetIDWithSeed( "#CLOSE", NULL, id );
            if( ImGui::CloseButton( close_button_id, ImVec2( button_x, button_y ) ) )
                *p_open = false;
            g.LastItemData = last_item_backup;
        }

        return is_open;
    }
}

namespace
{
    // from imgui_widgets.cpp
    static const ImGuiDataTypeInfo GDataTypeInfo[] =
    {
        { sizeof( char ),             "%d",   "%d"    },  // ImGuiDataType_S8
        { sizeof( unsigned char ),    "%u",   "%u"    },
        { sizeof( short ),            "%d",   "%d"    },  // ImGuiDataType_S16
        { sizeof( unsigned short ),   "%u",   "%u"    },
        { sizeof( int ),              "%d",   "%d"    },  // ImGuiDataType_S32
        { sizeof( unsigned int ),     "%u",   "%u"    },
    #ifdef _MSC_VER
        { sizeof( ImS64 ),            "%I64d","%I64d" },  // ImGuiDataType_S64
        { sizeof( ImU64 ),            "%I64u","%I64u" },
    #else
        { sizeof( ImS64 ),            "%lld", "%lld"  },  // ImGuiDataType_S64
        { sizeof( ImU64 ),            "%llu", "%llu"  },
    #endif
        { sizeof( float ),            "%f",   "%f"    },  // ImGuiDataType_Float (float are promoted to double in va_arg)
        { sizeof( double ),           "%f",   "%lf"   },  // ImGuiDataType_Double
    };
    IM_STATIC_ASSERT( IM_ARRAYSIZE( GDataTypeInfo ) == ImGuiDataType_COUNT );

    struct InputTextCallback_UserData
    {
        std::string* Str;
        ImGuiInputTextCallback ChainCallback;
        void* ChainCallbackUserData;
    };
}

void ImGuiEx::Clipper( int items_count, ImGuiEx::ClipperFunction clip_func )
{
    ImGuiListClipper clipper( items_count );

    while( clipper.Step() )
    {
        for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
            clip_func( i );
    }
}

void ImGuiEx::SetNextWindowPosRelative( const ImVec2& pos, ImGuiCond cond, const ImVec2& pivot )
{
    ImGui::SetNextWindowPos( ImGui::GetMainViewport()->Pos + pos, cond, pivot );
}

bool ImGuiEx::Button( const std::string_view label, bool enabled, const ImVec2& size_arg, ImGuiButtonFlags flags )
{
    if( !enabled )
        ImGui::BeginDisabled();
    const bool result = ImGui::ButtonEx( label.data(), size_arg, flags );
    if( !enabled )
        ImGui::EndDisabled();
    return enabled ? result : false;
}

bool ImGuiEx::SmallButton( const std::string_view label, bool enabled, ImGuiButtonFlags flags )
{
    if( !enabled )
        ImGui::BeginDisabled();

    ImGuiContext& g = *GImGui;
    float backup_padding_y = g.Style.FramePadding.y;
    g.Style.FramePadding.y = 0.0f;
    const bool result = ImGui::ButtonEx( label.data(), ImVec2( 0, 0 ), ImGuiButtonFlags_AlignTextBaseLine | flags );
    g.Style.FramePadding.y = backup_padding_y;

    if( !enabled )
    {
        ImGui::EndDisabled();
        return false;
    }

    return result;
}

void ImGuiEx::PushItemWidthText( const std::string_view text, const float additional )
{
    ImGui::PushItemWidth( -( ImGui::CalcTextSize( text.data() ).x + ImGui::GetStyle().FramePadding.x * 2 + additional ) );
}

bool ImGuiEx::Checkbox( const std::string_view label, bool checked )
{
    return ImGui::Checkbox( label.data(), &checked );
}

bool ImGuiEx::CheckboxTristate( const std::string_view label, std::optional< bool >& tristate_value )
{
	bool result = false;

	if( !tristate_value )
	{
		ImGui::PushItemFlag( ImGuiItemFlags_MixedValue, true );

		bool temp = false;
		result = ImGui::Checkbox( label.data(), &temp );
		if( result )
			tristate_value = true;

		ImGui::PopItemFlag();
	}
	else
	{
		result = ImGui::Checkbox( label.data(), &*tristate_value );
	}

	if( result )
		assert( tristate_value );

	return result;
}

bool ImGuiEx::InputString( const std::string_view label, std::string& str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data )
{
    InputTextCallback_UserData cb_user_data{};
    cb_user_data.Str = &str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;

    return ImGui::InputText( label.data(), str.data(), str.capacity() + 1, flags | ImGuiInputTextFlags_CallbackResize, []( ImGuiInputTextCallbackData* data )
    {
        InputTextCallback_UserData* user_data = ( InputTextCallback_UserData* )data->UserData;
        if( data->EventFlag == ImGuiInputTextFlags_CallbackResize )
        {
            std::string* str = user_data->Str;
            IM_ASSERT( data->Buf == str->c_str() );
            str->resize( data->BufTextLen );
            data->Buf = str->data();
        }
        else if( user_data->ChainCallback )
        {
            // Forward to user callback, if any
            data->UserData = user_data->ChainCallbackUserData;
            return user_data->ChainCallback( data );
        }
        return 0;
    }, &cb_user_data );
}

bool ImGuiEx::InputStringMultiline( const std::string_view label, std::string& str, const ImVec2& size, ImGuiInputTextFlags flags )
{
    return ImGui::InputTextMultiline( label.data(), str.data(), str.capacity() + 1, size, flags | ImGuiInputTextFlags_CallbackResize, []( ImGuiInputTextCallbackData* data )
    {
        if( data->EventFlag == ImGuiInputTextFlags_CallbackResize )
        {
            std::string* str = ( std::string* )data->UserData;
            IM_ASSERT( data->Buf == str->c_str() );
            str->resize( data->BufTextLen );
            data->Buf = str->data();
        }
        return 0;
    }, &str );
}

bool ImGuiEx::InputStringWithHint( const std::string_view label, const std::string_view hint, std::string& str, ImGuiInputTextFlags flags )
{
    return ImGui::InputTextWithHint( label.data(), hint.data(), str.data(), str.capacity() + 1, flags | ImGuiInputTextFlags_CallbackResize, []( ImGuiInputTextCallbackData* data )
    {
        if( data->EventFlag == ImGuiInputTextFlags_CallbackResize )
        {
            std::string* str = ( std::string* )data->UserData;
            IM_ASSERT( data->Buf == str->c_str() );
            str->resize( data->BufTextLen );
            data->Buf = str->data();
        }
        return 0;
    }, &str );
}

bool ImGuiEx::Combo( const std::string_view label, int* index, const std::vector< std::string >& items )
{
    return ImGui::Combo( label.data(), index, []( void* data, int idx, const char** out_text )
    {
        const auto* vec = ( const std::vector< std::string >* )data;
        *out_text = vec->at( idx ).c_str();
        return true;
    }, (void*)&items, (int)items.size() );
}

bool ImGuiEx::Combo( const std::string_view label, int* index, const std::vector< const char* >& items )
{
    return ImGui::Combo( label.data(), index, []( void* data, int idx, const char** out_text )
    {
        const auto* vec = ( const std::vector< const char* >* )data;
        *out_text = vec->at( idx );
        return true;
    }, (void*)&items, (int)items.size() );
}

void ImGuiEx::TextUnformatted( const std::string_view text )
{
    ImGui::TextUnformatted( text.data(), text.data() + text.size() );
}

bool ImGuiEx::InputText( const std::string_view label, std::string& str )
{
    char buffer[ 256 ] = {};
    Zero( buffer );
    memcpy( buffer, str.data(), str.size() );
    if( ImGui::InputText( label.data(), buffer, 256 ) )
    {
        str = buffer;
        return true;
    }
    else return false;
}

bool ImGuiEx::InputText( const std::string_view label, std::string& str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data )
{
    char buffer[ 256 ] = {};
	Zero( buffer );
	memcpy( buffer, str.data(), str.size() );
	if( ImGui::InputText( label.data(), buffer, 256, flags, callback, user_data ) )
	{
		str = buffer;
		return true;
	}
	else return false;
}


bool ImGuiEx::InputText( const std::string_view label, std::string& buffer, unsigned buff_size, const char* hint, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data )
{
    if( buffer.size() < buff_size )
        buffer.resize( buff_size, '\0' );

    IM_ASSERT( !( flags & ImGuiInputTextFlags_Multiline ) ); // call InputTextMultiline()
    return ImGui::InputTextEx( label.data(), hint, buffer.data(), (int)buffer.size(), size, flags, callback, user_data );
}

bool ImGuiEx::InputTextMultiline( const std::string_view label, std::string& buffer, unsigned buff_size, const ImVec2& size, ImGuiInputTextFlags flags /*= 0*/, ImGuiInputTextCallback callback /*= nullptr*/, void* user_data /*= nullptr */ )
{
    if( buffer.size() < buff_size )
        buffer.resize( buff_size, '\0' );

    IM_ASSERT( !( flags & ImGuiInputTextFlags_Multiline ) ); // call InputTextMultiline()
    return ImGui::InputTextMultiline( label.data(), buffer.data(), (int)buffer.size(), size, flags, callback, user_data );
}

void ImGuiEx::InputTextReadOnly( const std::string_view label, const std::string& buffer, const ImVec2& size /*= ImVec2( 0, 0 )*/ )
{
    auto lock = PushStyleColour( ImGuiCol_FrameBg, 0 );
    ImGui::InputTextEx( label.data(), nullptr, (char*)buffer.data(), (int)buffer.size(), size, ImGuiInputTextFlags_ReadOnly );
}

void ImGuiEx::InputIntReadOnly( const std::string_view label, int i, const ImVec2& size /*= ImVec2( 0, 0 ) */ )
{
    auto lock = PushStyleColour( ImGuiCol_FrameBg, 0 );
    ImGui::InputInt( label.data(), &i, 0, 0, ImGuiInputTextFlags_ReadOnly );
}

void ImGuiEx::InputFloatReadOnly( const std::string_view label, float f, const char* format, const ImVec2& size /*= ImVec2( 0, 0 ) */ )
{
    auto lock = PushStyleColour( ImGuiCol_FrameBg, 0 );
    ImGui::InputFloat( label.data(), &f, 0, 0, format, ImGuiInputTextFlags_ReadOnly );
}

bool ImGuiEx::DragScalarN( const std::string_view label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* formats[], ImGuiSliderFlags flags )
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    ImGui::BeginGroup();
    ImGui::PushID( label.data() );
    ImGui::PushMultiItemsWidths( components, ImGui::CalcItemWidth() );
    size_t type_size = GDataTypeInfo[ data_type ].Size;
    for( int i = 0; i < components; i++ )
    {
        ImGui::PushID( i );
        if( i > 0 )
            ImGui::SameLine( 0, g.Style.ItemInnerSpacing.x );
        value_changed |= ImGui::DragScalar( "", data_type, p_data, v_speed, p_min, p_max, formats[ i ], flags );
        ImGui::PopID();
        ImGui::PopItemWidth();
        p_data = (void*)((char*)p_data + type_size);
    }
    ImGui::PopID();

    const char* label_end = ImGui::FindRenderedTextEnd( label.data() );
    if( label.data() != label_end )
    {
        ImGui::SameLine( 0, g.Style.ItemInnerSpacing.x );
        ImGui::TextEx( label.data(), label_end );
    }

    ImGui::EndGroup();
    return value_changed;
}

bool ImGuiEx::DragInt2( const std::string_view label, int v[ 2 ], float v_speed, int v_min, int v_max, const char* formats[ 2 ] )
{
    return DragScalarN( label, ImGuiDataType_S32, v, 2, v_speed, &v_min, &v_max, formats );
}

bool ImGuiEx::DragInt3( const std::string_view label, int v[ 3 ], float v_speed, int v_min, int v_max, const char* formats[ 3 ] )
{
    return DragScalarN( label, ImGuiDataType_S32, v, 3, v_speed, &v_min, &v_max, formats );
}

bool ImGuiEx::DragFloat2( const std::string_view label, float v[ 2 ], float v_speed, float v_min, float v_max, const char* formats[ 2 ], ImGuiSliderFlags flags )
{
    return DragScalarN( label, ImGuiDataType_Float, v, 2, v_speed, &v_min, &v_max, formats, flags );
}

bool ImGuiEx::DragFloat3( const std::string_view label, float v[ 3 ], float v_speed, float v_min, float v_max, const char* formats[ 3 ], ImGuiSliderFlags flags )
{
    return DragScalarN( label, ImGuiDataType_Float, v, 3, v_speed, &v_min, &v_max, formats, flags );
}

void ImGuiEx::SetItemTooltip( const std::string_view tooltip, const std::string_view title, ImGuiHoveredFlags hovered_flags )
{
    if( ImGui::IsItemHovered( hovered_flags ) )
    {
        ImGui::BeginTooltip();
        if( title.size() )
        {
            ImGuiEx::TextUnformatted( title );
            ImGui::Separator();
        }
        ImGuiEx::TextUnformatted( tooltip );
        ImGui::EndTooltip();
    }
}

void ImGuiEx::HelpMarker( const std::string_view tooltip, const std::string_view title )
{
    ImGui::SameLine();
    ImGui::TextDisabled( IMGUI_ICON_HELP );
    SetItemTooltip( tooltip, title );
}

ImU32 ImGuiEx::GetStyleColor( ImGuiCol colour )
{
    return ImGui::ColorConvertFloat4ToU32( ImGui::GetStyleColorVec4( colour ) );
}

void ImGuiEx::ToolTip( const std::string_view label, const std::string_view description )
{
    if( ImGui::IsItemHovered() )
    {
        ImGui::BeginTooltip();
        ImGuiEx::TextUnformatted( label );
        if( !description.empty() )
        {
            ImGui::PushTextWrapPos( 35.0f * ImGui::GetFontSize() );
            ImGui::Separator();
            ImGuiEx::TextUnformatted( description );
            ImGui::PopTextWrapPos();
        }
        ImGui::EndTooltip();
    }
}

bool ImGuiEx::Splitter( bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size )
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID( "##Splitter" );
    ImRect bb;
    bb.Min = window->DC.CursorPos + ( split_vertically ? ImVec2( *size1, 0.0f ) : ImVec2( 0.0f, *size1 ) );
    bb.Max = bb.Min + ImGui::CalcItemSize( split_vertically ? ImVec2( thickness, splitter_long_axis_size ) : ImVec2( splitter_long_axis_size, thickness ), 0.0f, 0.0f );
    return ImGui::SplitterBehavior( bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 4.0f );
}

Finally ImGuiEx::Begin( const char* name, bool* open /*= nullptr*/, ImGuiWindowFlags flags /*= 0 */ )
{
    if( ImGui::Begin( name, open, flags ) )
        return Finally( ImGui::End );

    ImGui::End();
    return {};
}

Finally ImGuiEx::BeginGroup()
{
    ImGui::BeginGroup();
    return Finally( ImGui::EndGroup );
}

Finally ImGuiEx::BeginChild( const char* str_id, const ImVec2& size /*= ImVec2( 0, 0 )*/, bool border /*= false*/, ImGuiWindowFlags flags /*= 0 */ )
{
    if( ImGui::BeginChild( str_id, size, border, flags ) )
        return Finally( ImGui::EndChild );

    ImGui::EndChild();
    return {};
}

Finally ImGuiEx::BeginChildFrame( const char* str_id, const ImVec2& size, ImGuiWindowFlags flags /*= 0 */ )
{
    if( ImGui::BeginChildFrame( ImGui::GetID( str_id ), size, flags ) )
        return Finally( ImGui::EndChildFrame );

    ImGui::EndChildFrame();
    return {};
}

Finally ImGuiEx::BeginMainMenuBar()
{
    if( ImGui::BeginMainMenuBar() )
        return Finally( ImGui::EndMainMenuBar );
    return {};
}

Finally ImGuiEx::BeginMenuBar()
{
    if( ImGui::BeginMenuBar() )
        return Finally( ImGui::EndMenuBar );
    return {};
}

Finally ImGuiEx::BeginMenu( const std::string_view label, const bool enabled /*= true */ )
{
    if( ImGui::BeginMenu( label.data(), enabled ) )
        return Finally( ImGui::EndMenu );
    return {};
}

ImGuiEx::Box::Box()
{
    Reset();
}

ImGuiEx::Box& ImGuiEx::Box::GetBoxBuilder()
{
    static Box box_builder;
    return box_builder;
}

void ImGuiEx::Box::Reset()
{
    free_function = nullptr;
    function_data = nullptr;
    size = ImVec2::Zero();
}

bool ImGuiEx::Box::Validate()
{
    return GetBoxBuilder().free_function != nullptr;
}

ImGuiEx::Box& ImGuiEx::Box::StartBox()
{
    auto& box = GetBoxBuilder();
    box.Reset();
    return box;
}

ImGuiEx::Box& ImGuiEx::Box::WithFunction( bool( *function )( void* data, int idx, const char** out_text ), void* function_data, int items_count )
{
    this->free_function = function;
    this->function_data = function_data;
    this->items_count = items_count;
    return *this;
}

ImGuiEx::Box& ImGuiEx::Box::WithCallback( ListCallback_t callback, int items_count )
{
    this->list_callback = callback;
    this->function_data = &list_callback;
    this->free_function = detail::Callback_ArrayGetter;
    this->items_count = items_count;
    return *this;
}

ImGuiEx::Box& ImGuiEx::Box::WithVector( std::vector<std::string>* items )
{
    this->function_data = items;
    this->free_function = detail::Vector_ArrayGetter;
    this->items_count = (int)items->size();
    return *this;
}

ImGuiEx::Box& ImGuiEx::Box::WithArray( const char* items[], int items_count )
{
    this->function_data = items;
    this->free_function = detail::Items_ArrayGetter;
    this->items_count = items_count;
    return *this;
}

ImGuiEx::Box& ImGuiEx::Box::WithSelectableCallback( SelectableCallback_t callback )
{
    this->selectable_callback = callback;
    return *this;
}

ImGuiEx::Box& ImGuiEx::Box::WithSize( int height_in_items )
{
    this->height_in_items = height_in_items;
    return *this;
}

ImGuiEx::Box& ImGuiEx::Box::WithSize( const ImVec2& size )
{
    this->size = size;
    return *this;
}

bool ImGuiEx::Box::ShowComboBox( const std::string_view label, int* current_item )
{
    if( !Validate() )
        return false;

    // Call the getter to obtain the preview string which is a parameter to BeginCombo()
    const char* preview_value = NULL;
    if( *current_item >= 0 && *current_item < items_count )
        free_function( function_data, *current_item, &preview_value );

    if( !ImGui::BeginCombo( label.data(), preview_value, ImGuiComboFlags_None ) )
        return false;

    // Display items
    // FIXME-OPT: Use clipper (but we need to disable it on the appearing frame to make sure our call to SetItemDefaultFocus() is processed)
    bool value_changed = false;
    for( int i = 0; i < items_count; i++ )
    {
        auto lock = PushID( (void*)(intptr_t)i );
        const bool item_selected = ( i == *current_item );
        const char* item_text;
        if( !free_function( function_data, i, &item_text ) )
            item_text = "*Unknown item*";
        if( ImGui::Selectable( item_text, item_selected ) )
        {
            value_changed = true;
            *current_item = i;
        }
        if( item_selected )
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
    return value_changed;
}

void ImGuiEx::TextHeader( const std::string_view text, const ImGuiCol_ color /*= ImGuiCol_Header*/ )
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if( window->SkipItems )
        return;

    ImGui::PushStyleColor( ImGuiCol_Header, ImGui::GetStyleColorVec4( color ) );
    ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4( color ) );
    ImGui::PushStyleColor( ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4( color ) );
    ImGui::TreeNodeBehavior( window->GetID( text.data() ), ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowItemOverlap, text.data() );
    ImGui::PopStyleColor( 3 );

    return;
}

bool ImGuiEx::ListBoxHeader( const std::string_view label, int items_count, int height_in_items )
{
    if( height_in_items < 0 )
        height_in_items = ImMin( items_count, 7 );

    const ImGuiStyle& style = ImGui::GetStyle();

    // OVERRIDING THIS BEHAVIOUR BECAUSE IT LOOKS BAD
    // float height_in_items_f = ( height_in_items < items_count ) ? ( height_in_items + 0.25f ) : ( height_in_items + 0.00f );
    float height_in_items_f = height_in_items + 0.25f;

    // We include ItemSpacing.y so that a list sized for the exact number of items doesn't make a scrollbar appears. We could also enforce that by passing a flag to BeginChild().
    ImVec2 size;
    size + size;
    size.x = 0.0f;
    size.y = ImGui::GetTextLineHeightWithSpacing() * height_in_items_f + style.FramePadding.y * 2.0f;
    return ImGui::ListBoxHeader( label.data(), size );
}

bool ImGuiEx::Box::ShowListBox( const std::string_view label, int* current_item )
{
    if( !Validate() )
        return false;

    if( size.IsZero() )
    {
        if( !ListBoxHeader( label, items_count, height_in_items ) )
            return false;
    }
    else if( !ImGui::ListBoxHeader( label.data(), size ) )
        return false;

    // Assume all items have even height (= 1 line of text). If you need items of different or variable sizes you can create a custom version of ListBox() in your code without using the clipper.
    ImGuiContext& g = *ImGui::GetCurrentContext();
    bool value_changed = false;

    Clipper( items_count, [&]( const int i )
    {
        const bool item_selected = ( i == *current_item );
        const char* item_text;
        if( !free_function( function_data, i, &item_text ) )
            item_text = "*Unknown item*";

        auto lock = PushID( i );

        if( selectable_callback )
        {
            if( ImGui::Selectable( "##dummy_text", item_selected ) )
            {
                *current_item = i;
                value_changed = true;
            }

            ImGui::SameLine();
            selectable_callback( i, item_text );
        }
        else if( ImGui::Selectable( item_text, item_selected ) )
        {
            *current_item = i;
            value_changed = true;
        }

        if( item_selected )
            ImGui::SetItemDefaultFocus();
    } );

    ImGui::ListBoxFooter();
    if( value_changed )
        ImGui::MarkItemEdited( g.LastItemData.ID );

    return value_changed;
}

std::unique_ptr<Finally> ImGuiEx::TreeNodeEx( const std::string_view id, const std::string_view label, bool* p_open, ImGuiTreeNodeFlags flags )
{
    if( detail::TreeNodeClose( id, label, p_open, flags ) )
        return make_finally( ImGui::TreePop );
    return nullptr;
}

std::unique_ptr<Finally> ImGuiEx::TreeNode( const std::string_view label, bool* p_open, ImGuiTreeNodeFlags flags )
{
    if( detail::TreeNodeClose( label, label, p_open, flags ) )
        return make_finally( ImGui::TreePop );
    return nullptr;
}

std::unique_ptr<Finally> ImGuiEx::TreeNode( const std::string_view label, ImGuiTreeNodeFlags flags )
{
    if( ImGui::TreeNodeEx( label.data(), flags ) )
        return make_finally( ImGui::TreePop );
    return nullptr;
}

std::unique_ptr<Finally> ImGuiEx::Columns( const int count )
{
    const auto before = ImGui::GetColumnsCount();
    ImGui::Columns( count );
    return make_finally( &ImGui::Columns, before, nullptr, false );
}

std::unique_ptr<Finally> ImGuiEx::BeginColumns( const char* id, const int count, ImGuiOldColumnFlags flags )
{
    ImGui::BeginColumns( id, count, flags );
    return make_finally( &ImGui::EndColumns );
}

std::unique_ptr<Finally> ImGuiEx::TreeNode( const char* str_id, const char* fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    const bool open = ImGui::TreeNode( str_id, fmt, args );
    va_end( args );

    return open ? make_finally( ImGui::TreePop ) : nullptr;
}

std::unique_ptr<Finally> ImGuiEx::TreeNode( const void* ptr_id, const char* fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    const bool open = ImGui::TreeNode( ptr_id, fmt, args );
    va_end( args );

    return open ? make_finally( ImGui::TreePop ) : nullptr;
}

std::unique_ptr<Finally> ImGuiEx::TreeNode( const std::string_view label )
{
    if( ImGui::TreeNode( label.data() ) )
        return make_finally( ImGui::TreePop );
    return nullptr;
}

bool ImGuiEx::CollapsingHeader( const char* id, const char* text, ImGuiTreeNodeFlags flags )
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if( window->SkipItems )
        return false;

    return ImGui::TreeNodeBehavior( window->GetID( id ), flags | ImGuiTreeNodeFlags_CollapsingHeader, text );
}

void ImGuiEx::PushDisable( const bool disable /*= true*/ )
{
    ImGui::PushItemFlag( ImGuiItemFlags_Disabled, disable );
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, disable ? ImGui::GetStyle().Alpha * 0.5f : ImGui::GetStyle().Alpha );
}

void ImGuiEx::PopDisable()
{
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}

std::string ImGuiEx::TrimTextFromLeft( const std::string& text, const bool advance_cursor, const float width, const float padding )
{
    auto window = ImGui::GetCurrentWindow();
    if( window->SkipItems )
        return text;

    const float available_space = width - padding;
    const float required_space = ImGui::CalcTextSize( text.c_str() ).x;
    if( required_space <= available_space )
        return text;

    std::string final_name;
    size_t lo = 0;
    size_t hi = text.length();

    while( hi > lo + 1 )
    {
        size_t mid = (lo + hi) / 2;
        std::string trimmed_text = "..." + text.substr( mid );
        float required_space = ImGui::CalcTextSize( trimmed_text.c_str() ).x;
        if( required_space <= available_space )
            hi = mid;
        else
            lo = mid;
    }

    final_name = "..." + text.substr( hi );
    
    if( advance_cursor )
        ImGui::SetCursorPosX( ImGui::GetCursorPosX() + available_space - ImGui::CalcTextSize( final_name.c_str() ).x );

    return final_name;
}

Finally ImGuiEx::BeginDragDropSource( ImGuiDragDropFlags flags /*= 0 */ )
{
    if( ImGui::BeginDragDropSource( flags ) )
        return Finally( ImGui::EndDragDropSource );
    return {};
}

Finally ImGuiEx::BeginDragDropTarget()
{
    if( ImGui::BeginDragDropTarget() )
        return Finally( ImGui::EndDragDropTarget );
    return {};
}

Finally ImGuiEx::BeginPopupModal( const char* name, bool* p_open /*= nullptr*/, ImGuiWindowFlags flags /*= 0 */ )
{
    if( ImGui::BeginPopupModal( name, p_open, flags ) )
        return Finally( ImGui::EndPopup );
    return {};
}

Finally ImGuiEx::BeginPopup( const char* name, ImGuiWindowFlags flags /*= 0 */ )
{
    if( ImGui::BeginPopup( name, flags ) )
        return Finally( ImGui::EndPopup );
    return {};
}

Finally ImGuiEx::BeginFocused( const char* name, const char** set_last_focused, bool* open, ImGuiWindowFlags flags )
{
    if( set_last_focused && *set_last_focused && *set_last_focused != name )
        flags |= ImGuiWindowFlags_NoFocusOnAppearing;

    if( ImGui::Begin( name, open, flags ) )
    {
        return Finally( [name, set_last_focused]()
        {
            if( set_last_focused )
                *set_last_focused = name;
            ImGui::End();
        } );
    }

    ImGui::End();
    return {};
}

void ImGuiEx::Combo( const std::string_view label, const char* preview, const int item_count, ImGuiComboFlags flags, ClipperFunction func )
{
    if( !ImGui::BeginCombo( label.data(), preview, flags ) )
        return;

    Clipper( item_count, func );
    ImGui::EndCombo();
}

void ImGuiEx::ListBox( const std::string_view label, int items_count, int height_in_items, ClipperFunction func )
{
    if( !ListBoxHeader( label, items_count, height_in_items ) )
        return;

    Clipper( items_count, func );
    ImGui::ListBoxFooter();
}

void ImGuiEx::ListBox( const std::string_view label, int items_count, const ImVec2& size, ClipperFunction func )
{
    if( !ImGui::ListBoxHeader( label.data(), size ) )
        return;

    Clipper( items_count, func );
    ImGui::ListBoxFooter();
}

std::unique_ptr<Finally> ImGuiEx::Indent( const float width /*= 0.0f */ )
{
    ImGui::Indent( width );
    return make_finally( &ImGui::Unindent, width );
}

std::unique_ptr<Finally> ImGuiEx::PushID( const std::string_view str_id )
{
    ImGui::PushID( str_id.data(), str_id.data() + str_id.size() );
    return make_finally( ImGui::PopID );
}

std::unique_ptr<Finally> ImGuiEx::PushID( const void* ptr )
{
    ImGui::PushID( ptr );
    return make_finally( ImGui::PopID );
}

std::unique_ptr<Finally> ImGuiEx::PushID( const char* str_id_begin, const char* str_id_end )
{
    ImGui::PushID( str_id_begin, str_id_end );
    return make_finally( ImGui::PopID );
}

std::unique_ptr<Finally> ImGuiEx::PushID( int int_id )
{
    ImGui::PushID( int_id );
    return make_finally( ImGui::PopID );
}

std::unique_ptr<Finally> ImGuiEx::PushStyleColour( const ImGuiCol type, const ImU32 colour )
{
    ImGui::PushStyleColor( type, colour );
    return make_finally( &ImGui::PopStyleColor, 1 );
}

std::unique_ptr<Finally> ImGuiEx::PushStyleColours( std::initializer_list<std::pair<ImGuiCol, ImU32>> list )
{
    for( auto& pair : list )
        ImGui::PushStyleColor( pair.first, pair.second );

    return make_finally( &ImGui::PopStyleColor, (int)list.size() );
}

std::unique_ptr<Finally> ImGuiEx::PushItemWidth( const float width )
{
    ImGui::PushItemWidth( width );
    return make_finally( ImGui::PopItemWidth );
}

void ImGuiEx::StyleColorsGemini( ImGuiStyle* dst )
{
    ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    colors[ImGuiCol_Text]                   = ImVec4( 0.90f, 0.90f, 0.90f, 1.00f );
    colors[ImGuiCol_TextDisabled]           = ImVec4( 0.57f, 0.57f, 0.57f, 1.00f );
    colors[ImGuiCol_WindowBg]               = ImVec4( 0.25f, 0.25f, 0.25f, 1.00f );
    colors[ImGuiCol_ChildBg]                = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ImGuiCol_PopupBg]                = ImVec4( 0.11f, 0.11f, 0.14f, 0.92f );
    colors[ImGuiCol_Border]                 = ImVec4( 0.50f, 0.50f, 0.50f, 0.50f );
    colors[ImGuiCol_BorderShadow]           = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ImGuiCol_FrameBg]                = ImVec4( 0.00f, 0.00f, 0.00f, 0.39f );
    colors[ImGuiCol_FrameBgHovered]         = ImVec4( 0.47f, 0.68f, 0.69f, 0.40f );
    colors[ImGuiCol_FrameBgActive]          = ImVec4( 1.00f, 0.50f, 0.00f, 0.69f );
    colors[ImGuiCol_TitleBg]                = ImVec4( 0.71f, 0.31f, 0.00f, 0.83f );
    colors[ImGuiCol_TitleBgActive]          = ImVec4( 0.84f, 0.28f, 0.12f, 0.87f );
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4( 1.00f, 0.38f, 0.00f, 0.20f );
    colors[ImGuiCol_MenuBarBg]              = ImVec4( 0.55f, 0.43f, 0.40f, 0.80f );
    colors[ImGuiCol_ScrollbarBg]            = ImVec4( 0.00f, 0.00f, 0.00f, 0.60f );
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4( 0.00f, 0.90f, 0.83f, 0.30f );
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4( 0.00f, 1.00f, 0.83f, 0.40f );
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4( 0.00f, 0.36f, 0.32f, 0.60f );
    colors[ImGuiCol_CheckMark]              = ImVec4( 0.90f, 0.90f, 0.90f, 0.50f );
    colors[ImGuiCol_SliderGrab]             = ImVec4( 1.00f, 1.00f, 1.00f, 0.30f );
    colors[ImGuiCol_SliderGrabActive]       = ImVec4( 0.80f, 0.61f, 0.39f, 0.60f );
    colors[ImGuiCol_Button]                 = ImVec4( 0.00f, 0.00f, 0.00f, 0.62f );
    colors[ImGuiCol_ButtonHovered]          = ImVec4( 0.00f, 0.55f, 0.47f, 0.79f );
    colors[ImGuiCol_ButtonActive]           = ImVec4( 0.05f, 0.87f, 0.75f, 1.00f );
    colors[ImGuiCol_Header]                 = ImVec4( 0.00f, 0.70f, 0.64f, 0.45f );
    colors[ImGuiCol_HeaderHovered]          = ImVec4( 1.00f, 0.44f, 0.00f, 0.80f );
    colors[ImGuiCol_HeaderActive]           = ImVec4( 0.82f, 0.33f, 0.10f, 0.80f );
    colors[ImGuiCol_Separator]              = ImVec4( 0.68f, 0.31f, 0.02f, 1.00f );
    colors[ImGuiCol_SeparatorHovered]       = ImVec4( 0.98f, 0.40f, 0.00f, 1.00f );
    colors[ImGuiCol_SeparatorActive]        = ImVec4( 1.00f, 0.35f, 0.00f, 1.00f );
    colors[ImGuiCol_ResizeGrip]             = ImVec4( 1.00f, 1.00f, 1.00f, 0.16f );
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4( 0.85f, 0.27f, 0.00f, 0.60f );
    colors[ImGuiCol_ResizeGripActive]       = ImVec4( 1.00f, 0.41f, 0.00f, 0.90f );
    colors[ImGuiCol_Tab]                    = ImVec4( 0.74f, 0.26f, 0.00f, 0.79f );
    colors[ImGuiCol_TabHovered]             = ImVec4( 1.00f, 0.50f, 0.00f, 0.80f );
    colors[ImGuiCol_TabActive]              = ImVec4( 1.00f, 0.38f, 0.00f, 0.84f );
    colors[ImGuiCol_TabUnfocused]           = ImVec4( 0.57f, 0.38f, 0.28f, 0.82f );
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4( 0.47f, 0.24f, 0.14f, 0.84f );
    colors[ImGuiCol_DockingPreview]         = ImVec4( 0.83f, 0.34f, 0.00f, 0.31f );
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
    colors[ImGuiCol_PlotLines]              = ImVec4( 1.00f, 1.00f, 1.00f, 1.00f );
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ImGuiCol_PlotHistogram]          = ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
    colors[ImGuiCol_TableHeaderBg]          = ImVec4( 0.19f, 0.19f, 0.20f, 1.00f );
    colors[ImGuiCol_TableBorderStrong]      = ImVec4( 0.31f, 0.31f, 0.35f, 1.00f );   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableBorderLight]       = ImVec4( 0.23f, 0.23f, 0.25f, 1.00f );   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableRowBg]             = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4( 1.00f, 1.00f, 1.00f, 0.02f );
    colors[ImGuiCol_TextSelectedBg]         = ImVec4( 0.00f, 0.71f, 0.70f, 0.35f );
    colors[ImGuiCol_DragDropTarget]         = ImVec4( 1.00f, 1.00f, 1.00f, 0.90f );
    colors[ImGuiCol_NavHighlight]           = ImVec4( 0.94f, 0.42f, 0.01f, 0.80f );
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4( 0.20f, 0.20f, 0.20f, 0.35f );
}

void ImGuiEx::PopupHandler::AddPopup( const std::string& id, const bool modal, const ImGuiWindowFlags flags, std::function<bool()> func )
{
    popups.emplace_back( Popup{ func, id, flags, modal, false } );
}

void ImGuiEx::PopupHandler::Render()
{
    RemoveIf( popups, []( Popup& popup )
    {
        if( !popup.opened )
        {
            ImGui::OpenPopup( popup.id.c_str() );
            popup.opened = true;
        }

        bool open = true;

        auto popup_open = popup.modal ?
            ImGui::BeginPopupModal( popup.id.c_str(), &open, popup.flags ):
            ImGui::BeginPopup( popup.id.c_str(), popup.flags );

        if( popup_open )
        {
            const bool result = popup.func() || !open;
            ImGui::EndPopup();
            return result;
        }

        return false;
    } );
}

size_t ImGuiEx::FormatStringEllipsis(float max_width, bool ellipsis_at_end, char* buffer, size_t buffer_size)
{
    if (*buffer == '\0')
        return 0;

    auto font = ImGui::GetFont();
    char* label_end = buffer + strlen(buffer);
    
    char* buffer_start = buffer;
    char* buffer_end = buffer + buffer_size;

    ImWchar ellipsis_char = font->EllipsisChar;
    char ellipsis_text[ 8 ] = {};
    if (ellipsis_char == (ImWchar)-1)
    {
        ellipsis_text[0] = '.';
        ellipsis_text[1] = '.';
        ellipsis_text[2] = '.';
        ellipsis_text[3] = '\0';
    }
    else
        ImTextStrToUtf8(ellipsis_text, int(std::size(ellipsis_text)), &ellipsis_char, (&ellipsis_char) + 1);

    char* ellipsis_end = ellipsis_text + strlen(ellipsis_text);
    float ellipsis_total_width = ImGui::CalcTextSize(ellipsis_text).x;
    float width = 0;
    bool addEllipsis = false;

    auto IsSpace = [&](char c) -> bool
    {
        constexpr uint32_t MASK = (1 << (' ' - 1)) | (1 << ('\f' - 1)) | (1 << ('\n' - 1)) | (1 << ('\r' - 1)) | (1 << ('\t' - 1)) | (1 << ('\v' - 1));
        return c > 0 && c <= 32 && ((uint32_t(1) << (c - 1)) & MASK) != 0;
    };

    if (ellipsis_at_end)
    {
        char* start = buffer;
        char* end = buffer;

        while (*start != '\0')
        {
            while (IsSpace(*end))
                end++;

            if (*end != '\0')
                end++;

            const auto wordSize = ImGui::CalcTextSize(start, end);

            if (wordSize.x + width + ellipsis_total_width < max_width)
            {
                width += wordSize.x;
                start = end;
            }
            else if (*end == '\0' && wordSize.x + width < max_width)
            {
                width += wordSize.x;
                start = end;
            }
            else
            {
                addEllipsis = true;
                break;
            }
        }

        if (addEllipsis)
        {
            for (char* ellipsis = ellipsis_text; ellipsis != ellipsis_end && start != buffer_end; ellipsis++, start++)
                *start = *ellipsis;
        }

        if (start == buffer_end)
            start--;

        *start = '\0';
        return start - buffer_start;
    }
    else
    {
        char* start = label_end - 1;
        char* end = label_end;

        while (end != buffer)
        {
            while (start != buffer && IsSpace(*start))
                start--;

            if (start != buffer)
                start--;

            const auto wordSize = ImGui::CalcTextSize(start, end);

            if (wordSize.x + width + ellipsis_total_width < max_width)
            {
                width += wordSize.x;
                end = start;
            }
            else if (start == buffer && wordSize.x + width < max_width)
            {
                width += wordSize.x;
                end = start;
            }
            else
            {
                addEllipsis = true;
                break;
            }
        }

        if (addEllipsis)
        {
            if (label_end != buffer_end)
            {
                for (char* src = label_end, *dst = buffer_end; src != end; src--, dst--)
                    *(dst - 1) = *(src - 1);

                end += buffer_end - label_end;
                label_end = buffer_end;
            }

            char* pos = buffer;
            for (char* ellipsis = ellipsis_text; pos != end && ellipsis != ellipsis_end; pos++, ellipsis++)
                *pos = *ellipsis;

            if (pos != end)
                for (; end != label_end; pos++, end++)
                    *pos = *end;

            if (pos == buffer_end)
                pos--;

            *pos = '\0';
            return pos - buffer;
        }

        return label_end - buffer;
    }
}

char* ImGuiEx::FormatMemory(char* buffer, size_t size, uint64_t value)
{
    if (value > 4 * Memory::GB)
        ImFormatString(buffer, size, "%.2f %s", double(value) / (double)Memory::GB, "GB");
    else if (value > 4 * Memory::MB)
        ImFormatString(buffer, size, "%.2f %s", double(value) / (double)Memory::MB, "MB");
    else if (value > 4 * Memory::KB)
        ImFormatString(buffer, size, "%.2f %s", double(value) / (double)Memory::KB, "KB");
    else
        ImFormatString(buffer, size, "%.2f %s", double(value) / (double)Memory::B, "B");

    return buffer;
}

void ImGuiEx::TextAnimated(const char* text, float time)
{
    constexpr float Speed = 100.0f;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    const auto avail = ImGui::CalcItemWidth();
    const auto text_len = ImGui::CalcTextSize(text);

    const auto total_size = ImVec2(avail, text_len.y);
    const auto pos = window->DC.CursorPos;
    ImGui::ItemSize(total_size, 0.0f);
    const ImRect bb(pos, pos + total_size);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    if (avail >= text_len.x)
    {
        ImGui::RenderTextClipped(bb.Min, bb.Max, text, nullptr, &text_len, ImVec2(0, 0));
        return;
    }

    const float t_offset = std::fmod(time * Speed, text_len.x + (2.0f * Speed)) - Speed;
    const float offset = std::clamp(t_offset - (avail / 2), 0.0f, text_len.x - avail);
    const float alpha = std::clamp(1.0f + (std::min(t_offset, text_len.x - t_offset) / (Speed / 2)), 0.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text, alpha));
    ImGui::RenderTextClipped(ImVec2(bb.Min.x - offset, bb.Min.y), bb.Max, text, nullptr, &text_len, ImVec2(0, 0), &bb);
    ImGui::PopStyleColor();
}

bool ImGuiEx::VSliderScalar(const std::string_view label, const ImVec2& size, ImGuiDataType data_type, void* v, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID( label.data() );

	const ImVec2 label_size = ImGui::CalcTextSize(label.data(), NULL, true);
	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
	const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
	ImGui::ItemSize(total_bb, style.FramePadding.y);
	if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
		return false;

	// Default format string when passing NULL
	if (format == NULL)
		format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

	const bool hovered = ImGui::ItemHoverable(frame_bb, id);
	bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
	if (!temp_input_is_active)
	{
        // Tabbing or CTRL-clicking on Slider turns it into an input box
        const bool input_requested_by_tabbing = temp_input_allowed && (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_FocusedByTabbing) != 0;
        const bool clicked = (hovered && g.IO.MouseClicked[0]);
        const bool make_active = (input_requested_by_tabbing || clicked || g.NavActivateId == id || g.NavActivateInputId == id);
        if (make_active && temp_input_allowed)
            if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || g.NavActivateInputId == id)
                temp_input_is_active = true;

        if (make_active && !temp_input_is_active)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        }
	}

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
        const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;
        return ImGui::TempInputScalar(frame_bb, id, label.data(), data_type, v, format, is_clamp_input ? v_min : NULL, is_clamp_input ? v_max : NULL);
    }

	// Draw frame
	const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	ImGui::RenderNavHighlight(frame_bb, id);
	ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

	ImRect grab_bb;
	const bool value_changed = ImGui::SliderBehavior(frame_bb, id, data_type, v, v_min, v_max, format, flags | ImGuiSliderFlags_Vertical, &grab_bb);
	if (value_changed)
		ImGui::MarkItemEdited(id);

	// Render grab
	if (grab_bb.Max.y > grab_bb.Min.y)
		window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

	// Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
	// For the vertical slider we allow centered text to overlap the frame padding
    char value_buf[ 64 ] = {};
	const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, v, format);
	ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.0f));
	if (label_size.x > 0.0f)
		ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label.data() );

	return value_changed;
}

bool ImGuiEx::VDragScalar(const std::string_view label, const ImVec2& size, ImGuiDataType data_type, void* v, float v_speed, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags, bool has_min_max)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const ImGuiID id = window->GetID(label.data() );

	const ImVec2 label_size = ImGui::CalcTextSize(label.data(), NULL, true);
	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
	const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
        return false;

	// Default format string when passing NULL
	if (format == NULL)
		format = ImGui::DataTypeGetInfo(data_type)->PrintFmt;

	// Tabbing or CTRL-clicking on Drag turns it into an input box
	const bool hovered = ImGui::ItemHoverable(frame_bb, id);
	bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        // Tabbing or CTRL-clicking on Drag turns it into an InputText
        const bool input_requested_by_tabbing = temp_input_allowed && (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_FocusedByTabbing) != 0;
        const bool clicked = (hovered && g.IO.MouseClicked[0]);
        const bool double_clicked = (hovered && g.IO.MouseClickedCount[0] == 2);
        const bool make_active = (input_requested_by_tabbing || clicked || double_clicked || g.NavActivateId == id || g.NavActivateInputId == id);
        if (make_active && temp_input_allowed)
            if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || double_clicked || g.NavActivateInputId == id)
                temp_input_is_active = true;

        // (Optional) simple click (without moving) turns Drag into an InputText
        if (g.IO.ConfigDragClickToInputText && temp_input_allowed && !temp_input_is_active)
            if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !ImGui::IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * 0.50f))
            {
                g.NavActivateId = g.NavActivateInputId = id;
                g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
                temp_input_is_active = true;
            }

        if (make_active && !temp_input_is_active)
        {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        }
    }

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
        const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0 && (v_min == NULL || v_max == NULL || ImGui::DataTypeCompare(data_type, v_min, v_max) < 0);
        return ImGui::TempInputScalar(frame_bb, id, label.data(), data_type, v, format, is_clamp_input ? v_min : NULL, is_clamp_input ? v_max : NULL);
    }

	// Draw frame
	const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	ImGui::RenderNavHighlight(frame_bb, id);
	ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

	// Drag behavior
	bool value_changed = false;
	if (has_min_max)
		value_changed = ImGui::DragBehavior(id, data_type, v, v_speed, v_min, v_max, format, flags | ImGuiSliderFlags_Vertical);
	else if (g.ActiveId == id)
	{
		if (g.ActiveIdSource == ImGuiInputSource_Mouse && !g.IO.MouseDown[0])
			ImGui::ClearActiveID();
		else if (g.ActiveIdSource == ImGuiInputSource_Nav && g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
			ImGui::ClearActiveID();
	}

	if (value_changed)
		ImGui::MarkItemEdited(id);

	// Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[ 64 ] = {};
	const char* value_buf_end = value_buf + ImGui::DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, v, format);
	ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

	if (label_size.x > 0.0f)
		ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label.data());

	return value_changed;
}

bool ImGuiEx::TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const bool display_frame = (flags & ImGuiTreeNodeFlags_Framed) != 0;
	const ImVec2 padding = (display_frame || (flags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, 0.0f);

    if( !label_end )
		label_end = ImGui::FindRenderedTextEnd( label );
	const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, true);

	// We vertically grow up to current line height up the typical widget height.
	const float text_base_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset); // Latch before ItemSize changes it
	const float frame_height = ImGui::GetFrameHeight();
	ImRect frame_bb = ImRect(window->DC.CursorPos, window->DC.CursorPos + ImVec2(ImGui::CalcItemWidth(), frame_height));

	const float text_offset_x = (g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2));   // Collapser arrow width + Spacing
	const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);
	const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2 : 0.0f);   // Include collapser
	ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
	ImGui::ItemSize(ImVec2(text_width, frame_height), text_base_offset_y);

	// For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
	// (Ideally we'd want to add a flag for the user to specify if we want the hit test to be done up to the right side of the content or not)
	const ImRect interact_bb = display_frame ? frame_bb : ImRect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + text_width + style.ItemSpacing.x * 2, frame_bb.Max.y);
	bool is_open = ImGui::TreeNodeBehaviorIsOpen(id, flags);
	bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;

	// Store a flag for the current depth to tell if we will allow closing this node when navigating one of its child.
	// For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode() and TreePop().
	// This is currently only support 32 level deep and we are fine with (1 << Depth) overflowing into a zero.
	if (is_open && !g.NavIdIsAlive && (flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
		window->DC.TreeJumpToParentOnPopMask |= (1 << window->DC.TreeDepth);

	bool item_add = ImGui::ItemAdd(interact_bb, id);
	g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
	g.LastItemData.DisplayRect = frame_bb;

	if (!item_add)
	{
		if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
			ImGui::TreePushOverrideID(id);
		IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags | (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) | (is_open ? ImGuiItemStatusFlags_Opened : 0));
		return is_open;
	}

	// Flags that affects opening behavior:
	// - 0 (default) .................... single-click anywhere to open
	// - OpenOnDoubleClick .............. double-click anywhere to open
	// - OpenOnArrow .................... single-click on arrow to open
	// - OpenOnDoubleClick|OpenOnArrow .. single-click on arrow or double-click anywhere to open
	ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
	if (flags & ImGuiTreeNodeFlags_AllowItemOverlap)
		button_flags |= ImGuiButtonFlags_AllowItemOverlap;
	if (!is_leaf)
		button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

	const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
	const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
	const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);
	if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
		button_flags |= ImGuiButtonFlags_NoKeyModifiers;

	// Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
	// Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to requirements for multi-selection and drag and drop support.
	// - Single-click on label = Toggle on MouseUp (default)
	// - Single-click on arrow = Toggle on MouseUp (when _OpenOnArrow=0)
	// - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
	// - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
	// - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and _OpenOnArrow=0)
	// This makes _OpenOnArrow have a subtle effect on _OpenOnDoubleClick: arrow click reacts on Down rather than Up.
	// It is rather standard that arrow click react on Down rather than Up and we'd be tempted to make it the default
	// (by removing the _OpenOnArrow test below), however this would have a perhaps surprising effect on CollapsingHeader()?
	// So right now we are making this optional. May evolve later.
	// We set ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be active on the initial MouseDown in order for drag and drop to work.
	if (is_mouse_x_over_arrow && (flags & ImGuiTreeNodeFlags_OpenOnArrow))
		button_flags |= ImGuiButtonFlags_PressedOnClick;
	else if (flags & ImGuiTreeNodeFlags_OpenOnDoubleClick)
		button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
	else
		button_flags |= ImGuiButtonFlags_PressedOnClickRelease;

	bool selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;
	const bool was_selected = selected;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
	bool toggled = false;
	if (!is_leaf)
	{
		if (pressed && g.DragDropHoldJustPressedId != id)
		{
			if ((flags & (ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) == 0 || (g.NavActivateId == id))
				toggled = true;
			if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
				toggled |= is_mouse_x_over_arrow && !g.NavDisableMouseHover; // Lightweight equivalent of IsMouseHoveringRect() since ButtonBehavior() already did the job
			if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseDoubleClicked[0])
				toggled = true;
		}
		else if (pressed && g.DragDropHoldJustPressedId == id)
		{
			IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
			if (!is_open) // When using Drag and Drop "hold to open" we keep the node highlighted after opening, but never close it again.
				toggled = true;
		}

		if (g.NavId == id && g.NavMoveDir == ImGuiDir_Left && is_open)
		{
			toggled = true;
			ImGui::NavMoveRequestCancel();
		}
		if (g.NavId == id && g.NavMoveDir == ImGuiDir_Right && !is_open) // If there's something upcoming on the line we may want to give it the priority?
		{
			toggled = true;
			ImGui::NavMoveRequestCancel();
		}

		if (toggled)
		{
			is_open = !is_open;
			window->DC.StateStorage->SetInt(id, is_open);
			g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
		}
	}
	if (flags & ImGuiTreeNodeFlags_AllowItemOverlap)
		ImGui::SetItemAllowOverlap();

	// In this branch, TreeNodeBehavior() cannot toggle the selection so this will never trigger.
	if (selected != was_selected) //-V547
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

	// Render
	const ImU32 bg_col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
	const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
	ImGuiNavHighlightFlags nav_highlight_flags = ImGuiNavHighlightFlags_TypeThin;
	if (display_frame)
	{
		// Framed type
		ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
		ImGui::RenderNavHighlight(frame_bb, id, nav_highlight_flags);
		ImGui::RenderArrow(window->DrawList, frame_bb.Min + ImVec2(padding.x, text_base_offset_y), text_col, is_open ? ImGuiDir_Down : ImGuiDir_Right, 1.0f);
		if (flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
			frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
		if (g.LogEnabled)
		{
			// NB: '##' is normally used to hide text (as a library-wide feature), so we need to specify the text range to make sure the ## aren't stripped out here.
			const char log_prefix[] = "\n##";
			const char log_suffix[] = "##";
			ImGui::LogRenderedText(&text_pos, log_prefix, log_prefix + 3);
			ImGui::RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
			ImGui::LogRenderedText(&text_pos, log_suffix, log_suffix + 2);
		}
		else
		{
			ImGui::RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
		}
	}
	else
	{
		// Unframed typed for tree nodes
		if (hovered || selected)
		{
			ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
		}
		ImGui::RenderNavHighlight(frame_bb, id, nav_highlight_flags);
		if (flags & ImGuiTreeNodeFlags_Bullet)
			ImGui::RenderBullet(window->DrawList, frame_bb.Min + ImVec2(text_offset_x * 0.5f, g.FontSize * 0.50f + text_base_offset_y), text_col);
		else if (!is_leaf)
			ImGui::RenderArrow(window->DrawList, frame_bb.Min + ImVec2(padding.x, g.FontSize * 0.15f + text_base_offset_y), text_col, is_open ? ImGuiDir_Down : ImGuiDir_Right, 0.70f);
		if (g.LogEnabled)
			ImGui::LogRenderedText(&text_pos, ">");
		ImGui::RenderText(text_pos, label, label_end, false);
	}

	if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
		ImGui::TreePushOverrideID(id);
	IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) | (is_open ? ImGuiItemStatusFlags_Opened : 0));
	return is_open;
}

void ImGuiEx::PlotMultiHistogram(const std::string_view label, const float* values, int value_count, int row_count, const ImColor* row_colors, int color_count, int values_offset, const char* tooltip_text, float scale_min, float scale_max, ImVec2 frame_size)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label.data());
    const ImVec2 label_size = ImGui::CalcTextSize(label.data(), NULL, true);
    if (frame_size.x == 0.0f)
        frame_size.x = ImGui::CalcItemWidth();
    if (frame_size.y == 0.0f)
        frame_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, 0, &frame_bb))
        return;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id);

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX)
    {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int i = 0; i < value_count; i++)
        {
            float sum = 0.0f;
            for (int j = 0; j < row_count; j++)
            {
                const float v = ImMax(0.0f, values[((i + values_offset) * row_count) + j]);
                if (v != v) // Ignore NaN values
                    continue;

                sum += v;
            }

            v_min = ImMin(v_min, sum);
            v_max = ImMax(v_max, sum);
        }

        if (scale_min == FLT_MAX)
            scale_min = v_min;

        if (scale_max == FLT_MAX)
            scale_max = v_max;
    }

    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    int idx_hovered = -1;
    int idx_hovered_row = -1;
    if (value_count >= 1)
    {
        int res_w = ImMin((int)frame_size.x, value_count);
        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

        // Tooltip on hover
        if (hovered && inner_bb.Contains(g.IO.MousePos))
        {
            const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
            const int v_idx = (int)(t * value_count);
            IM_ASSERT(v_idx >= 0 && v_idx < value_count);

            const float r = ImClamp((g.IO.MousePos.y - inner_bb.Max.y) / (inner_bb.Min.y - inner_bb.Max.y), 0.0f, 0.9999f);
            float sum = 0.0f;
            for (int i = 0; i < row_count; i++)
            {
                const float v = ImMax(values[((v_idx + values_offset) * row_count) + i], 0.0f);
                if (v != v)
                    continue;

                const float scaled = (v - scale_min) * inv_scale;
                if (sum <= r && sum + scaled > r)
                {
                    idx_hovered = v_idx;
                    idx_hovered_row = i;
                    if (tooltip_text)
                        ImGui::SetTooltip(tooltip_text, unsigned(v_idx + values_offset), unsigned(i), v);
                }

                sum += scaled;
            }
        }

        const ImU32 col_hovered = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);
        for (int i = 0; i < value_count; i++)
        {
            ImVec2 t0 = ImVec2(float(i) * t_step, 1.0f); // Normalized lower left point
            ImVec2 t1 = ImVec2(t0.x + t_step, 1.0f); // Normalized upper right point
            
            for (int j = 0; j < row_count; j++)
            {
                const float v = ImMax(values[((i + values_offset) * row_count) + j], 0.0f);
                if (v != v)
                    continue;

                t1.y = t0.y - ((v - scale_min) * inv_scale);

                ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, t0);
                ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, t1);
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;

                window->DrawList->AddRectFilled(pos0, pos1, idx_hovered == i && idx_hovered_row == j ? col_hovered : ImU32(row_colors[j % color_count]));

                t0.y = t1.y;
            }
        }
    }

    if (label_size.x > 0.0f)
        ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label.data() );
}

//copied from: https://github.com/ocornut/imgui/issues/2887
void ImGuiEx::MakeTabVisible( const char* window_name )
{
    ImGuiWindow* window = ImGui::FindWindowByName( window_name );
    if( window == NULL || window->DockNode == NULL || window->DockNode->TabBar == NULL )
        return;
    window->DockNode->TabBar->NextSelectedTabId = window->ID;
}

bool ImGuiEx::ToggleButton(const char* label, bool* v)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const auto draw_list = window->DrawList;
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    const float height = ImGui::GetFrameHeight();
    const float width = height * 1.55f;
    const float handle_padding = std::max(style.FramePadding.x, style.FramePadding.y);
    const ImVec2 size = ImVec2(width, height);
    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 handle_size = ImVec2(height - (2.0f * handle_padding), height - (2.0f * handle_padding));
    const ImRect total_bb(pos, pos + ImVec2(width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), std::max(label_size.y, height)));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    ImGui::RenderNavHighlight(total_bb, id);

    if (hovered)
        draw_list->AddRectFilled(pos, pos + size, *v ? ImGui::GetColorU32(ImGuiCol_FrameBgActive, 0.7f) : ImGui::GetColorU32(ImGuiCol_FrameBgHovered), style.FrameRounding);
    else
        draw_list->AddRectFilled(pos, pos + size, *v ? ImGui::GetColorU32(ImGuiCol_ButtonActive, 0.7f) : ImGui::GetColorU32(ImGuiCol_Button), style.FrameRounding);

    const ImVec2 handle_pos = ImVec2(pos.x + (*v ? width - (handle_padding + handle_size.x) : handle_padding), pos.y + handle_padding);
    draw_list->AddRectFilled(handle_pos, handle_pos + handle_size, ImGui::GetColorU32(held ? ImGuiCol_SliderGrabActive : *v ? ImGuiCol_Text : ImGuiCol_SliderGrab), style.FrameRounding);

    ImVec2 label_pos = ImVec2(pos.x + width + style.ItemInnerSpacing.x, pos.y + style.FramePadding.y);
    if (g.LogEnabled)
        ImGui::LogRenderedText(&label_pos, *v ? "[x]" : "[ ]");

    if (label_size.x > 0.0f)
        ImGui::RenderText(label_pos, label);

    return pressed;
}