#pragma once

#include "Common/Utility/Geometric.h"

namespace Device
{
	class Texture;
	struct Rect;
	class SpriteBatch;
}

Device::Rect MakeRect( int x, int y, int width, int height );
void DrawRepeatedSpriteSection( Device::SpriteBatch *const sprite, Device::Texture *const texture, const Device::Rect &section, const Vector2d position, const Vector2d size, const float screen_height, const float screen_width );
void DrawSpriteSection(	Device::SpriteBatch *const sprite, Device::Texture *const texture, const Device::Rect &section, const Vector2d position, const float screen_height, const float screen_width );
void DrawSpriteSection(	Device::SpriteBatch *const sprite, Device::Texture *const texture, const Device::Rect &section, const Vector2d position, const float screen_height, const float screen_width, const unsigned int colour );
void DrawSizedSprite( Device::SpriteBatch *const sprite, Device::Texture *const texture, const Vector2d position, const Vector2d size, const float screen_height, const float screen_width );

extern Device::Rect InterfaceBackgroundRects[ 2 ];