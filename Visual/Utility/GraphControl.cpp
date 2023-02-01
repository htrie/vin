#include <iomanip>

#include "Common/Utility/Numeric.h"

#include "Visual/Device/WindowDX.h"
#include "Visual/Device/State.h"
#include "Visual/UI/UISystem.h"

#include "GraphControl.h"

namespace
{
	const unsigned caption_height = 15;
	const unsigned units_width = 35;
	const unsigned control_box_size = 4;
	const unsigned variance_width = 10;

	const unsigned sequence_radio_space = 17;
	
	const simd::color sequence_colours[] = {
		simd::color::argb( 255, 255, 0, 0 ),
		simd::color::argb( 255, 0, 255, 0 ),
		simd::color::argb( 255, 0, 0, 255 )
	};
}

GraphControl::GraphControl( float* backing_data, const unsigned backing_data_size, const float min_y_scale, const float max_y_scale, const bool allow_negative, const std::wstring& name, float* variance, const bool read_only, const float base_line, const unsigned num_sequences, const std::wstring* _sequence_names )
	: min_y_bound( min_y_scale ), max_y_bound( min_y_scale ), name( name ), num_control_points( backing_data_size ), data( backing_data ),
	x( 0 ), y( 0 ), width( 100 ), height( 100 ), current_drag( -1 ), variance( variance ), max_y_scale( max_y_scale ), min_y_scale( min_y_scale ), allow_negative( allow_negative ), read_only( read_only ), base_line( base_line )
	, num_sequences( num_sequences ), selected_sequence( 0 )
{
	if( num_sequences > 1 )
		sequence_names.assign( _sequence_names, _sequence_names + num_sequences );

	RecalculateScale();
}



void GraphControl::RecalculateScale( const bool minify )
{
	if( minify )
		max_y_bound = min_y_scale;

	for( unsigned i = 0; i < num_control_points * num_sequences; ++i )
	{
		max_y_bound = std::max( max_y_bound, fabs( data[ i ] ) );
	}

	if( allow_negative )
		min_y_bound = -max_y_bound;
	else
		min_y_bound = 0.0f;

	variance_max = ( max_y_bound - min_y_bound ) / 2.0f;
}

float GraphControl::GetMaxYScale() const
{
	return (max_y_scale_override > 0.f) ? max_y_scale_override : max_y_scale;
}

bool GraphControl::MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( read_only )
		return false;

	Device::WindowDX* window = Device::WindowDX::GetWindow();
	const bool shift_down = !!(window->GetKeyState(VK_SHIFT) & 0x8000);
	const bool ctrl_down = !!(window->GetKeyState(VK_CONTROL) & 0x8000);
	if (shift_down && ctrl_down && max_y_bound <= min_y_scale * 2.f)
		max_y_scale_override = min_y_scale * 2.f;
	else
		max_y_scale_override = 0.f;

