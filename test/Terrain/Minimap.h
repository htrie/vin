#pragma once

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/VertexDeclaration.h"

#include "ClientInstanceCommon/Game/Components/MinimapIcon.h"

#include "MinimapTilePack.h"
#include "MinimapRendering.h"

namespace Device
{
	class IndexBuffer;
	class VertexBuffer;
	class VertexDeclaration;
	class Texture;
	class RenderTarget;
	class Shader;
}

namespace Terrain
{
	class TileMap;
	class MinimapDataSource;

	class Minimap
	{
	public:
		Minimap( const MinimapDataSource& data_source, const std::wstring& cache_file, Game::MinimapIconIOBase& poi_base );
		~Minimap();

		void SetMapGeometryAlpha(const float geometry_alpha) { this->geometry_alpha = geometry_alpha; }
		void SetMapWalkabilityAlpha(const float walkability_alpha) { this->walkability_alpha = walkability_alpha; }

		void OnCreateDevice( Device::IDevice* device );
		///Use this overload if you want to provide all the tiles to be used rather than let the class find them on the file system.
		void OnCreateDevice( Device::IDevice* device, const std::shared_ptr< MinimapTilePack >& minimap_tile_pack );
		void OnResetDevice( Device::IDevice* device, const unsigned screen_width, const unsigned screen_height );
		void OnLostDevice();
		void OnDestroyDevice();

		void ReloadShaders(Device::IDevice* device);

		void SetAllVisible();
		void SetWalkableVisible();
		void SetVisible( const Vector2d& tile );
		void SetVisibleBound( const simd::vector4& tile_bound );
		void SetWalkableVisibleBound( const simd::vector4& tile_bound );
		void UploadTileVisibility();
		const simd::vector2& GetXBasis() const { return x_basis; }
		const simd::vector2& GetYBasis() const { return y_basis; }
		const simd::vector2& GetZBasis() const { return z_basis; }

		///A point of interest on the minimap
		///The position is in world space.
		typedef MinimapRendering::InterestPoint InterestPoint; //position, type, scale, fades out boolean

		void RenderToTexture(
			const simd::vector3& center_position,
			const simd::vector2& screen_offset,
			const float map_scale,
			const Device::Rect& rect,
			MinimapRendering::RectType rect_type,
			const unsigned subgraph_index,
			const MinimapRendering::DecayInfo& decay_info = {},
			const MinimapRendering::RoyaleInfo& royale_info = {} );

		void Render( 
			const simd::vector3& center_position,
			const simd::vector2& screen_offset,
			const float map_scale,
			const Device::Rect& rect,
			MinimapRendering::RectType rect_type,
			const unsigned subgraph_index,
			Memory::Vector<Terrain::Minimap::InterestPoint, Memory::Tag::UI>::iterator interest_points_begin,
			Memory::Vector<Terrain::Minimap::InterestPoint, Memory::Tag::UI>::iterator interest_points_end,
			const bool should_draw_arrows,
			const bool clip_to_rect,
			const MinimapRendering::DecayInfo& decay_info = {},
			const MinimapRendering::RoyaleInfo& royale_info = {} );

		const MinimapDataSource& data_source;
		const Location minimap_size;

		int walkability_vertices_count = 0;
		int walkability_indices_count = 0;

		int tilemap_vertices_count = 0;
		int tilemap_indices_count = 0;

		bool WasLoadedFromCache() const { return loaded_from_cache; }

	private:
		void LoadCacheFile();
		void SaveCacheFile() const;

		std::shared_ptr< MinimapTilePack > CreateTilePack( Device::IDevice* device );
		void CreateWalkabilityMesh(Device::IDevice * device);
		void CreateTilemapMesh(Device::IDevice* device);
		void CreateTilemapMeshCheap(Device::IDevice* device);
		void CreatePOIVertices( Device::IDevice* device );
		void CreatePOIIndices( Device::IDevice* device );

		float geometry_alpha = 0.7f;
		float walkability_alpha = 0.7f;

		Game::MinimapIconIOBase& poi_base;

		Device::IDevice* device = nullptr;
		unsigned screen_width = 1, screen_height = 1;

		///Basis vectors to translate from world space to minimap space.
		simd::vector2 x_basis, y_basis, z_basis;

		bool tile_visibility_walkable_revealed = false;
		bool tile_visibility_fully_revealed = false;
		bool tile_visibility_reset = true;
		bool loaded_from_cache = false;

		std::optional<Vector2d> last_explored;
		std::optional< simd::vector4 > tile_bound_revealed;

		std::shared_ptr< MinimapTilePack > minimap_tile_pack;

		Device::Handle<Device::VertexBuffer> tilemap_vertices;
		Device::Handle<Device::IndexBuffer> tilemap_indices;
		Device::Handle<Device::VertexBuffer> walkability_vertices;
		Device::Handle<Device::IndexBuffer> walkability_indices;

