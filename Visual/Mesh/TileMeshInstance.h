#pragma once

#include "Common/Utility/Bitset.h"
#include "Common/Utility/Orientation.h"
#include "Visual/Device/State.h"
#include "Visual/Material/Material.h"
#include "Visual/Renderer/Scene/Light.h"

#include "TileMesh.h"

namespace Terrain { typedef Resource::Handle< class TileMaterialOverrides > TileMaterialOverridesHandle; }

namespace Entity
{
	class Desc;
}

namespace Mesh
{

	enum GroundCases : uint8_t
	{
		GroundCase1A,
		GroundCase2A,
		GroundCase3A,
		GroundCase4A,
		GroundCase1A1B,
		GroundCase2A1B,
		GroundCase2A2B,
		GroundCase3A1B,
		GroundCase1A1B1C,
		GroundCase1A1C,
		GroundCase2A1C,
		GroundCase3A1C,
		NumCases
	};

	///Exception that indicates that a ground case could not be created for the material layout
	class MaterialLayoutUnsupported : public RuntimeError
	{
	public:
		MaterialLayoutUnsupported(const std::string& error) : RuntimeError("Material layout unsupported: " + error) { }
	};

	///An instance of a tile mesh with position and ground materials
	class TileMeshInstance
	{
	public:
		///Creates an empty tile
		TileMeshInstance() noexcept {}
		///Creates a tile from a tile mesh
		TileMeshInstance( const TileMeshChildHandle& tile_mesh );
		//Allow move but no copy
		TileMeshInstance( TileMeshInstance&& other ) noexcept { *this = std::move( other ); }
		TileMeshInstance& operator=( TileMeshInstance&& other ) noexcept;
		TileMeshInstance( const TileMeshInstance& ) = delete;
		TileMeshInstance& operator=( const TileMeshInstance& other ) = delete;
		~TileMeshInstance();

		///Should be called SetPosition
		///This can no longer be called after SetupSceneObjects has been called. If you want to update the world matrix after SetupSceneObjects, you must call DestroySceneObjects and then SetupSceneObjects again
		///@param x,y are in tile coordinates not world space.
		void UpdateWorldMatrix( const float x, const float y, const Orientation_t orientation, const int unit_height, const unsigned char tallwall_code );
		void UpdateWorldMatrix( const float x, const float y, const Orientation_t orientation, const int unit_height ) { UpdateWorldMatrix( x, y, orientation, unit_height, GetTallWallDirectionCode( orientation ) ); }
		void SetDynamicBoundingBox( const bool is_dynamic = true ) { flags.set( DynamicBoundingBox, is_dynamic ); }
		bool HasDynamicBoundingBox() const { return flags.test( DynamicBoundingBox ); }

		void SetupSceneObjects(uint8_t scene_layers);
		void DestroySceneObjects();
		bool HaveSceneObjectsBeenSetUp() const { return flags.test( SceneIsSetUp ); }
		void RecreateSceneObjects( uint8_t scene_layers ) { if( HaveSceneObjectsBeenSetUp() ) { DestroySceneObjects(); SetupSceneObjects( scene_layers ); } }

		// Returns the number of draw calls that this TileMeshInstance has or will have. Does not need to actually create the draw calls beforehand.
		size_t GetDrawCallCount() const;
		// Returns the number of draw calls that use shadow-enabled blend modes. Does not need to actually create the draw calls beforehand.
		size_t GetShadowDrawCallCount() const;

		void RecreateEntities();

		///@name Ground Materials
		//@{
		///Sets a single ground material covering the entire tile
		void SetMaterial( const MaterialHandle& material, const unsigned char *_ground_indices );
		///Sets materials at all corners.
		///@param new_materials is an array of 4 materials, one for each corner
		///@param tile_x, tile_y are used to make random generation of blend connection points consistant across the tile map.
		///@param _blend_texture is used to blend between materials in the tile.
		void SetMaterials( const MaterialHandle* new_materials, const int tile_x, const int tile_y, const unsigned char* _ground_indices, const unsigned char* _connection_points, const bool force_recreate_entities = false ) { SetMaterials( new_materials, MaterialHandle(), tile_x, tile_y, _ground_indices, _connection_points, float( tile_x ), float( tile_y ), force_recreate_entities ); }
		///Sets materials at all corners and in the center of the tile.
		void SetMaterials( const MaterialHandle* new_materials, const MaterialHandle& center, const int tile_x, const int tile_y, const unsigned char* _ground_indices, const unsigned char* _connection_points, const bool force_recreate_entities = false ) { SetMaterials( new_materials, center, tile_x, tile_y, _ground_indices, _connection_points, float( tile_x ), float( tile_y ), force_recreate_entities ); }
		void SetMaterials( const MaterialHandle* new_materials, const MaterialHandle& center, const int tile_x, const int tile_y, const unsigned char* _ground_indices, const unsigned char* _connection_points, const float ftile_x, const float ftile_y, const bool force_recreate_entities = false );
		//@}

		bool HasUserBlendMask() const;
		bool HasTileMeshFlag( const SegmentFlags::Flags flag ) const;
		bool HasNoOverlapGeo() const { return HasTileMeshFlag( SegmentFlags::NoOverlap ); }
		bool HasUniqueMeshGeo() const { return HasTileMeshFlag( SegmentFlags::IsUniqueMesh ); }
		bool HasTallWallGeo() const { return HasTileMeshFlag( SegmentFlags::IsTallWall ); }
		bool HasWater() const { return water_level != std::numeric_limits< float >::infinity(); }
		float GetWaterLevel() const { return water_level; }

		const TileMeshChildHandle& GetMesh() const { return subtile_mesh; }
		const simd::matrix& GetWorldMatrix() const { return world; }
		MaterialHandle GetMaterial( const Location& at ) const;