#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	if( uMsg == WM_LBUTTONDOWN )
	{
		POINTS points = MAKEPOINTS( lParam );

		std::vector< simd::vector2 > graph_points( num_control_points * num_sequences );
		GenerateGraphPointsScreenSpace( graph_points );

		for( unsigned i = 0; i < num_control_points * num_sequences; ++i )
		{
			if( selected_sequence > 0 && (i / num_control_points + 1) != selected_sequence )
				continue;

			if( points.x > graph_points[ i ].x - 5 && points.x < graph_points[ i ].x + 5 &&
				points.y > graph_points[ i ].y - 5 && points.y < graph_points[ i ].y + 5 )
			{
				current_drag = i;
				Device::WindowDX* window = Device::WindowDX::GetWindow();
				window->SetCapture();
				return true;
			}
		}

		const unsigned graph_width = width - units_width - ( variance ? variance_width : 0 );
		const int graph_min_y = y + caption_height;
		const unsigned graph_height = height - caption_height;
		const int graph_min_x = x + units_width;
		const int graph_max_x = graph_min_x + graph_width;
		const int graph_max_y = y + height;

		if( variance )
		{
			const float variance_y = graph_min_y + graph_height * ( 1.0f - *variance / variance_max );
			const float variance_x = float( graph_max_x + variance_width );

			if( points.x > variance_x - 5 && points.x < variance_x + 5 &&
				points.y > variance_y - 5 && points.y < variance_y + 5 )
			{
				current_drag = num_control_points * num_sequences;
				Device::WindowDX* window = Device::WindowDX::GetWindow();
				window->SetCapture();
				return true;
			}
		}

		if( GetMaxYScale() != min_y_scale )
		{
			const float scale_min_y = float( graph_min_y + caption_height );
			const float scale_max_y = float( graph_max_y - caption_height );
			const float scale_height = scale_max_y - scale_min_y;
			const float scale_proportion = ( max_y_bound - min_y_scale ) / ( GetMaxYScale() - min_y_scale );
			const float scale_x = float( graph_min_x - variance_width );
			const float scale_y = scale_min_y + scale_height * ( 1.0f - scale_proportion );

			if( points.x > scale_x - 5 && points.x < scale_x + 5 &&
				points.y > scale_y - 5 && points.y < scale_y + 5 )
			{
				current_drag = num_control_points * num_sequences + 1;
				Device::WindowDX* window = Device::WindowDX::GetWindow();
				window->SetCapture();
				return true;
			}
		}

		if( num_sequences > 1 )
		{
			for( unsigned seq = 0; seq <= num_sequences; ++seq )
			{
				const int sequence_box_y = graph_min_y + seq * sequence_radio_space + sequence_radio_space;
				const int sequence_box_x = graph_min_x - variance_width;

				if( points.x > sequence_box_x - 5 && points.x < sequence_box_x + 5 &&
					points.y > sequence_box_y - 5 && points.y < sequence_box_y + 5 )
				{
					selected_sequence = seq;
					return true;
				}
			}
		}
	}
	else if( uMsg == WM_MOUSEMOVE && current_drag != -1 )
	{
		POINTS points = MAKEPOINTS( lParam );

		const int graph_min_y = y + caption_height;
		const int graph_max_y = y + height;
		const unsigned graph_height = height - caption_height;
		const float data_bound_size = max_y_bound - min_y_bound;

		const float proportion_y = Saturate( 1.0f - ( float( points.y ) - graph_min_y ) / float( graph_height ) );

		if( current_drag == num_control_points * num_sequences )
		{
			*variance = variance_max * proportion_y;
		}
		else if( current_drag == num_control_points * num_sequences + 1 )
		{
			const float scale_min_y = float( graph_min_y + caption_height );
			const float scale_max_y = float( graph_max_y - caption_height );
			const float scale_height = scale_max_y - scale_min_y;

			const float scale_proportion = Saturate( 1.0f - ( float( points.y ) - scale_min_y ) / scale_height );

			max_y_bound = min_y_scale + scale_proportion * (GetMaxYScale() - min_y_scale);
			RecalculateScale( false );
		}
		else
		{
			float new_value = min_y_bound + data_bound_size * proportion_y;

			const float difference = new_value - data[current_drag];

			for( unsigned seq = 0; seq < num_sequences; ++seq )
			{
				if( num_sequences > 0 && selected_sequence != 0 && seq != (selected_sequence - 1) )
					continue;

				if( wParam & MK_SHIFT )
				{
					for( unsigned i = 0; i < num_control_points; ++i )
					{
						const auto index = seq * num_control_points + i;
						data[index] += difference;
						data[index] = Clamp( data[index], min_y_bound, max_y_bound );
					}
				}
				else
				{
					const auto index = current_drag % num_control_points + seq * num_control_points;
					data[index] += difference;
					data[index] = Clamp( data[index], min_y_bound, max_y_bound );
				}
			}
		}

		graph_changed( this );
		return true;
	}
	else if( uMsg == WM_LBUTTONUP && current_drag != -1 )
	{
		current_drag = -1;
		ReleaseCapture();
		return true;
	}
#endif

	return false;
}

