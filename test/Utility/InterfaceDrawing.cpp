#include "Visual/Utility/InterfaceDrawing.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/SpriteBatch.h"
#include "Visual/Device/Texture.h"

void DrawRepeatedSpriteSection( Device::SpriteBatch *const sprite, Device::Texture *const texture, const Device::Rect &section, const Vector2d position, const Vector2d size, const float screen_height, const float screen_width )
{
	const float source_width = float( section.right - section.left );
	const float source_height = float( section.bottom - section.top );

	const float scale_factor = screen_height / 1600.0f;

	auto scale = simd::matrix::scale(simd::vector3( scale_factor, scale_factor, 1.0f ));

	const float num_source_in_destination_x = (size.x * screen_width ) / ( source_width * scale_factor );
	const float num_source_in_destination_y = (size.y * screen_height) / ( source_height * scale_factor );
	const int num_x_iterations = int( num_source_in_destination_x ) + 1;
	const int num_y_iterations = int( num_source_in_destination_y ) + 1;

	for( int y = 0; y < num_y_iterations; ++y )
		for( int x = 0; x < num_x_iterations; ++x )
		{
			Device::Rect current_rect = section;
			if( y == num_y_iterations - 1 )
			{
				current_rect.bottom = current_rect.top + int((num_source_in_destination_y - int( num_source_in_destination_y ) ) * source_height);
			}
			if( x == num_x_iterations - 1 )
			{
				current_rect.right = current_rect.left + int((num_source_in_destination_x - int( num_source_in_destination_x ) ) * source_width);
			}

			auto translation = simd::matrix::translation(simd::vector3( position.x * screen_width + x * ( source_width * scale_factor ), position.y * screen_height + y * ( source_height * scale_factor ), 0.0f ));

			const auto transform = scale * translation;
			sprite->SetTransform( &transform );
			sprite->Draw( texture, &current_rect, NULL, NULL, simd::color::rgba(255,255,255,255).c() );
		}
}

void DrawSpriteSection(	Device::SpriteBatch *const sprite, Device::Texture *const texture, const Device::Rect &section, const Vector2d position, const float screen_height, const float screen_width, const unsigned int colour )
{
	const float source_width = float( section.right - section.left );
	const float source_height = float( section.bottom - section.top );

	const float scale_factor = screen_height / 1600.0f;

	auto scale = simd::matrix::scale(simd::vector3( scale_factor, scale_factor, 1.0f ));
	auto translation = simd::matrix::translation(simd::vector3( position.x * screen_width, position.y * screen_height, 0.0f ));

	const auto transform = scale * translation;
	sprite->SetTransform( &transform );
	sprite->Draw( texture, &section, NULL, NULL, colour );
}

void DrawSpriteSection(	Device::SpriteBatch *const sprite, Device::Texture *const texture, const Device::Rect &section, const Vector2d position, const float screen_height, const float screen_width )
{
	DrawSpriteSection( sprite, texture, section, position, screen_height, screen_width, simd::color::rgba(255,255,255,255).c());
}

void DrawSizedSprite( Device::SpriteBatch *const sprite, Device::Texture *const texture, const Vector2d position, const Vector2d size, const float screen_height, const float screen_width )
{
	Device::SurfaceDesc description;
	texture->GetLevelDesc( 0, &description );
	auto scaling = simd::matrix::scale(simd::vector3( (size.x * screen_width) / description.Width, ( size.y * screen_height ) / description.Height, 1.0f ));
	auto translation = simd::matrix::translation(simd::vector3( position.x * screen_width, position.y * screen_height, 0.0f ));
	const auto transformation = scaling * translation;
	sprite->SetTransform( &transformation );
	sprite->Draw( texture, NULL, NULL, NULL, simd::color::rgba( 255, 255, 255, 255 ).c());
}

Device::Rect MakeRect( int x, int y, int width, int height )
{
	Device::Rect rect;
	rect.left = x;
	rect.right = x + width;
	rect.top = y;
	rect.bottom = y + height;
	return rect;
}

Device::Rect InterfaceBackgroundRects[ 2 ] =
{
	MakeRect( 4, 4, 256, 256 ),
	MakeRect( 264, 4, 256, 256 )
};
