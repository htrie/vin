#pragma once

#include "Common/Utility/Event.h"
#include "Visual/Device/Constants.h"

namespace simd
{
	class vector2;
}

namespace Device
{
	class Font;
	class Line;
	class SpriteBatch;
}

///Represents a graph control that can have segments moved up and down
class GraphControl
{
public:
	///@param backing_data is modified directly by the control.
	///@param backing_data_size will be used to control how many control points the graph contains
	///@param variance is an optional parameter that will add a variance control the graph if it is supplied. Works like backing_data.
	///@param num_sequences means that there will be multiple sequences in the source data to edit simultaniously.
	///		for example, R, G, B values on a single graph.
	GraphControl( float* backing_data, const unsigned backing_data_size,
		const float min_y_scale, const float max_y_scale, const bool allow_negative, const std::wstring& name, float* variance = NULL, const bool read_only = false, const float base_line = 0.0f, const unsigned num_sequences = 1, const std::wstring* sequence_names = NULL );

	void SetPosition( int _x, int _y ) { x = _x; y = _y; }
	void SetSize( unsigned _width, unsigned _height ) { width = _width; height = _height; }
	const float GetMostRecentValue( ) const { return data [ num_control_points - 1 ]; }

	void OnRender(Device::SpriteBatch* sprite_batch, Device::Line* line, Device::Font* font, bool basic_view = false) const;
	void OnRender(Device::SpriteBatch* sprite_batch, Device::Line* line, Device::Font* font, std::wstring& name_with_value, bool basic_view = false ) const;
	bool MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	void RecalculateScale( const bool minify = true );

	///Event for user modification of the graph.
	Event< GraphControl* > graph_changed;

	///@name Unchanging control data
	//@{
	const std::wstring name;
	const unsigned num_control_points;
	const float max_y_scale, min_y_scale;
	const bool allow_negative;
	const bool read_only;
	const float base_line;
	const unsigned num_sequences;
	//@}
private:

	std::vector< std::wstring > sequence_names;

	float min_y_bound, max_y_bound;
	float variance_max;

	void GenerateGraphPointsScreenSpace( std::vector< simd::vector2 >& output ) const;

	///Current values manipulated by the control
	float *data;
	
	///Position information
	int x, y;
	unsigned width, height;

	unsigned current_drag;

	unsigned selected_sequence;

	float* variance;

	float max_y_scale_override = 0.f;
	float GetMaxYScale() const;
};