void GraphControl::GenerateGraphPointsScreenSpace( std::vector< simd::vector2 >& output ) const
{
	const float data_bound_size = max_y_bound - min_y_bound;

	const int graph_min_x = x + units_width;
	const int graph_min_y = y + caption_height;
	const unsigned graph_width = width - units_width - ( variance ? variance_width : 0 );
	const unsigned graph_height = height - caption_height;

	for( unsigned seq = 0; seq < num_sequences; ++seq )
	{
		for( unsigned i = 0; i < num_control_points; ++i )
		{
			const float x_proportion = i / float( num_control_points - 1 );
			const float y_proportion = (data[i + seq * num_control_points] - min_y_bound) / data_bound_size;

			output[i + seq * num_control_points].x = graph_min_x + x_proportion * graph_width;
			output[i + seq * num_control_points].y = graph_min_y + (1.0f - y_proportion) * graph_height;
		}
	}
}

void GraphControl::OnRender(bool basic_view /*= false*/ ) const
{
	UI::System::Get().Begin();
	
	//Size of the bound in data units
	const float data_bound_size = max_y_bound - min_y_bound;

	const unsigned graph_width = width - units_width - ( variance ? variance_width : 0 );
	const int graph_min_x = x + units_width;
	const int graph_max_x = graph_min_x + graph_width;
	const int graph_min_y = y + caption_height;
	const int graph_max_y = y + height;
	const unsigned graph_height = height - caption_height;

	//Hide sections for basic view rendering
	if ( !basic_view )
	{
		//Draw the zero line
		const float zero_proportion = ( base_line - min_y_bound ) / data_bound_size;
		if ( zero_proportion > 0.0f && zero_proportion < 1.0f )
		{
			const float zero_y = graph_min_y + ( 1.0f - zero_proportion ) * graph_height;
			const simd::vector2 zero_line [ 2 ] =
			{
				simd::vector2( float( graph_min_x ), zero_y ),
				simd::vector2( float( graph_max_x ), zero_y )
			};


			UI::System::Get().Draw( zero_line, 2, simd::color::argb( 255, 127, 127, 127 ).c() );
		}

		//Draw graph outline
		const simd::vector2 outline [ 5 ] =
		{
			simd::vector2( float( graph_min_x ), float( graph_min_y ) ),
			simd::vector2( float( graph_max_x ), float( graph_min_y ) ),
			simd::vector2( float( graph_max_x ), float( graph_max_y ) ),
			simd::vector2( float( graph_min_x ), float( graph_max_y ) ),
			simd::vector2( float( graph_min_x ), float( graph_min_y ) )
		};

		UI::System::Get().Draw( outline, 5, simd::color::argb( 255, 255, 255, 255 ).c() );
	}

	//Prepare vector of graph points
 	std::vector< simd::vector2 > graph_points( num_control_points * num_sequences );
 	GenerateGraphPointsScreenSpace( graph_points );
 
 	//Draw the graph line
 	for( unsigned seq = 0; seq < num_sequences; ++seq )
 	{
 		UI::System::Get().Draw( &graph_points[seq * num_control_points], num_control_points, (num_sequences > 1 ? sequence_colours[ seq ].c() : simd::color::argb( 255, 255, 255, 255 ).c() ));
 	}
 
 	if( !read_only )
 	{
 		//Draw control points
 		for( unsigned i = 0; i < graph_points.size(); ++i )
 		{
 			//Only display control points if we are editing this sequence at the moment
 			if( selected_sequence > 0 && (i / num_control_points + 1) != selected_sequence )
 				continue;
 
 			//Draw a control box at each point
 			const simd::vector2 control_box[ 5 ] =
 			{
 				simd::vector2( graph_points[ i ].x -control_box_size, graph_points[ i ].y - control_box_size ),
 				simd::vector2( graph_points[ i ].x +control_box_size, graph_points[ i ].y - control_box_size ),
 				simd::vector2( graph_points[ i ].x +control_box_size, graph_points[ i ].y + control_box_size ),
 				simd::vector2( graph_points[ i ].x -control_box_size, graph_points[ i ].y + control_box_size ),
 				simd::vector2( graph_points[ i ].x -control_box_size, graph_points[ i ].y - control_box_size ),
 			};
 
 			UI::System::Get().Draw( control_box, 5, simd::color::argb( 255, 255, 255, 0 ).c() );
 		}
 	}
 	if( variance )
 	{
 		const float variance_y = graph_min_y + graph_height * ( 1.0f - *variance / variance_max );
 		const float variance_x = float( graph_max_x + variance_width );
 
 
 		const simd::vector2 variance_line[ 2 ] =
 		{
 			simd::vector2( variance_x, float( graph_min_y ) ),
 			simd::vector2( variance_x, float( graph_max_y ) )
 		};
         
 		UI::System::Get().Draw( variance_line, 2, simd::color::argb( 255, 255, 255, 255 ).c() );
 
 		if( !read_only )
 		{
 			//Draw a control box at each point
 			const simd::vector2 control_box[ 5 ] =
 			{
 				simd::vector2( variance_x -control_box_size, variance_y - control_box_size ),
 				simd::vector2( variance_x +control_box_size, variance_y - control_box_size ),
 				simd::vector2( variance_x +control_box_size, variance_y + control_box_size ),
 				simd::vector2( variance_x -control_box_size, variance_y + control_box_size ),
 				simd::vector2( variance_x -control_box_size, variance_y - control_box_size ),
 			};
 			
 			UI::System::Get().Draw( control_box, 5, simd::color::argb( 255, 255, 255, 0 ).c() );
 		}
 	}
 
 	if( !read_only && GetMaxYScale() != min_y_scale )
 	{
 		const float scale_min_y = float( graph_min_y + caption_height );
 		const float scale_max_y = float( graph_max_y - caption_height );
 		const float scale_height = scale_max_y - scale_min_y;
 		const float scale_proportion = ( max_y_bound - min_y_scale ) / ( GetMaxYScale() - min_y_scale );
 		const float scale_x = float( graph_min_x - variance_width );
 		const float scale_y = scale_min_y + scale_height * ( 1.0f - scale_proportion );
 
 		const simd::vector2 scale_line[ 2 ] =
 		{
 			simd::vector2( scale_x, scale_min_y ),
 			simd::vector2( scale_x, scale_max_y )
 		};
 
 		UI::System::Get().Draw( scale_line, 2, simd::color::argb( 255, 255, 255, 255 ).c() );
 
 		//Draw a control box at each point
 		const simd::vector2 control_box[ 5 ] =
 		{
 			simd::vector2( scale_x -control_box_size, scale_y - control_box_size ),
 			simd::vector2( scale_x +control_box_size, scale_y - control_box_size ),
 			simd::vector2( scale_x +control_box_size, scale_y + control_box_size ),
 			simd::vector2( scale_x -control_box_size, scale_y + control_box_size ),
 			simd::vector2( scale_x -control_box_size, scale_y - control_box_size ),
 		};
 
 		UI::System::Get().Draw( control_box, 5, simd::color::argb( 255, 255, 255, 0 ).c() );
 	}
 
 	if( !read_only && num_sequences > 1 )
 	{
 		for( unsigned seq = 0; seq <= num_sequences; ++seq )
 		{
 			const float sequence_box_y = float( graph_min_y + seq * sequence_radio_space + sequence_radio_space );
 			const float sequence_box_x = float( graph_min_x - variance_width );
 
 			//Draw a control box at each point
 			const simd::vector2 control_box[5] =
 			{
 				simd::vector2( sequence_box_x - control_box_size, sequence_box_y - control_box_size ),
 				simd::vector2( sequence_box_x + control_box_size, sequence_box_y - control_box_size ),
 				simd::vector2( sequence_box_x + control_box_size, sequence_box_y + control_box_size ),
 				simd::vector2( sequence_box_x - control_box_size, sequence_box_y + control_box_size ),
 				simd::vector2( sequence_box_x - control_box_size, sequence_box_y - control_box_size ),
 			};
 
 			const auto colour = (seq ? sequence_colours[seq - 1].c() : simd::color::argb( 255, 255, 255, 255 ).c());
 			UI::System::Get().Draw( control_box, 5, colour );
 
 			if( seq == selected_sequence )
 			{
 				const simd::vector2 line1[2] = { control_box[0], control_box[2] };
 				UI::System::Get().Draw( line1, 2, colour );
 				const simd::vector2 line2[2] = { control_box[1], control_box[3] };
 				UI::System::Get().Draw( line2, 2, colour );
 			}
 		}
 	}

	UI::System::Get().End();

	if (variance && *variance > 0.0f)
	{
		UI::System::Get().Begin();

		const float screen_space_variance = (*variance / data_bound_size) * graph_height;

		std::vector< simd::vector2 > upper_variance(graph_points);
		std::vector< simd::vector2 > lower_variance(graph_points);
		for (size_t i = 0; i < upper_variance.size(); ++i)
		{
			upper_variance[i].y = std::max(float(graph_min_y), upper_variance[i].y - screen_space_variance);
			lower_variance[i].y = std::min(float(graph_max_y), lower_variance[i].y + screen_space_variance);
		}

		for (unsigned seq = 0; seq < num_sequences; ++seq)
		{
			UI::System::Get().Draw(&upper_variance[seq * num_control_points], num_control_points, (num_sequences > 1 ? sequence_colours[seq].c() : simd::color::argb(255, 255, 50, 50).c()));
			UI::System::Get().Draw(&lower_variance[seq * num_control_points], num_control_points, (num_sequences > 1 ? sequence_colours[seq].c() : simd::color::argb(255, 255, 50, 50).c()));
		}

		UI::System::Get().End();
	}

	//Draw the graph title
	Device::Rect title_pos;
	title_pos.top = y;
	title_pos.left = graph_min_x - 50;
	title_pos.bottom = graph_min_y;
	title_pos.right = graph_max_x + 50;
	UI::System::Get().DrawTextW(12, name.c_str(), &title_pos, DT_SINGLELINE | DT_CENTER, simd::color::argb( 255, 255, 255, 255 ).c() );

	// Hide these sections of font for basic view
	if ( !basic_view )
	{
		std::wstringstream upper_unit_text;
		upper_unit_text << std::setiosflags( std::ios::fixed ) << std::setprecision( 1 ) << max_y_bound;

		Device::Rect upper_unit;
		upper_unit.top = graph_min_y - caption_height / 2;
		upper_unit.left = x;
		upper_unit.right = graph_min_x;
		upper_unit.bottom = graph_min_y + caption_height / 2;
		UI::System::Get().DrawTextW(12, upper_unit_text.str( ).c_str( ), &upper_unit, DT_SINGLELINE | DT_RIGHT, simd::color::argb( 255, 255, 255, 255 ).c() );

		std::wstringstream lower_unit_text;
		lower_unit_text << std::setiosflags( std::ios::fixed ) << std::setprecision( 1 ) << min_y_bound;

		Device::Rect lower_unit;
		lower_unit.top = graph_max_y - caption_height / 2;
		lower_unit.left = x;
		lower_unit.right = graph_min_x;
		lower_unit.bottom = graph_max_y + caption_height / 2;
		UI::System::Get().DrawTextW(12, lower_unit_text.str( ).c_str( ), &lower_unit, DT_SINGLELINE | DT_RIGHT, simd::color::argb( 255, 255, 255, 255 ).c() );
	}

	if( !read_only && num_sequences > 1 )
	{
		for( unsigned seq = 0; seq <= num_sequences; ++seq )
		{
			const unsigned sequence_box_y = graph_min_y + seq * sequence_radio_space + sequence_radio_space;
			const unsigned sequence_box_x = graph_min_x - variance_width;

			Device::Rect sequence_name_pos;
			sequence_name_pos.left = x - 3;
			sequence_name_pos.top = sequence_box_y - caption_height / 2;
			sequence_name_pos.right = sequence_box_x - control_box_size - 3;
			sequence_name_pos.bottom = sequence_box_y + caption_height / 2;

			UI::System::Get().DrawTextW(12, ( seq ? sequence_names[ seq - 1 ].c_str() : L"ALL" ), &sequence_name_pos, DT_SINGLELINE | DT_RIGHT, simd::color::argb( 255, 255, 255, 255 ).c() );
		}
	}

	if( read_only )
	{
		//Display the most recent value to the right of the graph
		std::wstringstream most_recent_value;
		most_recent_value << data[ num_control_points - 1 ];

		Device::Rect recent_pos;
		recent_pos.top = ( graph_max_y + graph_min_y - caption_height ) / 2;
		recent_pos.left = graph_max_x + 2;
		recent_pos.right = recent_pos.left;
		recent_pos.bottom = recent_pos.top;
		UI::System::Get().DrawTextW(12, most_recent_value.str().c_str(), &recent_pos, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, simd::color::argb( 255, 255, 255, 255 ).c() );
	}
}