		void SetMaterialOverrides( Terrain::TileMaterialOverridesHandle tmo ) { material_overrides = std::move( tmo ); }
		void SetUniqueMeshSeed( const unsigned short seed ) { unique_mesh_seed = seed; }
		bool OverrideUniqueMeshSet( const unsigned char set, const unsigned char variation );
		bool OverrideUniqueMeshSets( const std::array< unsigned char, 4 >& data );
		void SetNoOverlap( const bool north, const bool south, const bool east, const bool west ) { flags.set( DisallowNoOverlapNorth, north ); flags.set( DisallowNoOverlapSouth, south ); flags.set( DisallowNoOverlapEast, east ); flags.set( DisallowNoOverlapWest, west ); }
		std::tuple< bool, bool, bool, bool > GetNoOverlapFlags() const { return { flags.test( DisallowNoOverlapNorth ), flags.test( DisallowNoOverlapSouth ), flags.test( DisallowNoOverlapEast ), flags.test( DisallowNoOverlapWest ) }; }

		unsigned short GetUniqueMeshSeed() const { return unique_mesh_seed; }

		//AssetViewer
		void ForEachVisibleGroundSegment( const std::function<void( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material )>& func ) const;
		void ForEachVisibleNormalSegment( const std::function<void( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material )>& func ) const;
#ifdef TILE_MESH_VISIBILITY
		bool GetNormalSegmentVisibility( const size_t index ) const { return normal_segment_visibility.at( index ); }
		void SetNormalSegmentVisibility( const size_t index, const bool visible ) { normal_segment_visibility[ index ] = visible; }
#endif
	protected:
		void SetMesh( const TileMeshChildHandle& tile_mesh );
		void SetBlendMask( const ::Texture::Handle& mask );

	private:
		void ProcessSegments( const std::function<void( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& material )>& process, const std::function<void( const TileMeshRawData::MeshSectionSegment& mesh_segment )>& next = []( const TileMeshRawData::MeshSectionSegment& ) {} ) const;
		bool ShouldRenderSegment( const TileMeshRawData::MeshSectionSegment& segment, const TileMeshRawData::MeshSection& section ) const;

		TileMeshChildHandle subtile_mesh;
		Memory::SmallVector<MaterialHandle, 16, Memory::Tag::Mesh> ground_section_materials;
		
		float water_level = std::numeric_limits< float >::infinity();

		unsigned short unique_mesh_seed = 0;
		unsigned char unique_mesh_variation_overrides = 0; //4x 2bit numbers
		unsigned char tallwall_direction_code = 0;

		enum Flags
		{
			SwapGMask,
			DynamicBoundingBox,
			SceneIsSetUp,
			DisallowNoOverlapSouth,
			DisallowNoOverlapEast,
			DisallowNoOverlapNorth,
			DisallowNoOverlapWest,
			OverrideUniqueSet0,
			OverrideUniqueSet1,
			OverrideUniqueSet2,
			OverrideUniqueSet3,
			NumFlags
		};
		Utility::Bitset< NumFlags > flags;

		GroundCases ground_case = GroundCase1A;

		simd::matrix world;
		Device::CullMode cull_mode = Device::CullMode::NONE;

		::Texture::Handle blend_mask;

		simd::vector4 blend1_uv_transform_a;
		simd::vector4 blend1_uv_transform_b;
		simd::vector4 blend2_uv_transform_a;
		simd::vector4 blend2_uv_transform_b;

	protected:
		class MergedMeshSectionSegment
		{
		public:
			MergedMeshSectionSegment( const TileMeshRawData::MeshSectionSegment& segment, const MaterialHandle& segment_material );
			void Merge( const TileMeshRawData::MeshSectionSegment& segment );
			void Merge( const MergedMeshSectionSegment& segment );
			bool HasNoDrawCall() const;

			uint64_t entity_id = 0;
			uint64_t wetness_entity_id = 0;
			BoundingBox bounding_box;

			Device::Handle<Device::IndexBuffer> index_buffer;
			unsigned index_count = 0;
			unsigned base_index = 0;
			const TileMeshRawData::MeshSectionSegment* segment = nullptr;
			const MaterialHandle segment_material;
			///This indicates that this section should have no shadows
			///This is done because the all the opaque geometry can be merged in to one draw call
			///which will be created seperately
			bool override_no_shadow = false;
			bool shadow_only = false;
		};

		const auto& GetMeshSegments() const { return segments; }

	private:
		static bool HasWetnessPass(const MergedMeshSectionSegment& merged_segment);

		MaterialHandle GetNormalSegmentMaterial( const unsigned char segment_material_index ) const;

		void DestroyEntities();

		Entity::Desc CreateGroundEntityDesc(const TileMeshRawData::MeshSection& mesh, const BoundingBox& bounding_box);
		Entity::Desc CreateNormalEntityDesc(const TileMeshRawData::MeshSection& mesh, const MergedMeshSectionSegment& merged_segment, const BoundingBox& bounding_box);
		Entity::Desc CreateWetnessEntityDesc(const TileMeshRawData::MeshSection& mesh, const MergedMeshSectionSegment& merged_segment, const BoundingBox& bounding_box);

		BoundingBox untransformed_bounding_box;

		uint64_t ground_entity_id = 0;

		uint8_t scene_layers = 0;

		Memory::Vector< MergedMeshSectionSegment, Memory::Tag::Mesh > segments;

		Memory::SmallVector<std::shared_ptr<::Renderer::Scene::Light>, 8, Memory::Tag::Mesh> lights;

		Terrain::TileMaterialOverridesHandle material_overrides;

#ifdef TILE_MESH_VISIBILITY
		Memory::Vector<bool, Memory::Tag::Mesh> normal_segment_visibility;
#endif
	};

}