		Device::Handle<Device::VertexBuffer> imagespace_vertices;

		Device::VertexDeclaration base_vertex_declaration;
		Device::VertexDeclaration textured_vertex_declaration;
		Device::VertexDeclaration imagespace_vertex_declaration;

		Device::Handle< Device::Texture > player_cross_texture;
		Device::Handle< Device::Texture > walkability_texture;
		std::unique_ptr< Device::RenderTarget > visibility_render_target;
		std::unique_ptr< Device::RenderTarget > visibility_render_target_copy;
		std::unique_ptr< Device::RenderTarget > lockable_target;

		std::unique_ptr< Device::RenderTarget > projected_walkability_target;
		std::unique_ptr< Device::RenderTarget > projected_decay_target;
		std::unique_ptr< Device::RenderTarget > projected_geometry_target;

		Device::Handle<Device::Pass> projected_walkability_pass;
		Device::Handle<Device::Pass> projected_geometry_pass;
		Device::Handle<Device::Pass> visibility_pass;

		Device::Handle<Device::VertexBuffer> vb_points_of_interest;
		Device::Handle<Device::IndexBuffer> ib_points_of_interest;

		struct BaseVertexShader
		{
			Device::Handle<Device::Shader> vertex_shader;
			Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;
		} base_vertex_shader;

		struct TexturedVertexShader
		{
			Device::Handle<Device::Shader> vertex_shader;
			Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;
		} textured_vertex_shader;

		struct PoiVertexShader
		{
			Device::Handle<Device::Shader> vertex_shader;
			Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;
		} poi_vertex_shader;

		struct TilePixelShader
		{
			Device::Handle<Device::Shader> pixel_shader;
			Device::Handle<Device::Pipeline> pipeline_textured;
			Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
		} tile_pixel_shader;

		struct WalkabilityShader
		{
			Device::Handle<Device::Shader> pixel_shader;
			Device::Handle<Device::Pipeline> pipeline;
			Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
		} walkability_shader;

		struct ImagespaceShader
		{
			Device::Handle<Device::Shader> vertex_shader;
			Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;
		} imagespace_shader;

		struct VisibilityShader
		{
			Device::Handle<Device::Shader> pixel_shader;
			Device::Handle<Device::Pipeline> pipeline;
			Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
		} visibility_shader;

		struct BlendingShader
		{
			Device::Handle<Device::Shader> pixel_shader;
			Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
		} blending_shader;

	#pragma pack(push)	
	#pragma pack(1)
		struct PipelineKey
		{
			Device::PointerID<Device::Pass> pass;
			Device::PrimitiveType primitive_type;
			Device::PointerID<Device::VertexDeclaration> vertex_declaration;
			Device::PointerID<Device::Shader> vertex_shader;
			Device::PointerID<Device::Shader> pixel_shader;
			Device::BlendState blend_state;
			Device::RasterizerState rasterizer_state;
			Device::DepthStencilState depth_stencil_state;

			PipelineKey() noexcept {}
			PipelineKey(Device::Pass* pass, Device::PrimitiveType primitive_type, Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state);
		};
	#pragma pack(pop)
		Device::Cache<PipelineKey, Device::Pipeline> pipelines;
		Device::Pipeline* FindPipeline(const std::string& name, Device::Pass* pass, Device::PrimitiveType primitive_type, Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state);

	#pragma pack(push)
	#pragma pack(1)
		struct BindingSetKey
		{
			Device::PointerID<Device::Shader> shader;
			uint32_t inputs_hash = 0;

			BindingSetKey() noexcept {}
			BindingSetKey(Device::Shader* shader, uint32_t inputs_hash);
		};
	#pragma pack(pop)
		Device::Cache<BindingSetKey, Device::DynamicBindingSet> binding_sets;
		Device::DynamicBindingSet* FindBindingSet(const std::string& name, Device::IDevice* device, Device::Shader* pixel_shader, const Device::DynamicBindingSet::Inputs& inputs);

	#pragma pack(push)
	#pragma pack(1)
		struct DescriptorSetKey
		{
			Device::PointerID<Device::Pipeline> pipeline;
			Device::PointerID<Device::DynamicBindingSet> pixel_binding_set;
			uint32_t samplers_hash = 0;

			DescriptorSetKey() noexcept {}
			DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash);
		};
	#pragma pack(pop)
		Device::Cache<DescriptorSetKey, Device::DescriptorSet> descriptor_sets;
		Device::DescriptorSet* FindDescriptorSet(const std::string& name, Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set);
		
		float icon_tex_size_ratio = 1.0f; // height:width
		::Texture::Handle crack_sdm_texture;
		std::wstring cache_filename;
	};

}
