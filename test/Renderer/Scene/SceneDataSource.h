#pragma once

#include "Common/Utility/NonCopyable.h"

#include "Visual/Device/Constants.h"
#include "Visual/Material/Material.h"
#include "WaterMarkupData.h"

namespace Mesh { class TileMeshInstance; }

namespace Device
{
	class IDevice;
}

class BoundingBox;
struct Location;
namespace Renderer
{
	namespace Lighting { class ILight; }

	namespace Scene
	{
		struct SynthesisMapData
		{
			SynthesisMapData() noexcept : size( -1, -1 ), min_point( 0.0f, 0.0f ), max_point( 0.0f, 0.0f ) {}
			SynthesisMapData(const float *decay_data, const float *stable_data, simd::vector2_int size, simd::vector2 min_point, simd::vector2 max_point) : decay_data(decay_data), stable_data(stable_data), size(size), min_point(min_point), max_point(max_point) {}
			const float* decay_data = nullptr;
			const float* stable_data = nullptr;
			simd::vector2_int size;
			simd::vector2 min_point, max_point;
		};

		struct WaterMapData
		{
			WaterMapData() noexcept : size( -1, -1 ) {}
			WaterMapData( const float* flow_data, simd::vector2_int size ) : flow_data( flow_data ), size( size ) {}
			const float* flow_data = nullptr;
			simd::vector2_int size;
		};

		struct FogBlockingData
		{
			FogBlockingData() noexcept : size( -1, -1 ) {}
			FogBlockingData(const unsigned char *blocking_data, simd::vector2_int size, simd::vector2 min_point, simd::vector2 max_point) : blocking_data(blocking_data), size(size) {}
			
			const unsigned char *blocking_data = nullptr;

			struct SourceConstraint
			{
				simd::vector2 world_pos;
				float radius;
				float source;
			};
			const SourceConstraint* source_constraints = nullptr;
			size_t source_constraints_count = 0;

			simd::vector2_int size;
		};

		struct BlightMapData
		{
			BlightMapData() noexcept : size(-1, -1) {}
			std::vector<float> blight_data;
			simd::vector2_int size;
		};

		struct RoomOcclusionData
		{
			struct Room
			{
				float x1, y1, x2, y2;
				float opacity;
			};
			std::vector< Room > rooms;
		};

		class SceneDataSource : NonCopyable
		{
		public:
			SceneDataSource() noexcept { }
			virtual ~SceneDataSource() { };

			virtual simd::vector2 GetSceneSize() const = 0;
			virtual simd::vector2 GetSceneLocation( ) const = 0;
			virtual float GetTileMapWorldHeight(const Location& loc) const { return 0.0f; }
			virtual float GetTileMapCustomData( const Location& loc ) const { return 0; }
			virtual const SynthesisMapData GetSynthesisMapData() const { return SynthesisMapData(); }
			virtual const WaterMapData GetWaterMapData() const { return WaterMapData(); }
			virtual const BlightMapData GetBlightMapData() const { return BlightMapData(); }
			virtual const FogBlockingData GetFogBlockingData() const { return FogBlockingData(); }
			virtual const OceanData GetOceanData() const { return OceanData(); }
			virtual const RiverData GetRiverData() const { return RiverData(); }
			virtual float GetNodeSize() const = 0;
			virtual MaterialHandle GetGroundMaterial(const unsigned x, const unsigned y) const { return MaterialHandle(); }

			virtual void OnCreateDevice( Device::IDevice *device ) = 0;
			virtual void OnDestroyDevice( ) = 0;

			virtual void SetupSceneObjects() = 0;
			virtual void DestroySceneObjects() = 0;
		};
	}
}
