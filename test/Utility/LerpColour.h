#pragma once

#include "Common/Utility/Numeric.h"
#include "Visual/dxut/dxstdafx.h"

inline DWORD Lerp( const DWORD a, const DWORD b, const float x )
{
	return D3DCOLOR_COLORVALUE( 
		Lerp( ((a&0x00ff0000)>>16) / 255.0f, ((b&0x00ff0000)>>16) / 255.0f, x ),
		Lerp( ((a&0x0000ff00)>>8 ) / 255.0f, ((b&0x0000ff00)>>8 ) / 255.0f, x ),
		Lerp( ((a&0x000000ff)    ) / 255.0f, ((b&0x000000ff)    ) / 255.0f, x ),
		Lerp( ((a&0xff000000)>>24) / 255.0f, ((b&0xff000000)>>24) / 255.0f, x )
		);
}