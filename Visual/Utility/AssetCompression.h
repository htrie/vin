#pragma once

#include <memory>
#include <string>

namespace AssetCompress
{
	// Compresses and replaces the file if successful
	bool CompressFile( const std::wstring& path );
	void SetCompressionEnabled( const bool value );
	bool IsCompressionEnabled();
}