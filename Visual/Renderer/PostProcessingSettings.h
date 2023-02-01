#pragma once

#include <istream>
#include <ostream>

#include "Visual/Resource/TextureResource.h"

namespace Renderer
{
	struct PostProcessingSettings
	{
		PostProcessingSettings( ) : original_intensity(1), bloom_intensity(0.3f), bloom_cutoff(0.3f) {}
		PostProcessingSettings( std::wistream& stream );

		void Lerp( const PostProcessingSettings* source, const PostProcessingSettings& destination, float alpha );

		float original_intensity, bloom_intensity, bloom_cutoff;
	};

	std::wostream& operator<<( std::wostream& stream, const PostProcessingSettings& settings );

	struct PostTransformSettings
	{
		PostTransformSettings( ) : post_transform_enabled( false ) { }
		PostTransformSettings( std::wistream& stream );
		bool post_transform_enabled;
		TextureHandle transform_texture;
	};

	std::wostream& operator<<( std::wostream& stream, const PostTransformSettings& settings );
}