void GraphControl::OnRender(std::wstring& name_with_value, bool basic_view /*= false*/ ) const
{

	UI::System::Get().Begin( );

	//Size of the bound in data units
	const float data_bound_size = max_y_bound - min_y_bound;

	const unsigned graph_width = width - units_width - ( variance ? variance_width : 0 );
	const int graph_min_x = x + units_width;
	const int graph_max_x = graph_min_x + graph_width;
	const int graph_min_y = y + caption_height;
	const int graph_max_y = y + height;
	const unsigned graph_height = height - caption_height;

	//Hide sections for basic view rendering
	if ( !basic_view )
	{
		//Draw the zero line
		const float zero_proportion = ( base_line - min_y_bound ) / data_bound_size;
		if ( zero_proportion > 0.0f && zero_proportion < 1.0f )
		{
			const float zero_y = graph_min_y + ( 1.0f - zero_proportion ) * graph_height;
			const simd::vector2 zero_line [ 2 ] =
			{
				simd::vector2( float( graph_min_x ), zero_y ),
				simd::vector2( float( graph_max_x ), zero_y )
			};

			UI::System::Get().Draw( zero_line, 2, simd::color::argb( 255, 100, 100, 100 ).c() );
		}

		//Draw graph outline
		const simd::vector2 outline [ 5 ] =
		{
			simd::vector2( float( graph_min_x ), float( graph_min_y ) ),
			simd::vector2( float( graph_max_x ), float( graph_min_y ) ),
			simd::vector2( float( graph_max_x ), float( graph_max_y ) ),
			simd::vector2( float( graph_min_x ), float( graph_max_y ) ),
			simd::vector2( float( graph_min_x ), float( graph_min_y ) )
		};

		UI::System::Get().Draw( outline, 5, simd::color::argb( 255, 255, 255, 255 ).c() );
	}

	//Prepare vector of graph points
	std::vector< simd::vector2 > graph_points( num_control_points * num_sequences );
	GenerateGraphPointsScreenSpace( graph_points );

	//Draw the graph line
	for ( unsigned seq = 0; seq < num_sequences; ++seq )
	{
		UI::System::Get().Draw( &graph_points [ seq * num_control_points ], num_control_points, ( num_sequences > 1 ? sequence_colours [ seq ].c() : simd::color::argb( 255, 255, 255, 255 ).c() ) );
	}

	if ( !read_only )
	{
		//Draw control points
		for ( unsigned i = 0; i < graph_points.size( ); ++i )
		{
			//Only display control points if we are editing this sequence at the moment
			if ( selected_sequence > 0 && ( i / num_control_points + 1 ) != selected_sequence )
				continue;

			//Draw a control box at each point
			const simd::vector2 control_box [ 5 ] =
			{
				simd::vector2( graph_points [ i ].x - control_box_size, graph_points [ i ].y - control_box_size ),
				simd::vector2( graph_points [ i ].x + control_box_size, graph_points [ i ].y - control_box_size ),
				simd::vector2( graph_points [ i ].x + control_box_size, graph_points [ i ].y + control_box_size ),
				simd::vector2( graph_points [ i ].x - control_box_size, graph_points [ i ].y + control_box_size ),
				simd::vector2( graph_points [ i ].x - control_box_size, graph_points [ i ].y - control_box_size ),
			};

			UI::System::Get().Draw( control_box, 5, simd::color::argb( 255, 255, 255, 0 ).c() );
		}
	}
	if ( variance )
	{
		const float variance_y = graph_min_y + graph_height * ( 1.0f - *variance / variance_max );
		const float variance_x = float( graph_max_x + variance_width );


		const simd::vector2 variance_line [ 2 ] =
		{
			simd::vector2( variance_x, float( graph_min_y ) ),
			simd::vector2( variance_x, float( graph_max_y ) )
		};

		UI::System::Get().Draw( variance_line, 2, simd::color::argb( 255, 255, 255, 255 ).c() );

		if ( !read_only )
		{
			//Draw a control box at each point
			const simd::vector2 control_box [ 5 ] =
			{
				simd::vector2( variance_x - control_box_size, variance_y - control_box_size ),
				simd::vector2( variance_x + control_box_size, variance_y - control_box_size ),
				simd::vector2( variance_x + control_box_size, variance_y + control_box_size ),
				simd::vector2( variance_x - control_box_size, variance_y + control_box_size ),
				simd::vector2( variance_x - control_box_size, variance_y - control_box_size ),
			};

			UI::System::Get().Draw( control_box, 5, simd::color::argb( 255, 255, 255, 0 ).c() );
		}
	}

	if ( !read_only && GetMaxYScale() != min_y_scale )
	{
		const float scale_min_y = float( graph_min_y + caption_height );
		const float scale_max_y = float( graph_max_y - caption_height );
		const float scale_height = scale_max_y - scale_min_y;
		const float scale_proportion = ( max_y_bound - min_y_scale ) / ( GetMaxYScale() - min_y_scale );
		const float scale_x = float( graph_min_x - variance_width );
		const float scale_y = scale_min_y + scale_height * ( 1.0f - scale_proportion );

		const simd::vector2 scale_line [ 2 ] =
		{
			simd::vector2( scale_x, scale_min_y ),
			simd::vector2( scale_x, scale_max_y )
		};

		UI::System::Get().Draw( scale_line, 2, simd::color::argb( 255, 255, 255, 255 ).c() );

		//Draw a control box at each point
		const simd::vector2 control_box [ 5 ] =
		{
			simd::vector2( scale_x - control_box_size, scale_y - control_box_size ),
			simd::vector2( scale_x + control_box_size, scale_y - control_box_size ),
			simd::vector2( scale_x + control_box_size, scale_y + control_box_size ),
			simd::vector2( scale_x - control_box_size, scale_y + control_box_size ),
			simd::vector2( scale_x - control_box_size, scale_y - control_box_size ),
		};

		UI::System::Get().Draw( control_box, 5, simd::color::argb( 255, 255, 255, 0 ).c() );
	}

	if ( !read_only && num_sequences > 1 )
	{
		for ( unsigned seq = 0; seq <= num_sequences; ++seq )
		{
			const float sequence_box_y = float( graph_min_y + seq * sequence_radio_space + sequence_radio_space );
			const float sequence_box_x = float( graph_min_x - variance_width );

			//Draw a control box at each point
			const simd::vector2 control_box [ 5 ] =
			{
				simd::vector2( sequence_box_x - control_box_size, sequence_box_y - control_box_size ),
				simd::vector2( sequence_box_x + control_box_size, sequence_box_y - control_box_size ),
				simd::vector2( sequence_box_x + control_box_size, sequence_box_y + control_box_size ),
				simd::vector2( sequence_box_x - control_box_size, sequence_box_y + control_box_size ),
				simd::vector2( sequence_box_x - control_box_size, sequence_box_y - control_box_size ),
			};

			const auto colour = ( seq ? sequence_colours [ seq - 1 ].c() : simd::color::argb( 255, 255, 255, 255 ).c() );
			UI::System::Get().Draw( control_box, 5, colour );

			if ( seq == selected_sequence )
			{
				const simd::vector2 line1 [ 2 ] = { control_box [ 0 ], control_box [ 2 ] };
				UI::System::Get().Draw( line1, 2, colour );
				const simd::vector2 line2 [ 2 ] = { control_box [ 1 ], control_box [ 3 ] };
				UI::System::Get().Draw( line2, 2, colour );
			}
		}
	}

	UI::System::Get().End( );

	if ( variance && *variance > 0.0f )
	{
		UI::System::Get().Begin( );

		const float screen_space_variance = ( *variance / data_bound_size ) * graph_height;

		std::vector< simd::vector2 > upper_variance( graph_points );
		std::vector< simd::vector2 > lower_variance( graph_points );
		for ( size_t i = 0; i < upper_variance.size( ); ++i )
		{
			upper_variance [ i ].y = std::max( float( graph_min_y ), upper_variance [ i ].y - screen_space_variance );
			lower_variance [ i ].y = std::min( float( graph_max_y ), lower_variance [ i ].y + screen_space_variance );
		}

		for ( unsigned seq = 0; seq < num_sequences; ++seq )
		{
			UI::System::Get().Draw( &upper_variance [ seq * num_control_points ], num_control_points, ( num_sequences > 1 ? sequence_colours [ seq ].c() : simd::color::argb( 255, 255, 255, 255 ).c() ) );
			UI::System::Get().Draw( &lower_variance [ seq * num_control_points ], num_control_points, ( num_sequences > 1 ? sequence_colours [ seq ].c() : simd::color::argb( 255, 255, 255, 255 ).c() ) );
		}

		UI::System::Get().End( );
	}

	//Draw the graph title
	Device::Rect title_pos;
	title_pos.top = y;
	title_pos.left = graph_min_x;
	title_pos.bottom = graph_min_y;
	title_pos.right = graph_max_x;
	UI::System::Get().DrawTextW(12, name_with_value.c_str( ), &title_pos, DT_SINGLELINE | DT_CENTER, simd::color::argb( 255, 255, 255, 255 ).c() );

	// Hide these sections of font for basic view
	if ( !basic_view )
	{
		std::wstringstream upper_unit_text;
		upper_unit_text << std::setiosflags( std::ios::fixed ) << std::setprecision( 1 ) << max_y_bound;

		Device::Rect upper_unit;
		upper_unit.top = graph_min_y - caption_height / 2;
		upper_unit.left = x;
		upper_unit.right = graph_min_x;
		upper_unit.bottom = graph_min_y + caption_height / 2;
		UI::System::Get().DrawTextW(12, upper_unit_text.str( ).c_str( ), &upper_unit, DT_SINGLELINE | DT_RIGHT, simd::color::argb( 255, 255, 255, 255 ).c() );

		std::wstringstream lower_unit_text;
		lower_unit_text << std::setiosflags( std::ios::fixed ) << std::setprecision( 1 ) << min_y_bound;

		Device::Rect lower_unit;
		lower_unit.top = graph_max_y - caption_height / 2;
		lower_unit.left = x;
		lower_unit.right = graph_min_x;
		lower_unit.bottom = graph_max_y + caption_height / 2;
		UI::System::Get().DrawTextW(12, lower_unit_text.str( ).c_str( ), &lower_unit, DT_SINGLELINE | DT_RIGHT, simd::color::argb( 255, 255, 255, 255 ).c() );
	}

	if ( !read_only && num_sequences > 1 )
	{
		for ( unsigned seq = 0; seq <= num_sequences; ++seq )
		{
			const unsigned sequence_box_y = graph_min_y + seq * sequence_radio_space + sequence_radio_space;
			const unsigned sequence_box_x = graph_min_x - variance_width;

			Device::Rect sequence_name_pos;
			sequence_name_pos.left = x - 3;
			sequence_name_pos.top = sequence_box_y - caption_height / 2;
			sequence_name_pos.right = sequence_box_x - control_box_size - 3;
			sequence_name_pos.bottom = sequence_box_y + caption_height / 2;

			UI::System::Get().DrawTextW(12, ( seq ? sequence_names [ seq - 1 ].c_str( ) : L"ALL" ), &sequence_name_pos, DT_SINGLELINE | DT_RIGHT, simd::color::argb( 255, 255, 255, 255 ).c() );
		}
	}
}
