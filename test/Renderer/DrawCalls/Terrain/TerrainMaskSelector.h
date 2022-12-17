#pragma once

#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/HashUtil.h"

namespace Renderer
{
	namespace DrawCalls
	{
		namespace Terrain
		{
			struct MaskTransform
			{
				MaskTransform( ) { };
				MaskTransform( const simd::vector4&_row_a, const simd::vector4&_row_b ) : row_a( _row_a ), row_b( _row_b ) { }
				simd::vector4 row_a;
				simd::vector4 row_b;
			};

			class TerrainMaskSelector
			{
			public:
				TerrainMaskSelector( );

				struct MaskPoints
				{
					unsigned char side_a, side_b;
					unsigned char point_a, point_b;

					operator size_t( ) const
					{
						return MurmurHash2( this, sizeof( MaskPoints ), 0);
					}

					//			bool operator<( const MaskPoints &other ) const
					//			{
					//				return memcmp( this, &other, sizeof( MaskPoints ) ) < 0;
					//			}
				};

				const MaskTransform& GetMaskTransform( const MaskPoints& ) const;

				HASH_TYPE PointHash
				{
				public:
					size_t operator()(const MaskPoints& op) const
					{
						return op;
					}
				};


			private:
				void AddPermutations( const MaskPoints& mask_points, const MaskTransform& mask_transform );

				typedef std::unordered_map< MaskPoints, MaskTransform, PointHash > MaskTransforms_t;
				MaskTransforms_t mask_transforms;
			};

		} // namespace Terrain

	} // namespace DrawCalls

} // namespace Renderer

//EOF
