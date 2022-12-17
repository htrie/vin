
#include "Common/File/PathHelper.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/BufferStream.h"
#include "Common/Utility/TupleHash.h"
#include "Common/Utility/lz4.h"
#include "Common/Loaders/CEMinimapIconsEnum.h"
#include "Common/Engine/IOStatus.h"

#include "Visual/Terrain/MinimapDataSource.h"
#include "Visual/Renderer/GlobalShaderDeclarations.h"
#include "Visual/Renderer/CachedHLSLShader.h"
#include "Visual/Renderer/Utility.h"
#include "Visual/Renderer/TextureMapping.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/TargetResampler.h"
#include "Visual/Renderer/Blit.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Shader.h"

#include "ClientInstanceCommon/Terrain/TileMap.h"
#include "ClientInternal/Game/PointOfInterest.h"

#include "Minimap.h"

namespace Terrain
{
	using namespace MinimapRendering;

	namespace
	{
		float GetTexSizeRatio( Device::Texture* tex )
		{
			Device::SurfaceDesc desc;
			tex->GetLevelDesc( 0, &desc );
			return float( desc.Height ) / desc.Width;
		}

		struct MapBaseVertex
		{
			MapBaseVertex() noexcept {}
			MapBaseVertex(const simd::vector2& tile_index, const float& height, const float& subgraph_index) : tile_index(tile_index), height(height), subgraph_index(subgraph_index) { }

			simd::vector2 tile_index = 0.0f;
			float height = 0.0f;
			float subgraph_index = 0;
		};

		struct MapTexturedVertex
		{
			MapTexturedVertex() noexcept {}
			MapTexturedVertex(const simd::vector2& tile_index, const float& height, const simd::vector2& uv, const simd::vector2& projection_offset, const float& subgraph_index)
				: tile_index(tile_index), height(height), uv(uv), projection_offset(projection_offset), subgraph_index(subgraph_index) { }

			simd::vector2 tile_index = 0.0f;
			float height = 0.0f;
			simd::vector2 uv = 0.0f;
			simd::vector2 projection_offset = 0.0f;
			float subgraph_index = 0;
		};

		#pragma pack( push, 1 )
		struct ImagespaceVertex
		{
			simd::vector4 viewport_pos = 0.0f;
			simd::vector2 tex_pos = 0.0f;
		};
		#pragma pack( pop )

		const Device::VertexElements minimap_base_vertex_elements =
		{
			{ 0, offsetof(MapBaseVertex, tile_index), Device::DeclType::FLOAT2, Device::DeclUsage::POSITION, 0 },
			{ 0, offsetof(MapBaseVertex, height),     Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 0 },
			{ 0, offsetof(MapBaseVertex, subgraph_index),     Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 1 },
		};

		const Device::VertexElements minimap_textured_vertex_elements =
		{
			{ 0, offsetof(MapTexturedVertex, tile_index),        Device::DeclType::FLOAT2, Device::DeclUsage::POSITION, 0 },
			{ 0, offsetof(MapTexturedVertex, height),            Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 0 },
			{ 0, offsetof(MapTexturedVertex, uv),                Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 1 },
			{ 0, offsetof(MapTexturedVertex, projection_offset), Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 2 },
			{ 0, offsetof(MapTexturedVertex, subgraph_index),     Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 3 },
		};

		const Device::VertexElements imagespace_vertex_elements =
		{
			{ 0, offsetof(ImagespaceVertex, viewport_pos), Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
			{ 0, offsetof(ImagespaceVertex, tex_pos), Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0 },
		};
	}

	namespace MinimapTexUtil
	{
		struct ColorR8G8B8A8
		{
			unsigned char r = 0, g = 0, b = 0, a = 0;
			ColorR8G8B8A8() noexcept {}
			ColorR8G8B8A8(unsigned char r, unsigned char g, unsigned char b, unsigned char a) : r(r), g(g), b(b), a(a) {}
			bool operator ==(const ColorR8G8B8A8 &other)
			{
				return r == other.r && g == other.g && b == other.b && a == other.a;
			}
		};
	}

	Minimap::PipelineKey::PipelineKey(Device::Pass* pass, Device::PrimitiveType primitive_type, Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state)
		: pass(pass), primitive_type(primitive_type), pixel_shader(pixel_shader), blend_state(blend_state), rasterizer_state(rasterizer_state), depth_stencil_state(depth_stencil_state)
	{}

	Minimap::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
		: shader(shader), inputs_hash(inputs_hash)
	{}

	Minimap::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
		: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
	{}

	Minimap::Minimap( const MinimapDataSource& data_source, const std::wstring& cache_file, Game::MinimapIconIOBase& poi_base )
		: data_source( data_source )
		, minimap_size( data_source.GetMinimapSize() )
		, cache_filename( PathHelper::NormalisePath(cache_file) )
		, poi_base( poi_base )
		, base_vertex_declaration(minimap_base_vertex_elements)
		, textured_vertex_declaration(minimap_textured_vertex_elements)
		, imagespace_vertex_declaration(imagespace_vertex_elements)
	{
		//Work out the basis vectors for the tile map space.
		const simd::vector2 x_unit( float( WorldSpaceTileSize ), 0.0f ), y_unit( 0.0f, float( WorldSpaceTileSize ) );
		const simd::vector3 z_unit( 0.0f, 0.0f, UnitToWorldHeightScale );
		x_basis = MinimapViewMatrix.mulpos(x_unit);
		y_basis = MinimapViewMatrix .mulpos(y_unit);
		const simd::vector3 temp_z_basis = MinimapViewMatrix.mulpos(z_unit);
		z_basis = simd::vector2( temp_z_basis[0], -temp_z_basis[1] );
	}

	Device::Pipeline* Minimap::FindPipeline(const std::string& name, Device::Pass* pass, Device::PrimitiveType primitive_type, Device::VertexDeclaration* vertex_declaration, Device::Shader* vertex_shader, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state)
	{
		return pipelines.FindOrCreate(PipelineKey(pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state), [&]()
		{
			return Device::Pipeline::Create(name, device, pass, primitive_type, vertex_declaration, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state);
		}).Get();
	}

	Device::DynamicBindingSet* Minimap::FindBindingSet(const std::string& name, Device::IDevice* device, Device::Shader* pixel_shader, const Device::DynamicBindingSet::Inputs& inputs)
	{
		return binding_sets.FindOrCreate(BindingSetKey(pixel_shader, inputs.Hash()), [&]()
		{
			return Device::DynamicBindingSet::Create(name, device, pixel_shader, inputs);
		}).Get();
	}

	Device::DescriptorSet* Minimap::FindDescriptorSet(const std::string& name, Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set)
	{
		return descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
		{
			return Device::DescriptorSet::Create(name, device, pipeline, {}, { pixel_binding_set });
		}).Get();
	}

	void Minimap::OnCreateDevice( Device::IDevice* device, const std::shared_ptr< MinimapTilePack >& tile_pack )
	{
		this->device = device;
		
		minimap_tile_pack = tile_pack;

		CreateTilemapMesh(device);
		//CreateTilemapMeshCheap(device);

		CreateWalkabilityMesh( device );

		ImagespaceVertex vertices[4] =
		{
			{ simd::vector4(-1.f, -1.f, 0.0f, 1.0f), simd::vector2(0.0f, 1.0f) },
			{ simd::vector4(-1.f,  1.f, 0.0f, 1.0f), simd::vector2(0.0f, 0.0f) },
			{ simd::vector4( 1.f, -1.f, 0.0f, 1.0f), simd::vector2(1.0f, 1.0f) },
			{ simd::vector4( 1.f,  1.f, 0.0f, 1.0f), simd::vector2(1.0f, 0.0f) },
		};

		Device::MemoryDesc memory{};
		memory.pSysMem = vertices;
		memory.SysMemPitch = UINT(sizeof(ImagespaceVertex));
		memory.SysMemSlicePitch = UINT(std::size(vertices) * memory.SysMemPitch);

		imagespace_vertices = Device::VertexBuffer::Create("VB Minimap ImageSpace", device, 4 * sizeof(ImagespaceVertex), Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, &memory);

		crack_sdm_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/2DArt/Decay/crack_sdm.dds"));

		ReloadShaders(device);

	#if defined(PS4)
		// Texture size at mip level 0 must be divisible by 8 when aliased by a RenderTarget.
		// And force alignment to 128 to prevent tiling/filtering issues when copying the visibility data.
		const int visibility_map_width = (minimap_size.x + 127) & ~127;
		const int visibility_map_height = (minimap_size.y + 127) & ~127;
	#elif defined(_XBOX_ONE)
		// Seems to prevent driver crash on Xbox.
		const int visibility_map_width = std::max(128, minimap_size.x);
		const int visibility_map_height = std::max(128, minimap_size.y);
	#else
		const int visibility_map_width = minimap_size.x;
		const int visibility_map_height = minimap_size.y;
	#endif

		visibility_render_target = Device::RenderTarget::Create("Visibility", device, visibility_map_width, visibility_map_height, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, false, false);
		visibility_render_target_copy = Device::RenderTarget::Create("Visibility Copy", device, visibility_map_width, visibility_map_height, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, false, false);
		lockable_target = Device::RenderTarget::Create("Lockable Visibility", device, visibility_map_width, visibility_map_height, visibility_render_target->GetFormat(), Device::Pool::SYSTEMMEM, false, false);

		visibility_pass = Device::Pass::Create("Visibility", device, { visibility_render_target.get() }, nullptr, Device::ClearTarget::NONE, simd::color(0), 0.0f);

		LoadCacheFile();
	}

	void Minimap::OnCreateDevice( Device::IDevice* device )
	{
		OnCreateDevice( device, CreateTilePack( device ) );
	}

	void Minimap::OnDestroyDevice()
	{
		SaveCacheFile();

		tilemap_vertices.Reset();
		tilemap_indices.Reset();

		walkability_vertices.Reset();
		walkability_indices.Reset();

		device = NULL;

		visibility_render_target.reset();
		visibility_render_target_copy.reset();
		lockable_target.reset();

		visibility_pass.Reset();

		minimap_tile_pack.reset();

		base_vertex_shader.vertex_shader.Reset();
		base_vertex_shader.vertex_uniform_buffer.Reset();
		textured_vertex_shader.vertex_shader.Reset();
		textured_vertex_shader.vertex_uniform_buffer.Reset();

		base_vertex_shader.vertex_shader.Reset();
		base_vertex_shader.vertex_uniform_buffer.Reset();
		poi_vertex_shader.vertex_shader.Reset();
		poi_vertex_shader.vertex_uniform_buffer.Reset();
		textured_vertex_shader.vertex_shader.Reset();
		textured_vertex_shader.vertex_uniform_buffer.Reset();
		tile_pixel_shader.pixel_shader.Reset();
		tile_pixel_shader.pixel_uniform_buffer.Reset();
		walkability_shader.pixel_shader.Reset();
		walkability_shader.pixel_uniform_buffer.Reset();
		imagespace_shader.vertex_shader.Reset();
		imagespace_shader.vertex_uniform_buffer.Reset();
		visibility_shader.pixel_shader.Reset();
		visibility_shader.pixel_uniform_buffer.Reset();
		blending_shader.pixel_shader.Reset();
		blending_shader.pixel_uniform_buffer.Reset();

		imagespace_vertices.Reset();

		crack_sdm_texture = ::Texture::Handle();
	}

	template<typename ColorType>
	void CopyFromWalkabilityData( const MinimapDataSource::WalkabilityData& src_data, void* ptr, int row_stride )
	{
		::TexUtil::LockedData<ColorType> dst_locked_data(ptr, row_stride);

		for (int y = 0; y < src_data.height; y++)
		{
			for (int x = 0; x < src_data.width; x++)
			{
				const unsigned char color = src_data.data[x + y * src_data.width];
				dst_locked_data.Get(::simd::vector2_int(x, y)) =
					ColorType(color, color, color, 255);
			}
		}
	}

	void Minimap::OnResetDevice(Device::IDevice* device, const unsigned screen_width, const unsigned screen_height)
	{
		this->screen_width = screen_width;
		this->screen_height = screen_height;

		projected_walkability_target = Device::RenderTarget::Create("Projected Walkability", device, screen_width, screen_height, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, false, false);
		projected_geometry_target = Device::RenderTarget::Create("Projected Geometry", device, screen_width, screen_height, Device::PixelFormat::A8R8G8B8, Device::Pool::DEFAULT, true, false);

		player_cross_texture = Utility::CreateTexture(device, L"Art/2DArt/minimap/player.png", Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, true);
		icon_tex_size_ratio = GetTexSizeRatio(player_cross_texture.Get());

		const MinimapDataSource::WalkabilityData walkability_data = data_source.GetWalkabilityData();
		assert(walkability_data.data && walkability_data.width > 0 && walkability_data.height > 0);

		{
			Memory::Vector<unsigned char, Memory::Tag::Device> walkability_texture_data(walkability_data.width * walkability_data.height * 4);
			CopyFromWalkabilityData<MinimapTexUtil::ColorR8G8B8A8>(walkability_data, walkability_texture_data.data(), walkability_data.width * 4);
			walkability_texture = Device::Texture::CreateTextureFromMemory("Walkability", device, walkability_data.width, walkability_data.height, walkability_texture_data.data(), 4);
		}

		projected_walkability_pass = Device::Pass::Create("Projected Walkability", device, { projected_walkability_target.get() }, nullptr, Device::ClearTarget::COLOR, simd::color(0), 1.0f);
		projected_geometry_pass = Device::Pass::Create("Projected Geometry", device, { projected_geometry_target.get() }, nullptr, Device::ClearTarget::COLOR, simd::color(0), 1.0f);

		walkability_shader.pipeline = Device::Pipeline::Create("Minimap Walkability", device, projected_walkability_pass.Get(), Device::PrimitiveType::TRIANGLELIST, &base_vertex_declaration, base_vertex_shader.vertex_shader.Get(), walkability_shader.pixel_shader.Get(),
			Device::DisableBlendState(), Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0), Device::UIDepthStencilState().SetDepthState(Device::DisableDepthState()));

		tile_pixel_shader.pipeline_textured = Device::Pipeline::Create("Minimap Textured", device, projected_geometry_pass.Get(), Device::PrimitiveType::TRIANGLELIST, &textured_vertex_declaration, textured_vertex_shader.vertex_shader.Get(), tile_pixel_shader.pixel_shader.Get(),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)),
			Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0),
			Device::UIDepthStencilState().SetDepthState(Device::DisableDepthState()));

		visibility_shader.pipeline = Device::Pipeline::Create("Minimap Visibility", device, visibility_pass.Get(), Device::PrimitiveType::TRIANGLESTRIP, &imagespace_vertex_declaration, imagespace_shader.vertex_shader.Get(), visibility_shader.pixel_shader.Get(),
			Device::DisableBlendState(), Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0), Device::UIDepthStencilState().SetDepthState(Device::DisableDepthState()));

		CreatePOIVertices(device);
		CreatePOIIndices(device);
	}

	void Minimap::OnLostDevice()
	{
		descriptor_sets.Clear();
		binding_sets.Clear();
		pipelines.Clear();

		projected_walkability_pass.Reset();
		projected_geometry_pass.Reset();

		walkability_shader.pipeline.Reset();
		tile_pixel_shader.pipeline_textured.Reset();
		visibility_shader.pipeline.Reset();

		projected_walkability_target.reset();
		projected_geometry_target.reset();

		player_cross_texture.Reset();

		vb_points_of_interest.Reset();
		ib_points_of_interest.Reset();
		walkability_texture.Reset();
	}


	std::shared_ptr< MinimapTilePack > Minimap::CreateTilePack( Device::IDevice* device )
	{
		PROFILE;

		Memory::FlatSet< MinimapKey, Memory::Tag::Game > tiles;
		const Utility::Bound bound( minimap_size );
		std::for_each( bound.begin(), bound.end(), [&]( const Location& loc )
		{
			TileDescriptionHandle tile;
			Orientation::Orientation ori;
			std::tie( tile, ori, std::ignore, std::ignore, std::ignore, std::ignore ) = data_source.GetMinimapTile( loc );
			if( tile.IsNull() )
				return;

			tiles.insert( MinimapKey( tile, ToMinimapOrientation[ ori ] ) );
		});

		return CreateTilePackFromExistingPacks( tiles, device );
//#ifdef DEBUG
//		minimap_tile_pack->Save( Log::GetLogDirectoryW() + L"/current_tile_pack.mtp" );
//#endif

	}


	class TopologySorter
	{
	public:
		struct Link
		{
			size_t srcObjectIndex = 0;
			size_t dstObjectIndex = 0;
		};
		void ProcessGraph(const Link *links, const size_t linksCount, size_t *resultObjectIndices, size_t objectsCount)
		{
			totalIterationsCount = 0;
			LoadLinks(links, linksCount, objectsCount);
			//processing data
			std::vector<int> objectIndices; //stack for dfs

			size_t resultObjectsCount = 0;
			for (size_t objectIndex = 0; objectIndex < objects.size(); objectIndex++)
			{
				DeepSearch(objectIndex, resultObjectIndices, resultObjectsCount);
			}
			assert(resultObjectsCount == objects.size());
		}
	private:
		void DeepSearch(size_t objectIndex, size_t *resultObjectIndices, size_t &resultObjectsCount)
		{
			totalIterationsCount++;
			if (objects[objectIndex].isAdded)
				return;

			objects[objectIndex].isAdded = true;
			const Object &currObject = objects[objectIndex];
			for (size_t neighbourNumber = 0; neighbourNumber < currObject.neighboursCount; neighbourNumber++)
			{
				const size_t neighbourIndex = neighbourIndices[currObject.neighboursOffset + neighbourNumber];
				DeepSearch(neighbourIndex, resultObjectIndices, resultObjectsCount);
			}
			resultObjectIndices[resultObjectsCount++] = objectIndex;
		}
		void LoadLinks(const Link *links, const size_t linksCount, size_t objectsCount)
		{
/*			size_t maxIndex = 0;
			for (size_t linkIndex = 0; linkIndex < linksCount; linkIndex++)
			{
				maxIndex = std::max(maxIndex, links[linkIndex].srcObjectIndex);
				maxIndex = std::max(maxIndex, links[linkIndex].dstObjectIndex);
			}
			objectsCount = std::max(maxIndex + 1, objectsCount);*/
			objects.resize(objectsCount);

			size_t totalNeighboursCount = 0;
			for (size_t linkIndex = 0; linkIndex < linksCount; linkIndex++)
			{
				objects[links[linkIndex].srcObjectIndex].neighboursCount++;
				totalNeighboursCount++;
			}

			neighbourIndices.resize(totalNeighboursCount);

			size_t currOffset = 0;
			for (size_t objectIndex = 0; objectIndex < objectsCount; objectIndex++)
			{
				objects[objectIndex].neighboursOffset = currOffset;
				currOffset += objects[objectIndex].neighboursCount;
				objects[objectIndex].neighboursCount = 0;
				objects[objectIndex].isAdded = false;
			}

			for (size_t linkIndex = 0; linkIndex < linksCount; linkIndex++)
			{
				auto &srcObject = objects[links[linkIndex].srcObjectIndex];
				neighbourIndices[srcObject.neighboursOffset + srcObject.neighboursCount++] = links[linkIndex].dstObjectIndex;
			}
		}
		struct Object
		{
			size_t neighboursOffset = -1;
			size_t neighboursCount = 0;
			bool isAdded = false;
		};
		std::vector<Object> objects;
		std::vector<size_t> neighbourIndices;
		int totalIterationsCount = 0;
	};

	void Minimap::CreateTilemapMesh( Device::IDevice* device )
	{
		const simd::vector2 pack_image_size( float( minimap_tile_pack->GetWidth() ), float( minimap_tile_pack->GetHeight() ) );

		const MinimapTilePackIndex pack_index( *minimap_tile_pack );

		std::vector<MapTexturedVertex > vertices;
		std::vector<unsigned int > indices;

		//We iterate along diagonal rows starting from 0,0 and moving outwards for correct Z ordering.
		const int width = minimap_size.x;
		const int height = minimap_size.y;

		struct MinimapTileDesc
		{
			TileDescriptionHandle tile;
			Orientation_t ori = Orientation::I;
			bool is_centered = false;
			Location actual_location;
			Location size;
			int tile_height = 0;
			int tile_index = 0;
			unsigned subgraph_index = 0;
			std::pair< const PackedMinimapTile*, bool > pack_find_result;
		};
		const auto blank_tile = GetSpecialTile( Loaders::SpecialTilesValues::Blank );
		const auto ReadMinimapTile = [&](Location loc) -> MinimapTileDesc
		{
			MinimapTileDesc res_tile;
			std::tie(res_tile.tile, res_tile.ori, res_tile.actual_location, res_tile.size, res_tile.tile_height, res_tile.subgraph_index) = data_source.GetMinimapTile(loc);
			res_tile.tile_index = res_tile.actual_location.x + res_tile.actual_location.y * width;

			if (!res_tile.tile.IsNull())
			{
				res_tile.pack_find_result = pack_index.Find(res_tile.tile, res_tile.ori);

				res_tile.is_centered = (Location(res_tile.actual_location.x, res_tile.actual_location.y) == Location(loc.x, loc.y));
				if (res_tile.pack_find_result.first == nullptr || !res_tile.tile->ShowOnMinimap())
				{
					res_tile.pack_find_result = pack_index.Find(blank_tile, Orientation::I);
					res_tile.is_centered = true;
					res_tile.actual_location = loc;
					res_tile.size = Location(1, 1);
					res_tile.ori = Orientation::I;
				}
			}
			return res_tile;
		};

		std::vector<TopologySorter::Link> topology_links;

		const auto ProcessLink = [&](int src_tile_index, Location dst_tile_loc)
		{
			if (dst_tile_loc.x < 0 || dst_tile_loc.y < 0 || dst_tile_loc.x >= width || dst_tile_loc.y >= height)
				return;

			MinimapTileDesc dst_tile_desc = ReadMinimapTile(dst_tile_loc);
			if (dst_tile_desc.tile.IsNull())
				return;

			TopologySorter::Link topology_link{};
			topology_link.srcObjectIndex = src_tile_index;
			topology_link.dstObjectIndex = dst_tile_desc.tile_index;
			topology_links.push_back(topology_link);
		};

		const auto minimap_bound = Utility::Bound( minimap_size );
		for( const Location& tile_loc : minimap_bound )
		{
			MinimapTileDesc curr_tile_desc = ReadMinimapTile( tile_loc );
			if( curr_tile_desc.tile.IsNull() || !curr_tile_desc.is_centered ) 
				continue;

			constexpr int shadow_length = 5;

			for (int x = 0; x < curr_tile_desc.size.x + 1; x++)
			{
				const Location src_loc = Location(curr_tile_desc.actual_location.x + x, curr_tile_desc.actual_location.y + curr_tile_desc.size.y);
				for (int offset = 0; offset < shadow_length; offset++)
				{
					const Location dst_loc = Location(src_loc.x + offset, src_loc.y + offset);
					ProcessLink(curr_tile_desc.tile_index, dst_loc);
				}
			}
			for (int y = 0; y < curr_tile_desc.size.y; y++)
			{
				const Location src_loc = Location(curr_tile_desc.actual_location.x + curr_tile_desc.size.x, curr_tile_desc.actual_location.y + y);
				for (int offset = 0; offset < shadow_length; offset++)
				{
					const Location dst_loc = Location(src_loc.x + offset, src_loc.y + offset);
					ProcessLink(curr_tile_desc.tile_index, dst_loc);
				}
			}
		}

		const int tiles_count = width * height;

		std::vector<size_t> tile_indices;
		tile_indices.resize( tiles_count );
		TopologySorter topology_sorter;
		topology_sorter.ProcessGraph(topology_links.data(), topology_links.size(), tile_indices.data(), tiles_count);
		for(int tile_number = 0; tile_number < width * height; tile_number++)
		{
			const int tile_index = (int)tile_indices[tile_number];
			//If the tile is a multi tile then we only want to make a minimap image for its origin tile.

			const Location curr_location = Location(tile_index % width, tile_index / width);
			MinimapTileDesc curr_tile_desc = ReadMinimapTile(curr_location);
			if (!curr_tile_desc.tile.IsNull() && curr_tile_desc.pack_find_result.first && curr_tile_desc.is_centered)
			{
				const bool flip_tile = Flips(curr_tile_desc.ori);
				const auto& pack_location = *(curr_tile_desc.pack_find_result.first);

				simd::vector2 center_position = x_basis * float(curr_tile_desc.actual_location.x) + y_basis * float(curr_tile_desc.actual_location.y) + z_basis * float(curr_tile_desc.tile_height);
				center_position.y = -center_position.y;
				const simd::vector2 center_uv(pack_location.center_x/* + 0.5f*/, pack_location.center_y/* + 0.5f*/);

				float positive_x = pack_location.width_columns * MinimapTileWidth - pack_location.offset_x;
				float negative_x = -pack_location.offset_x;
				const float positive_y = pack_location.height_pixels - pack_location.offset_y;
				const float negative_y = -pack_location.offset_y;

				//If we are rendering a flipped version of the tile then we need to flip the uv coordinates
				const float uv_positive_x = (flip_tile ? negative_x : positive_x);
				const float uv_negative_x = (flip_tile ? positive_x : negative_x);
				if (flip_tile)
				{
					//We also need to flip the positioning but without turning the triangles around.
					std::swap(positive_x, negative_x);
					positive_x = -positive_x;
					negative_x = -negative_x;
				}

				//Add the four vertices wound clockwise------
				const auto planar_center_pos = simd::vector2(float(curr_tile_desc.actual_location.x), float(curr_tile_desc.actual_location.y));

				const int start_vertex = int(vertices.size());
				vertices.push_back(MapTexturedVertex(
					planar_center_pos,
					float(curr_tile_desc.tile_height),
					(center_uv + simd::vector2(uv_negative_x, negative_y)) / pack_image_size,
					simd::vector2(negative_x, negative_y) * MinimapProjectionRatio,
					float(curr_tile_desc.subgraph_index)
				));

				vertices.push_back(MapTexturedVertex(
					planar_center_pos,
					float(curr_tile_desc.tile_height),
					(center_uv + simd::vector2(uv_positive_x, negative_y)) / pack_image_size,
					simd::vector2(positive_x, negative_y) * MinimapProjectionRatio,
					float(curr_tile_desc.subgraph_index)
				));

				vertices.push_back(MapTexturedVertex(
					planar_center_pos,
					float(curr_tile_desc.tile_height),
					(center_uv + simd::vector2(uv_positive_x, positive_y)) / pack_image_size,
					simd::vector2(positive_x, positive_y) * MinimapProjectionRatio,
					float(curr_tile_desc.subgraph_index)
				));

				vertices.push_back(MapTexturedVertex(
					planar_center_pos,
					float(curr_tile_desc.tile_height),
					(center_uv + simd::vector2(uv_negative_x, positive_y)) / pack_image_size,
					simd::vector2(negative_x, positive_y) * MinimapProjectionRatio,
					float(curr_tile_desc.subgraph_index)
				));

				indices.push_back(start_vertex + 0);
				indices.push_back(start_vertex + 1);
				indices.push_back(start_vertex + 2);

				indices.push_back(start_vertex + 2);
				indices.push_back(start_vertex + 3);
				indices.push_back(start_vertex + 0);
			}
		}

		Device::MemoryDesc sysMemDesc;

		memset( &sysMemDesc, 0, sizeof( sysMemDesc ) );
		sysMemDesc.pSysMem = vertices.data();
		static_assert(sizeof(unsigned int) == 4, "Unsigned int assumed to be size 4");
		tilemap_vertices_count = int(vertices.size());
		if (vertices.size() > 0)
			tilemap_vertices = Device::VertexBuffer::Create("VB Minimap Tilemap", device, UINT(vertices.size()) * sizeof(MapTexturedVertex), Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, &sysMemDesc);
		else
			tilemap_vertices.Reset();


		memset(&sysMemDesc, 0, sizeof(sysMemDesc));
		sysMemDesc.pSysMem = indices.data();
		tilemap_indices_count = int(indices.size());
		if (indices.size() > 0)
			tilemap_indices = Device::IndexBuffer::Create("IB Minimap Tilemap", device, UINT(indices.size()) * sizeof(unsigned int), Device::UsageHint::DEFAULT, Device::IndexFormat::_32, Device::Pool::DEFAULT, &sysMemDesc);
		else
			tilemap_indices.Reset();
	}

	void Minimap::CreateTilemapMeshCheap(Device::IDevice* device)
	{
		const simd::vector2 pack_image_size(float(minimap_tile_pack->GetWidth()), float(minimap_tile_pack->GetHeight()));

		const MinimapTilePackIndex pack_index(*minimap_tile_pack);

		std::vector<MapTexturedVertex > vertices;
		std::vector<unsigned int > indices;

		//We iterate along diagonal rows starting from 0,0 and moving outwards for correct Z ordering.
		unsigned index = 0;
		const int width = minimap_size.x;
		const int height = minimap_size.y;
		for (int diag = width + height - 1; diag >= 0; --diag)
		{
			int x = std::min(diag, width - 1);
			int y = std::max(0, diag - width + 1);
			for (; y < height && x >= 0; --x, ++y)
			{
				TileDescriptionHandle tile;
				Orientation::Orientation ori;
				Location actual_location;
				Location size;
				int tile_height;
				unsigned subgraph_index;
				std::tie(tile, ori, actual_location, size, tile_height, subgraph_index) = data_source.GetMinimapTile(Location(x, y));

				//If the tile is a multi tile then we only want to make a minimap image for its origin tile.

				//bool condition = GetAsyncKeyState('T') ? (Location(actual_location.x + size.x - 1, actual_location.y + size.y - 1) == Location(x, y)) : (Location(actual_location.x, actual_location.y) == Location(x, y));
				//if (!tile.IsNull() && condition)
				if (!tile.IsNull() && Location(actual_location.x, actual_location.y) == Location(x, y))
				{
					const auto found = pack_index.Find(tile, ori);

					//Only make minimap tiles for images that were in the minimap tile pack.
					if (found.first != nullptr && tile->ShowOnMinimap())
					{
						const bool flip_tile = Flips(ori);
						const auto& pack_location = *found.first;

						simd::vector2 center_position = x_basis * float(actual_location.x) + y_basis * float(actual_location.y) + z_basis * float(tile_height);
						center_position.y = -center_position.y;
						const simd::vector2 center_uv(pack_location.center_x/* + 0.5f*/, pack_location.center_y/* + 0.5f*/);

						float positive_x = pack_location.width_columns * MinimapTileWidth - pack_location.offset_x;
						float negative_x = -pack_location.offset_x;
						const float positive_y = pack_location.height_pixels - pack_location.offset_y;
						const float negative_y = -pack_location.offset_y;

						//If we are rendering a flipped version of the tile then we need to flip the uv coordinates
						const float uv_positive_x = (flip_tile ? negative_x : positive_x);
						const float uv_negative_x = (flip_tile ? positive_x : negative_x);
						if (flip_tile)
						{
							//We also need to flip the positioning but without turning the triangles around.
							std::swap(positive_x, negative_x);
							positive_x = -positive_x;
							negative_x = -negative_x;
						}

						//Add the four vertices wound clockwise------
						const simd::vector2 planar_center_pos = simd::vector2(float(actual_location.x), float(actual_location.y));

						const int start_vertex = int(vertices.size());
						vertices.push_back(MapTexturedVertex(
							planar_center_pos,
							float(tile_height),
							(center_uv + simd::vector2(uv_negative_x, negative_y)) / pack_image_size,
							simd::vector2(negative_x, negative_y) * MinimapProjectionRatio,
							float(subgraph_index)
						));

						vertices.push_back(MapTexturedVertex(
							planar_center_pos,
							float(tile_height),
							(center_uv + simd::vector2(uv_positive_x, negative_y)) / pack_image_size,
							simd::vector2(positive_x, negative_y) * MinimapProjectionRatio,
							float(subgraph_index)
						));

						vertices.push_back(MapTexturedVertex(
							planar_center_pos,
							float(tile_height),
							(center_uv + simd::vector2(uv_positive_x, positive_y)) / pack_image_size,
							simd::vector2(positive_x, positive_y) * MinimapProjectionRatio,
							float(subgraph_index)
						));

						vertices.push_back(MapTexturedVertex(
							planar_center_pos,
							float(tile_height),
							(center_uv + simd::vector2(uv_negative_x, positive_y)) / pack_image_size,
							simd::vector2(negative_x, positive_y) * MinimapProjectionRatio,
							float(subgraph_index)
						));

						indices.push_back(start_vertex + 0);
						indices.push_back(start_vertex + 1);
						indices.push_back(start_vertex + 2);

						indices.push_back(start_vertex + 2);
						indices.push_back(start_vertex + 3);
						indices.push_back(start_vertex + 0);
					}
				}

				++index;
			}
		}

		Device::MemoryDesc sysMemDesc;

		memset(&sysMemDesc, 0, sizeof(sysMemDesc));
		sysMemDesc.pSysMem = vertices.data();
		static_assert(sizeof(unsigned int) == 4, "Unsigned int assumed to be size 4");
		tilemap_vertices_count = int(vertices.size());
		if (vertices.size() > 0)
			tilemap_vertices = Device::VertexBuffer::Create("VB Minimap Tilemap Cheap", device, UINT(vertices.size()) * sizeof(MapTexturedVertex), Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, &sysMemDesc);
		else
			tilemap_vertices.Reset();


		memset(&sysMemDesc, 0, sizeof(sysMemDesc));
		sysMemDesc.pSysMem = indices.data();
		tilemap_indices_count = int(indices.size());
		if (indices.size() > 0)
			tilemap_indices = Device::IndexBuffer::Create("IB Minimap Tilemap Cheap", device, UINT(indices.size()) * sizeof(unsigned int), Device::UsageHint::DEFAULT, Device::IndexFormat::_32, Device::Pool::DEFAULT, &sysMemDesc);
		else
			tilemap_indices.Reset();
	}

	void Minimap::CreateWalkabilityMesh(Device::IDevice* device)
	{
		constexpr float stride = TileSize / 2.0f;
		const int width = int(minimap_size.x * TileSize / stride + 0.5f);
		const int height = int(minimap_size.y * TileSize / stride + 0.5f);

		struct MeshNode
		{
			float height = 0.0f;
			bool is_walkable = false;
			int index_in_mesh = 0;
			unsigned subgraph_index = 0;
		};
		std::vector<MeshNode> mesh_nodes;

		//Building all walkability mesh vertices. Unused ones will be discarded
		mesh_nodes.resize(width * height);
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				const Location loc = Location(int(x * stride + 0.5f), int(y * stride + 0.5f));
				mesh_nodes[x + y * width].height = data_source.GetTilemapHeight(loc);
				mesh_nodes[x + y * width].is_walkable = data_source.GetLocationWalkability(loc);
				mesh_nodes[x + y * width].index_in_mesh = -1;
				std::tie(std::ignore, std::ignore, std::ignore, std::ignore, std::ignore, mesh_nodes[x + y * width].subgraph_index) = data_source.GetMinimapTile(loc / TileSize);
			}
		}

		//Adjusting height of unwalkable mesh vertices to prevent mesh deformation around ledges
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				if (mesh_nodes[x + y * width].is_walkable) continue; //don't interpolate vertices that are already fine

				float sum_weight = 0.0f;
				float sum_height = 0.0f;
				for (int y_offset = -4; y_offset <= 4; y_offset++)
				{
					for (int x_offset = -4; x_offset <= 4; x_offset++)
					{
						const Location loc = Location(x + x_offset, y + y_offset);
						if (loc.x >= 0 && loc.y >= 0 && loc.x < width && loc.y < height)
						{
							const MeshNode neighbour = mesh_nodes[loc.x + loc.y * width];
							if (!neighbour.is_walkable)
								continue;
							sum_weight += 1.0f;
							sum_height += neighbour.height;
						}
					}
				}
				if (sum_weight > 1e-3f)
				{
					mesh_nodes[x + y * width].height = sum_height / sum_weight;
				}
			}
		}

		std::vector<MapBaseVertex > vertices;
		std::vector<unsigned int > indices;
		for( int diag = width - 1 + height - 1 - 1; diag >= 0 ; --diag )
		{
			int x = std::min( diag, width - 1 - 1 );
			int y = std::max( 0, diag - width + 1 + 1 );
			for( ; y < height - 1 && x >= 0; --x, ++y )
			{
				Location base_locations[4];
				base_locations[0] = Location(x, y);
				base_locations[1] = Location(x + 1, y);
				base_locations[2] = Location(x + 1, (y + 1));
				base_locations[3] = Location(x, (y + 1));

				int base_indices[ 4 ] = { 0 };
				for (int quad_vertex = 0; quad_vertex < 4; quad_vertex++)
				{
					base_indices[quad_vertex] = base_locations[quad_vertex].x + base_locations[quad_vertex].y * width;
				}

				//add those quad vertices, that are not added to the mesh yet
				for (int quad_vertex = 0; quad_vertex < 4; quad_vertex++)
				{
					const int base_index = base_indices[quad_vertex];
					if (mesh_nodes[base_index].index_in_mesh == -1)
					{
						mesh_nodes[base_index].index_in_mesh = int(vertices.size());
						vertices.push_back(MapBaseVertex(
							simd::vector2(float(base_locations[quad_vertex].x * stride) / TileSize, float(base_locations[quad_vertex].y * stride) / TileSize),
							mesh_nodes[base_index].height,
							float(mesh_nodes[base_index].subgraph_index)
						));
					}
				}

				indices.push_back(mesh_nodes[base_indices[0]].index_in_mesh);
				indices.push_back(mesh_nodes[base_indices[1]].index_in_mesh);
				indices.push_back(mesh_nodes[base_indices[2]].index_in_mesh);

				indices.push_back(mesh_nodes[base_indices[2]].index_in_mesh);
				indices.push_back(mesh_nodes[base_indices[3]].index_in_mesh);
				indices.push_back(mesh_nodes[base_indices[0]].index_in_mesh);
			}
		}


		Device::MemoryDesc sysMemDesc;

		walkability_indices_count = int(indices.size());
		memset(&sysMemDesc, 0, sizeof(sysMemDesc));
		sysMemDesc.pSysMem = indices.data();
		if (indices.size() > 0)
			walkability_indices = Device::IndexBuffer::Create("IB Minimap Walkability", device, UINT(indices.size()) * sizeof(unsigned int), Device::UsageHint::DEFAULT, Device::IndexFormat::_32, Device::Pool::DEFAULT, &sysMemDesc);
		else
			walkability_indices.Reset();


		walkability_vertices_count = int(vertices.size());
		memset(&sysMemDesc, 0, sizeof(sysMemDesc));
		sysMemDesc.pSysMem = vertices.data();
		if (vertices.size() > 0)
			walkability_vertices = Device::VertexBuffer::Create("VB Minimap Walkability", device, UINT(vertices.size()) * sizeof(MapBaseVertex), Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, &sysMemDesc);
		else
			walkability_vertices.Reset();
	}



	template< typename T >
	void FillIndexBuffer(T* index, const unsigned num_quads)
	{
		for (unsigned i = 0; i < num_quads; ++i)
		{
			*index++ = i * 4;
			*index++ = i * 4 + 1;
			*index++ = i * 4 + 2;


			*index++ = i * 4;
			*index++ = i * 4 + 2;
			*index++ = i * 4 + 3;
		}
	}

	void Minimap::CreatePOIVertices( Device::IDevice* device )
	{
		const auto point_position = simd::vector2(0.0f, 0.0f);
		constexpr unsigned icons_per_row = 14;
		constexpr float icon_text_size_x = 1.0f / icons_per_row;
		const float icon_text_size_y = icon_text_size_x / icon_tex_size_ratio;
		const auto tsize = simd::vector2(52.f, 52.f);
		
		constexpr size_t                vertex_count = Loaders::MinimapIconsValues::NumRows * (size_t)4;
		std::vector<MapTexturedVertex > vertices( vertex_count );
		MapTexturedVertex*              vertex = vertices.data();
		
		for( unsigned i = 0; i < Loaders::MinimapIconsValues::NumRows; ++i )
		{
			const auto tex_offset_x = float(i % icons_per_row) * icon_text_size_x;
			const auto tex_offset_y = float(i / icons_per_row) * icon_text_size_y;
			if( tex_offset_x > 1.0f || tex_offset_y > 1.0f )
				continue;

			const auto subgraph_index = -1.f;
			*vertex++ = MapTexturedVertex(simd::vector2(0.0f, 0.0f), 0.0f, simd::vector2(tex_offset_x, tex_offset_y + icon_text_size_y), point_position + simd::vector2(-tsize.x, -tsize.y), subgraph_index);
			*vertex++ = MapTexturedVertex(simd::vector2(0.0f, 0.0f), 0.0f, simd::vector2(tex_offset_x, tex_offset_y), point_position + simd::vector2(-tsize.x, tsize.y), subgraph_index);
			*vertex++ = MapTexturedVertex(simd::vector2(0.0f, 0.0f), 0.0f, simd::vector2(tex_offset_x + icon_text_size_x, tex_offset_y), point_position + simd::vector2(tsize.x, tsize.y), subgraph_index);
			*vertex++ = MapTexturedVertex(simd::vector2(0.0f, 0.0f), 0.0f, simd::vector2(tex_offset_x + icon_text_size_x, tex_offset_y + icon_text_size_y), point_position + simd::vector2(tsize.x, -tsize.y), subgraph_index);
		}

		Device::MemoryDesc sysMemDesc;
		memset( &sysMemDesc, 0, sizeof(sysMemDesc) );
		sysMemDesc.pSysMem = vertices.data();

		vb_points_of_interest = Device::VertexBuffer::Create("VB Minimap POI", device, UINT(vertex_count) * sizeof( MapTexturedVertex ), Device::UsageHint::DEFAULT, Device::Pool::DEFAULT, &sysMemDesc );
	}

	void Minimap::CreatePOIIndices( Device::IDevice* device )
	{
		constexpr unsigned num_quads   = 1;
		constexpr unsigned num_indices = num_quads * 6;

		//Check if we need to use 32 bit indices.
		if constexpr( num_quads * 4 > 0xff )
		{
			static_assert( sizeof(unsigned int) == 4, "Unsigned int assumed to be size 4" );

			std::vector<unsigned int > indices( num_quads * 6 );
			FillIndexBuffer<unsigned int >( indices.data(), num_quads );

			Device::MemoryDesc sysMemDesc;
			memset( &sysMemDesc, 0, sizeof(Device::MemoryDesc) );
			sysMemDesc.pSysMem = indices.data();

			ib_points_of_interest = Device::IndexBuffer::Create("IB Minimap POI", device, num_indices*sizeof(unsigned int), Device::UsageHint::DEFAULT, Device::IndexFormat::_32, Device::Pool::DEFAULT, &sysMemDesc );
		}
		else
		{
			static_assert( sizeof(unsigned short) == 2, "Unsigned short assumed to be size 2" );

			std::vector<unsigned short > indices( num_quads * 6 );
			FillIndexBuffer<unsigned short >( indices.data(), num_quads );

			Device::MemoryDesc sysMemDesc;
			memset( &sysMemDesc, 0, sizeof(Device::MemoryDesc) );
			sysMemDesc.pSysMem = indices.data();

			ib_points_of_interest = Device::IndexBuffer::Create("IB Minimap POI", device, num_indices*sizeof(unsigned short), Device::UsageHint::DEFAULT, Device::IndexFormat::_16, Device::Pool::DEFAULT, &sysMemDesc );
		}
	}	

	void Minimap::ReloadShaders(Device::IDevice* device)
	{
		//load the shader string into memory
		base_vertex_shader.vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Base_Vertex", Renderer::LoadShaderSource(L"Shaders/Minimap_Tile_Vertex.hlsl"), nullptr, "BaseVertexTransform", Device::VERTEX_SHADER);
		poi_vertex_shader.vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Poi_Vertex", Renderer::LoadShaderSource(L"Shaders/Minimap_Tile_Vertex.hlsl"), nullptr, "PointOfInterestVertexTransform", Device::VERTEX_SHADER);
		textured_vertex_shader.vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Textured_Vertex", Renderer::LoadShaderSource(L"Shaders/Minimap_Tile_Vertex.hlsl"), nullptr, "TexturedVertexTransform", Device::VERTEX_SHADER);
		tile_pixel_shader.pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Pixel", Renderer::LoadShaderSource(L"Shaders/Minimap_Pixel.hlsl"), nullptr, "RenderTiles", Device::PIXEL_SHADER);
		walkability_shader.pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Walkability_Pixel", Renderer::LoadShaderSource(L"Shaders/Minimap_Pixel.hlsl"), nullptr, "RenderWalkability", Device::PIXEL_SHADER);
		imagespace_shader.vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "Imagespace_Vertex", Renderer::LoadShaderSource(L"Shaders/FullScreenQuad_VertexShader.hlsl"), nullptr, "VShad", Device::VERTEX_SHADER);
		visibility_shader.pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Visibility_Pixel", Renderer::LoadShaderSource(L"Shaders/Minimap_Visibility_Pixel.hlsl"), nullptr, "RenderVisibility", Device::PIXEL_SHADER);
		blending_shader.pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Minimap_Blending_Pixel", Renderer::LoadShaderSource(L"Shaders/Minimap_Blending_Pixel.hlsl"), nullptr, "BlendMinimap", Device::PIXEL_SHADER);

		base_vertex_shader.vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap_Base", device, base_vertex_shader.vertex_shader.Get());
		poi_vertex_shader.vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap_Poi", device, poi_vertex_shader.vertex_shader.Get());
		textured_vertex_shader.vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap_Textured", device, textured_vertex_shader.vertex_shader.Get());
		tile_pixel_shader.pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap", device, tile_pixel_shader.pixel_shader.Get());
		walkability_shader.pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap_Walkability", device, walkability_shader.pixel_shader.Get());
		imagespace_shader.vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("Imagespace", device, imagespace_shader.vertex_shader.Get());
		visibility_shader.pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap_Visibility", device, visibility_shader.pixel_shader.Get());
		blending_shader.pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("Minimap_Blending", device, blending_shader.pixel_shader.Get());
	}

	inline simd::vector2 NegY( const simd::vector2& v )
	{
		return simd::vector2( v.x, -v.y );
	}

	void Minimap::RenderToTexture(
		const simd::vector3& center_position,
		const simd::vector2& screen_offset,
		const float map_scale,
		const Device::Rect& rect,
		RectType rect_type,
		const unsigned subgraph_index,
		const DecayInfo& decay_info,
		const RoyaleInfo& royale_info )
	{
		auto& command_buffer = *device->GetCurrentUICommandBuffer();

		constexpr float y_scale = 0.0003;
		const float aspect_ratio = float(screen_height) / screen_width;

		const simd::vector2 offset = (screen_offset * 2.0f) / simd::vector2(float(screen_width), -float(screen_height));
		const auto screen_offset_matrix = simd::matrix::translation(simd::vector3(offset.x, offset.y, 0.0f));

		const simd::vector2 minimap_center_position =
			x_basis * (center_position.x / WorldSpaceTileSize) +
			y_basis * (center_position.y / WorldSpaceTileSize) +
			z_basis * (center_position.z / Terrain::UnitToWorldHeightScale);

		const float res_x_scale = y_scale * aspect_ratio * map_scale;
		const float res_y_scale = y_scale * map_scale;
		const auto trans = simd::matrix::translation(simd::vector3(-minimap_center_position.x, minimap_center_position.y, 0.0f));
		const auto scale = simd::matrix::scale(simd::vector3(res_x_scale, res_y_scale, 0.0f));
		const auto res_matrix = trans * scale * simd::matrix::scale(simd::vector3(1.0f, -1.0f, 1.0f)) * screen_offset_matrix;

		Vector2d crop_center(0.0f, 0.0f);
		float crop_radius = 1e7f;
		float crop_border = 0.0f;

		crop_center.x = (rect.left + rect.right) * 0.5f;	
		crop_center.y = (rect.top + rect.bottom) * 0.5f;

		if (rect_type == RectType::Circle)
		{
			crop_radius = std::min(std::abs(rect.right - rect.left), std::abs(rect.top - rect.bottom)) * 0.5f;
			crop_border = crop_radius * 0.2f;
		}
		else
			if (rect_type == RectType::Quad)
			{
				crop_border = std::min(std::abs(rect.right - rect.left), std::abs(rect.top - rect.bottom)) * 0.5f * 0.15f;
			}
		const simd::vector4 clipping_circle(crop_center.x, crop_center.y, crop_radius, 0.0f);

		//Rendering walkability map
		{
			command_buffer.BeginPass(projected_walkability_pass.Get(), projected_walkability_target->GetSize(), projected_walkability_target->GetSize());

			base_vertex_shader.vertex_uniform_buffer->SetMatrix("transform", &res_matrix);

			auto vec = simd::vector4(x_basis.x, x_basis.y, 0.0f, 0.0f);
			base_vertex_shader.vertex_uniform_buffer->SetVector("x_basis", &vec);
			walkability_shader.pixel_uniform_buffer->SetVector("x_basis", &vec);
			
			vec = simd::vector4(y_basis.x, y_basis.y, 0.0f, 0.0f);
			base_vertex_shader.vertex_uniform_buffer->SetVector("y_basis", &vec);
			walkability_shader.pixel_uniform_buffer->SetVector("y_basis", &vec);
			
			vec = simd::vector4(z_basis.x, z_basis.y, 0.0f, 0.0f);
			base_vertex_shader.vertex_uniform_buffer->SetVector("z_basis", &vec);
			walkability_shader.pixel_uniform_buffer->SetVector("z_basis", &vec);

			vec = simd::vector4(float(minimap_size.x), float(minimap_size.y), 0.0f, 0.0f);
			base_vertex_shader.vertex_uniform_buffer->SetVector("tiles_count", &vec);
			walkability_shader.pixel_uniform_buffer->SetVector("tiles_count", &vec);

			walkability_shader.pixel_uniform_buffer->SetVector("render_circle", &clipping_circle);

			walkability_shader.pixel_uniform_buffer->SetFloat("subgraph_index", float(subgraph_index));

			walkability_shader.pixel_uniform_buffer->SetFloat("tile_world_size", float(Terrain::WorldSpaceTileSize));

			walkability_shader.pixel_uniform_buffer->SetVector( "royale_damage_circle", &royale_info.damage_circle );
			walkability_shader.pixel_uniform_buffer->SetVector( "royale_safe_circle", &royale_info.safe_circle );

			simd::vector4 decay_minmax;
			decay_minmax.x = decay_info.min_point.x;
			decay_minmax.y = decay_info.min_point.y;
			decay_minmax.z = decay_info.max_point.x;
			decay_minmax.w = decay_info.max_point.y;
			walkability_shader.pixel_uniform_buffer->SetVector("decay_map_minmax", &decay_minmax);
			
			simd::vector4 decay_map_size;
			decay_map_size.x = (float)decay_info.map_size.x;
			decay_map_size.y = (float)decay_info.map_size.y;
			decay_map_size.z = 0.0f;
			decay_map_size.w = 0.0f;
			walkability_shader.pixel_uniform_buffer->SetVector("decay_map_size", &decay_map_size);
		
			walkability_shader.pixel_uniform_buffer->SetFloat("decay_map_time", decay_info.decay_time);
			walkability_shader.pixel_uniform_buffer->SetFloat("creation_time", decay_info.creation_time);
			walkability_shader.pixel_uniform_buffer->SetVector("stabiliser_position", &decay_info.stabiliser_position);
			walkability_shader.pixel_uniform_buffer->SetFloat("global_stability", decay_info.global_stability);

			if (walkability_indices && walkability_vertices && walkability_texture)
			{
				if (command_buffer.BindPipeline(walkability_shader.pipeline.Get()))
				{
					Device::DynamicBindingSet::Inputs inputs;
					inputs.push_back({ "walkability_sampler", walkability_texture.Get() });
					inputs.push_back({ "visibility_sampler", visibility_render_target->GetTexture().Get() });
					inputs.push_back({ "decay_map_sampler", decay_info.decay_tex.Get() });
					inputs.push_back({ "stable_map_sampler", decay_info.stable_tex.Get() });
					inputs.push_back({ "crack_sampler", crack_sdm_texture.GetTexture().Get() });
					auto* pixel_binding_set = FindBindingSet("Minimap Walkability", device, walkability_shader.pixel_shader.Get(), inputs);

					auto* descriptor_set = FindDescriptorSet("Minimap Walkability", walkability_shader.pipeline.Get(), pixel_binding_set);
					command_buffer.BindDescriptorSet(descriptor_set, {}, { base_vertex_shader.vertex_uniform_buffer.Get(), walkability_shader.pixel_uniform_buffer.Get() });

					command_buffer.BindBuffers(walkability_indices.Get(), walkability_vertices.Get(), 0, sizeof(MapBaseVertex));
					command_buffer.DrawIndexed(walkability_indices_count, 1, 0, 0, 0);
				}
			}

			command_buffer.EndPass();
		}

		//Rendering tile geometry
		{
			command_buffer.BeginPass(projected_geometry_pass.Get(), projected_geometry_target->GetSize(), projected_geometry_target->GetSize());

			textured_vertex_shader.vertex_uniform_buffer->SetMatrix("transform", &res_matrix);

			auto vec = simd::vector4(x_basis.x, x_basis.y, 0.0f, 0.0f);
			textured_vertex_shader.vertex_uniform_buffer->SetVector("x_basis", &vec);

			vec = simd::vector4(y_basis.x, y_basis.y, 0.0f, 0.0f);
			textured_vertex_shader.vertex_uniform_buffer->SetVector("y_basis", &vec);

			vec = simd::vector4(z_basis.x, z_basis.y, 0.0f, 0.0f);
			textured_vertex_shader.vertex_uniform_buffer->SetVector("z_basis", &vec);

			vec = simd::vector4(float(minimap_size.x), float(minimap_size.y), 0.0f, 0.0f);
			textured_vertex_shader.vertex_uniform_buffer->SetVector("tiles_count", &vec);

			tile_pixel_shader.pixel_uniform_buffer->SetVector("render_circle", &clipping_circle);
			tile_pixel_shader.pixel_uniform_buffer->SetFloat("alpha_override", 1.0f);

			tile_pixel_shader.pixel_uniform_buffer->SetFloat("subgraph_index", float(subgraph_index));

			if (tilemap_vertices && tilemap_indices)
			{
				if (command_buffer.BindPipeline(tile_pixel_shader.pipeline_textured.Get()))
				{
					Device::DynamicBindingSet::Inputs inputs;
					inputs.push_back({ "tilemap_sampler", minimap_tile_pack->GetTexture().Get() });
					auto* pixel_binding_set = FindBindingSet("Minimap Geometry", device, tile_pixel_shader.pixel_shader.Get(), inputs);

					auto* descriptor_set = FindDescriptorSet("Minimap Geometry", tile_pixel_shader.pipeline_textured.Get(), pixel_binding_set);
					command_buffer.BindDescriptorSet(descriptor_set, {}, { textured_vertex_shader.vertex_uniform_buffer.Get(), tile_pixel_shader.pixel_uniform_buffer.Get() });

					command_buffer.BindBuffers(tilemap_indices.Get(), tilemap_vertices.Get(), 0, sizeof(MapTexturedVertex));
					command_buffer.DrawIndexed(tilemap_indices_count, 1, 0, 0, 0);
				}
			}

			command_buffer.EndPass();
		}

	}

	void Minimap::Render(
		const simd::vector3& center_position,
		const simd::vector2& screen_offset,
		const float map_scale,
		const Device::Rect &rect,
		RectType rect_type,
		const unsigned subgraph_index,
		Memory::Vector<Terrain::Minimap::InterestPoint, Memory::Tag::UI>::iterator interest_points_begin,
		Memory::Vector<Terrain::Minimap::InterestPoint, Memory::Tag::UI>::iterator interest_points_end,
		const bool should_draw_arrows,
		const bool clip_to_rect,
		const DecayInfo& decay_info,
		const RoyaleInfo& royale_info )
	{
		const float aspect_ratio = float( screen_height ) / screen_width ;

		const simd::vector2 offset = (screen_offset * 2.0f) / simd::vector2( float( screen_width ), -float( screen_height ) );
		const auto screen_offset_matrix = simd::matrix::translation(simd::vector3( offset.x, offset.y, 0.0f ));

		const simd::vector2 minimap_center_position = 
			x_basis * (center_position.x / WorldSpaceTileSize) + 
			y_basis * (center_position.y / WorldSpaceTileSize) + 
			z_basis * (center_position.z / Terrain::UnitToWorldHeightScale);

		constexpr float y_scale = 0.0003;
		const float res_x_scale = y_scale * aspect_ratio * map_scale;
		const float res_y_scale = y_scale * map_scale;
		const auto trans = simd::matrix::translation(simd::vector3( -minimap_center_position.x, minimap_center_position.y, 0.0f ));
		const auto scale = simd::matrix::scale(simd::vector3(res_x_scale, res_y_scale, 0.0f));
		const auto res_matrix = trans * scale * simd::matrix::scale(simd::vector3(1.0f, -1.0f, 1.0f)) * screen_offset_matrix;

		Vector2d crop_center(0.0f, 0.0f);
		float crop_radius = 1e7f;
		float crop_border = 0.0f;

		crop_center.x = (rect.left + rect.right) * 0.5f;
		crop_center.y = (rect.top + rect.bottom) * 0.5f;

		if (rect_type == RectType::Circle)
		{
			crop_radius = std::min(std::abs(rect.right - rect.left), std::abs(rect.top - rect.bottom)) * 0.5f;
			crop_border = crop_radius * 0.2f;
		}
		else
			if (rect_type == RectType::Quad)
			{
				crop_border = std::min(std::abs(rect.right - rect.left), std::abs(rect.top - rect.bottom)) * 0.5f * 0.15f;
			}
		const simd::vector4 clipping_circle(crop_center.x, crop_center.y, crop_radius, 0.0f);

		auto& command_buffer = *device->GetCurrentUICommandBuffer();
		auto* pass = device->GetCurrentUIPass();

		//Blending walkability and geometry to main screen
		{
			if( clip_to_rect )
				command_buffer.SetScissor(rect);

			const auto viewport_size = simd::vector4((float)projected_geometry_target->GetWidth(), (float)projected_geometry_target->GetHeight(), 0.0f, 0.0f);
			blending_shader.pixel_uniform_buffer->SetVector("viewport_size", &viewport_size);
			blending_shader.pixel_uniform_buffer->SetFloat("geometry_opacity", geometry_alpha);
			blending_shader.pixel_uniform_buffer->SetFloat("walkability_opacity", walkability_alpha);
			blending_shader.pixel_uniform_buffer->SetBool("use_royale_data", royale_info.damage_circle.z > 0.0f || royale_info.safe_circle.z > 0.0f);

			auto* pipeline = FindPipeline("Minimap Blending", pass, Device::PrimitiveType::TRIANGLESTRIP, &imagespace_vertex_declaration, imagespace_shader.vertex_shader.Get(), blending_shader.pixel_shader.Get(),
					Device::BlendState(
						Device::BlendChannelState(Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
						Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)),
					Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0),
					Device::UIDepthStencilState().SetDepthState(Device::DisableDepthState()));
			if (command_buffer.BindPipeline(pipeline))
			{
				Device::DynamicBindingSet::Inputs inputs;
				inputs.push_back({ "walkability_sampler", projected_walkability_target->GetTexture().Get() });
				inputs.push_back({ "geometry_sampler", projected_geometry_target->GetTexture().Get() });
				auto* pixel_binding_set = FindBindingSet("Minimap Blending", device, blending_shader.pixel_shader.Get(), inputs);

				auto* descriptor_set = FindDescriptorSet("Minimap Blending", pipeline, pixel_binding_set);
				command_buffer.BindDescriptorSet(descriptor_set, {}, { imagespace_shader.vertex_uniform_buffer.Get(), blending_shader.pixel_uniform_buffer.Get() });

				command_buffer.BindBuffers(nullptr, imagespace_vertices.Get(), 0, sizeof(ImagespaceVertex));
				command_buffer.Draw(4, 0);
			}
		}

		//Rendering of points of interest
		{
			const Loaders::MinimapIconsTable icons;

			for (auto interest_point = interest_points_begin; interest_point != interest_points_end; ++interest_point)
			{
				simd::vector2 map_point_position =
					x_basis * (std::get< 0 >(*interest_point).x / (float)WorldSpaceTileSize) +
					y_basis * (std::get< 0 >(*interest_point).y / (float)WorldSpaceTileSize) +
					z_basis * (std::get< 0 >(*interest_point).z / UnitToWorldHeightScale);

				const auto icon_row = icons.GetDataRowByIndex( std::get< 1 >( *interest_point ) );
				const auto poi_type = icons.GetDataRowEnum( icon_row );

				const auto res_matrix = scale * screen_offset_matrix;

				auto screen_point_position = res_matrix.mulpos(simd::vector2(
					map_point_position[0] - minimap_center_position.x, 
					map_point_position[1] - minimap_center_position.y));

				const Vector2d pixel_point_position((screen_point_position[0] * 0.5f + 0.5f) * screen_width, (0.5f - screen_point_position[1] * 0.5f) * screen_height);
				Vector2d fixed_pixel_point_position = pixel_point_position;

				bool draw_arrow = false;

				Vector2d pixel_delta = pixel_point_position - crop_center;
				const float dist = pixel_delta.length();
				const float poi_radius = crop_radius - crop_border;

				// Points of Interest should fade out after reaching 60% of their visible radius
				float poi_visible_radius = poi_radius;
				if( rect_type == RectType::Quad )
					poi_visible_radius = std::abs( (rect.right - rect.left) * 0.5f );

				// Increase the visible radius to be 2 and a half (minimap) screens away
				poi_visible_radius *= icon_row->GetVisibleRadius() / 100.0f;

				const float poi_fade_radius = poi_visible_radius * 0.8f;

				// Do not render points of interest outside of their visible radius
				const bool poi_is_always_visible = !std::get< 3 >(*interest_point);
				if( !poi_is_always_visible && dist > poi_visible_radius )
					continue;

				// Do not render if they are not located in the currently active subgraph
				if (!poi_is_always_visible)
				{
					unsigned poi_subgraph_index = -1;
					Location loc(Round(std::get< 0 >(*interest_point).x / (float)WorldSpaceTileSize), Round(std::get< 0 >(*interest_point).y / (float)WorldSpaceTileSize));
					std::tie(std::ignore, std::ignore, std::ignore, std::ignore, std::ignore, poi_subgraph_index) = data_source.GetMinimapTile(loc);
					if (poi_subgraph_index != subgraph_index)
						continue;
				}

				float poi_alpha = 1.0f;
				if( !poi_is_always_visible && dist > poi_fade_radius )
				{
					const float distance_from_fade_radius = dist - poi_fade_radius;
					poi_alpha = 1.0f - (distance_from_fade_radius / (poi_visible_radius - poi_fade_radius));
				}

				tile_pixel_shader.pixel_uniform_buffer->SetFloat("alpha_override", poi_alpha);

				if (rect_type == RectType::Circle)
				{
					if (dist > poi_radius)
					{
						pixel_delta = pixel_delta / pixel_delta.length() * poi_radius;
						draw_arrow = true;
					}

					fixed_pixel_point_position = pixel_delta + crop_center;
				}
				else
				if (rect_type == RectType::Quad)
				{
					if (fixed_pixel_point_position.x < rect.left + crop_border)
					{
						fixed_pixel_point_position.x = rect.left + crop_border;
						draw_arrow = true;
					}
					if (fixed_pixel_point_position.x > rect.right - crop_border)
					{
						fixed_pixel_point_position.x = rect.right - crop_border;
						draw_arrow = true;
					}
					if (fixed_pixel_point_position.y < rect.top + crop_border)
					{
						fixed_pixel_point_position.y = rect.top + crop_border;
						draw_arrow = true;
					}
					if (fixed_pixel_point_position.y > rect.bottom - crop_border)
					{
						fixed_pixel_point_position.y = rect.bottom - crop_border;
						draw_arrow = true;
					}
				}
				const simd::vector2 fixed_screen_point_position(
					fixed_pixel_point_position.x / screen_width * 2.0f - 1.0f,
					1.0f - fixed_pixel_point_position.y / screen_height * 2.0f);


				const float icon_scale_mult = std::get< 2 >(*interest_point);

				poi_vertex_shader.vertex_uniform_buffer->SetMatrix("transform", &res_matrix);

				const simd::vector4 map_center_pos(minimap_center_position.x, minimap_center_position.y, 0.0f, 0.0f);
				poi_vertex_shader.vertex_uniform_buffer->SetVector("map_center_pos", &(map_center_pos));

				const simd::vector4 poi_center_pos(fixed_screen_point_position.x, fixed_screen_point_position.y, 0.0f, 0.0f);
				poi_vertex_shader.vertex_uniform_buffer->SetVector("poi_center_pos", &(poi_center_pos));

				tile_pixel_shader.pixel_uniform_buffer->SetVector("render_circle", &clipping_circle);
				tile_pixel_shader.pixel_uniform_buffer->SetVector( "royale_damage_circle", &royale_info.damage_circle );
				tile_pixel_shader.pixel_uniform_buffer->SetVector( "royale_safe_circle", &royale_info.safe_circle );

				tile_pixel_shader.pixel_uniform_buffer->SetFloat("subgraph_index", float(subgraph_index));

				const simd::vector4 viewport_size((float)screen_width, (float)screen_height, 0.0f, 0.0f);
				poi_vertex_shader.vertex_uniform_buffer->SetVector("viewport_size", &viewport_size);

				auto* pipeline = FindPipeline("Minimap Poi", pass, Device::PrimitiveType::TRIANGLELIST, &textured_vertex_declaration, poi_vertex_shader.vertex_shader.Get(), tile_pixel_shader.pixel_shader.Get(),
					Device::BlendState(
						Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
						Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)),
					Device::RasterizerState(Device::CullMode::NONE, Device::FillMode::SOLID, 0, 0),
					Device::UIDepthStencilState().SetDepthState(Device::DisableDepthState()));
				if (command_buffer.BindPipeline(pipeline))
				{

					if (draw_arrow && !icon_row->GetHideEdgeArrows() && (should_draw_arrows || poi_type == Loaders::MinimapIconsValues::Entrance) )
					{
						constexpr float scale = 3.0f;

						const Vector2d delta = fixed_pixel_point_position - crop_center;
						const float arrow_angle = std::atan2(-delta.y, delta.x) - PI / 2.0f;
						const auto rotation_matrix = simd::matrix::rotationZ(arrow_angle);

						poi_vertex_shader.vertex_uniform_buffer->SetFloat("icon_scale", scale);

						const auto arrow_matrix = rotation_matrix * res_matrix;
						poi_vertex_shader.vertex_uniform_buffer->SetMatrix("transform", &arrow_matrix);

					#ifdef CONSOLE_UI
						const float offset_between_arrows = ( scale * (screen_height - 1) / 128.0f ) * 2.0f * map_scale;
						const float arrow_height_increase = 0.75f * scale * (screen_height - 1) / 128.0f;
						const auto unscaled_rotation_offset = rotation_matrix * simd::vector4( offset_between_arrows, 0.0f, 0.0f, 0.0f );
						const auto unscaled_height_offset = rotation_matrix * simd::vector4( 0.0f, arrow_height_increase, 0.0f, 0.0f );
						const simd::vector4 poi_rotated_offset( unscaled_rotation_offset.x / screen_width, unscaled_rotation_offset.y / screen_height, 0.0f, 0.0f );
						const simd::vector4 poi_height_offset( unscaled_height_offset.x / screen_width, unscaled_height_offset.y / screen_height, 0.0f, 0.0f );

						{	// Left arrow
							const auto poi_center_pos = simd::vector4( fixed_screen_point_position.x, fixed_screen_point_position.y, 0.0f, 0.0f ) - poi_rotated_offset + poi_height_offset;
							poi_vertex_shader.vertex_uniform_buffer->SetVector("poi_center_pos", &(poi_center_pos));

							Device::DynamicBindingSet::Inputs inputs;
							inputs.push_back( { "tilemap_sampler", player_cross_texture.Get() } );
							auto* pixel_binding_set = FindBindingSet( "Minimap POI", device, tile_pixel_shader.pixel_shader.Get(), inputs );

							auto* descriptor_set = FindDescriptorSet( "Minimap POI", pipeline, pixel_binding_set );
							command_buffer.BindDescriptorSet( descriptor_set, {}, { poi_vertex_shader.vertex_uniform_buffer.Get(), tile_pixel_shader.pixel_uniform_buffer.Get() });

							command_buffer.BindBuffers( ib_points_of_interest.Get(), vb_points_of_interest.Get(), 0, sizeof( MapTexturedVertex ) );

							const unsigned int arrow_left_index = (Loaders::MinimapIconsValues::ConsoleMapArrowL - 1) * 4;
							command_buffer.DrawIndexed(6, 1, 0, arrow_left_index, 0);
						}

						{	// Right arrow
							const auto poi_center_pos = simd::vector4( fixed_screen_point_position.x, fixed_screen_point_position.y, 0.0f, 0.0f ) + poi_rotated_offset + poi_height_offset;
							poi_vertex_shader.vertex_uniform_buffer->SetVector("poi_center_pos", &(poi_center_pos));

							Device::DynamicBindingSet::Inputs inputs;
							inputs.push_back( { "tilemap_sampler", player_cross_texture.Get() } );
							auto* pixel_binding_set = FindBindingSet( "Minimap POI", device, tile_pixel_shader.pixel_shader.Get(), inputs );

							auto* descriptor_set = FindDescriptorSet( "Minimap POI", pipeline, pixel_binding_set );
							command_buffer.BindDescriptorSet( descriptor_set, {}, { poi_vertex_shader.vertex_uniform_buffer.Get(), tile_pixel_shader.pixel_uniform_buffer.Get() });

							command_buffer.BindBuffers( ib_points_of_interest.Get(), vb_points_of_interest.Get(), 0, sizeof( MapTexturedVertex ) );

							const unsigned int arrow_right_index = (Loaders::MinimapIconsValues::ConsoleMapArrowR - 1) * 4;
							command_buffer.DrawIndexed(6, 1, 0, arrow_right_index, 0);
						}
					#else
						const float arrow_height_increase = 0.85f * scale * (screen_height - 1) / 128.0f;
						const auto unscaled_height_offset = rotation_matrix * simd::vector4( 0.0f, arrow_height_increase, 0.0f, 0.0f );
						const simd::vector4 poi_height_offset( unscaled_height_offset.x / screen_width, unscaled_height_offset.y / screen_height, 0.0f, 0.0f );

						// Need to move the arrow away from the icon
						const auto arrow_offset_pos = simd::vector4( fixed_screen_point_position.x, fixed_screen_point_position.y, 0.0f, 0.0f ) + poi_height_offset;
						poi_vertex_shader.vertex_uniform_buffer->SetVector("poi_center_pos", &(arrow_offset_pos));

						Device::DynamicBindingSet::Inputs inputs;
						inputs.push_back({ "tilemap_sampler", player_cross_texture.Get() });
						auto* pixel_binding_set = FindBindingSet("Minimap POI", device, tile_pixel_shader.pixel_shader.Get(), inputs);

						auto* descriptor_set = FindDescriptorSet("Minimap POI", pipeline, pixel_binding_set);
						command_buffer.BindDescriptorSet(descriptor_set, {}, { poi_vertex_shader.vertex_uniform_buffer.Get(), tile_pixel_shader.pixel_uniform_buffer.Get() });

						command_buffer.BindBuffers(ib_points_of_interest.Get(), vb_points_of_interest.Get(), 0, sizeof(MapTexturedVertex));

						constexpr unsigned start_index = (Loaders::MinimapIconsValues::PCMapArrow - 1) * 4;
						command_buffer.DrawIndexed(6, 1, 0, start_index, 0);
					#endif
					}

					{
						poi_vertex_shader.vertex_uniform_buffer->SetMatrix("transform", &res_matrix);
				
						poi_vertex_shader.vertex_uniform_buffer->SetFloat("icon_scale", icon_scale_mult);

						// Reset poi_center_pos_handle back to the previous center_pos
						poi_vertex_shader.vertex_uniform_buffer->SetVector("poi_center_pos", &(poi_center_pos));

						Device::DynamicBindingSet::Inputs inputs;
						inputs.push_back({ "tilemap_sampler", player_cross_texture.Get() });
						auto* pixel_binding_set = FindBindingSet("Minimap POI", device, tile_pixel_shader.pixel_shader.Get(), inputs);

						auto* descriptor_set = FindDescriptorSet("Minimap POI", pipeline, pixel_binding_set);
						command_buffer.BindDescriptorSet(descriptor_set, {}, { poi_vertex_shader.vertex_uniform_buffer.Get(), tile_pixel_shader.pixel_uniform_buffer.Get() });

						command_buffer.BindBuffers(ib_points_of_interest.Get(), vb_points_of_interest.Get(), 0, sizeof(MapTexturedVertex));

						// render the quad that matches the required icon
						const unsigned start_index = std::get< 1 >(*interest_point) * 4;
						command_buffer.DrawIndexed(6, 1, 0, start_index, 0);
					}
				}
			}
		}
	}

	void Minimap::UploadTileVisibility()
	{
		if (!device)
			return;

		if (!last_explored && !tile_visibility_fully_revealed && !tile_visibility_walkable_revealed && !tile_visibility_reset && !tile_bound_revealed)
			return; // There's nothing to update

		auto& command_buffer = *device->GetCurrentUICommandBuffer();

		Renderer::System::Get().GetResampler().ResampleColor(command_buffer, visibility_render_target.get(), visibility_render_target_copy.get());

		command_buffer.BeginPass(visibility_pass.Get(), visibility_render_target->GetSize(), visibility_render_target->GetSize());

		const auto last_tile = last_explored.value_or(Vector2d());
		const simd::vector4 explored_tile(last_tile.x, last_tile.y, 0.0f, 0.0f);
		visibility_shader.pixel_uniform_buffer->SetVector("explored_tile", &explored_tile);

		const simd::vector4 tile_map_size(float(minimap_size.x), float(minimap_size.y), 0.0f, 0.0f);
		visibility_shader.pixel_uniform_buffer->SetVector("tile_map_size", &tile_map_size);

		const simd::vector4 visibility_map_size(float(visibility_render_target->GetWidth()), float(visibility_render_target->GetHeight()), 0.0f, 0.0f);
		visibility_shader.pixel_uniform_buffer->SetVector("visibility_map_size", &visibility_map_size);

		const auto revealed_bound = tile_bound_revealed.value_or( simd::vector4::zero() );
		visibility_shader.pixel_uniform_buffer->SetVector( "revealed_bound", &revealed_bound );

		const auto use_revealed_bound = revealed_bound.sqrlen() > 0.0001f;

		visibility_shader.pixel_uniform_buffer->SetBool("use_revealed_bound", use_revealed_bound);

		visibility_shader.pixel_uniform_buffer->SetFloat("visibility_radius", 4.0f);
		visibility_shader.pixel_uniform_buffer->SetFloat("visibility_fully_revealed", float(tile_visibility_fully_revealed ? 1.0f : 0.0f));
		visibility_shader.pixel_uniform_buffer->SetFloat("visibility_walkable_revealed", float(tile_visibility_walkable_revealed ? 1.0f : 0.0f));
		visibility_shader.pixel_uniform_buffer->SetFloat("visibility_reset", float(tile_visibility_reset ? 1.0f : 0.0f));

		if (command_buffer.BindPipeline(visibility_shader.pipeline.Get()))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "curr_visibility_sampler", visibility_render_target_copy->GetTexture().Get() });
			inputs.push_back({ "walkability_sampler", walkability_texture.Get() });

			auto* pixel_binding_set = FindBindingSet("Minimap Visibility", device, visibility_shader.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("Minimap Visibility", visibility_shader.pipeline.Get(), pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { imagespace_shader.vertex_uniform_buffer.Get(), visibility_shader.pixel_uniform_buffer.Get() });

			command_buffer.BindBuffers(nullptr, imagespace_vertices.Get(), 0, sizeof(ImagespaceVertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();

		tile_visibility_fully_revealed = false;
		tile_visibility_walkable_revealed = false;
		tile_visibility_reset = false;
		last_explored = std::nullopt;
		tile_bound_revealed = std::nullopt;
	}

	void Minimap::SetVisible( const Vector2d& tile_loc )
	{
		last_explored = tile_loc;
	}

	void Minimap::SetVisibleBound( const simd::vector4& tile_bound )
	{
		tile_bound_revealed = tile_bound;
	}

	void Minimap::SetWalkableVisibleBound( const simd::vector4& tile_bound )
	{
		//setting both of these engages a special mode in the shader to only apply a walkable reveal to the bound
		tile_bound_revealed = tile_bound;
		tile_visibility_walkable_revealed = true;
	}

	void Minimap::SetAllVisible()
	{
		tile_visibility_fully_revealed = true;
	}

	void Minimap::SetWalkableVisible()
	{
		tile_visibility_walkable_revealed = true;
	}

	Minimap::~Minimap()
	{
		SaveCacheFile();
	}

	void ReadVisibilityFromStream( Utility::BufferStreamIn& src_stream, Device::RenderTarget& dst_lockable_target )
	{
		const size_t width = dst_lockable_target.GetWidth();
		const size_t height = dst_lockable_target.GetHeight();

		Device::LockedRect rect{};
		dst_lockable_target.LockRect(&rect, Device::Lock::DEFAULT);

		if (rect.pBits)
		{
			Engine::IOStatus::FileAccess file_access;
			Engine::IOStatus::LogFileRead(width * height);

			memset(rect.pBits, 0, rect.Pitch * height);

			::TexUtil::LockedData< MinimapTexUtil::ColorR8G8B8A8 > visibility_data(rect.pBits, rect.Pitch);

			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const char value = src_stream.Read< char >();
					auto& colour = visibility_data.Get( simd::vector2_int( x, y ) );
					colour.r = value;
					colour.g = 0;
					colour.b = 0;
					colour.a = 0;
				}
			}
		}

		dst_lockable_target.UnlockRect();
	}

	namespace
	{
		constexpr unsigned minimap_cache_hash_key = 0x54fe8a61u;
		
		unsigned GenerateMinimapCacheEncryptionKey( const std::wstring& filename, const unsigned data_hash )
		{
			const std::wstring file_target = PathHelper::PathTargetName( filename );
			const unsigned filename_hash = CompileTime::MurmurHash2( file_target.c_str(), (int)file_target.length(), 0x29a37f48u );
			return Utility::HashValues( filename_hash, data_hash, 0x45a2fb17u );
		}

		//very very basic (and trivially reversible) encryption. definitely NOT secure.
		void EncryptMinimapCache( std::vector< unsigned char >& data, const unsigned key )
		{
			constexpr uint32_t m = 0x5bd1e995;
			constexpr uint32_t r = 24;
			constexpr size_t stride_size = 4;
			const size_t num_strides = data.size() / stride_size;
			uint32_t k = key;
			for( size_t i = 0; i < num_strides; ++i )
			{
				uint32_t* block = (uint32_t*)( data.data() + ( i * stride_size ) );
				*block ^= k;
				
				k = uint32_t( uint64_t( k ) * m );
				k ^= k >> r;
				k = uint32_t( uint64_t( k ) * m );
			}

			for( size_t i = num_strides * stride_size; i < data.size(); ++i )
			{
				data[ i ] ^= static_cast< unsigned char >( k << ( i * 8 ) );
			}
		}

		void DecryptMinimapCache( std::vector< unsigned char >& data, const unsigned key )
		{
			EncryptMinimapCache( data, key );
		}

		std::vector< unsigned char > CompressEncryptMinimapCache( const unsigned char* data, const size_t size, const unsigned key )
		{
			std::vector< unsigned char > compressed_data( LZ4_compressBound( (int)size ) );
			if( compressed_data.empty() )
				return {}; //failure (data too large)

			const int compressed_size = LZ4_compress_default( (const char*)data, (char*)compressed_data.data(), (int)size, (int)compressed_data.size() );
			if( compressed_size <= 0 )
				return {}; //failure

			compressed_data.resize( compressed_size );
			EncryptMinimapCache( compressed_data, key );
			return compressed_data;
		}

		std::vector< unsigned char > DecompressDecryptMinimapCache( const std::vector< unsigned char >& encrypted_data, const unsigned key )
		{
			auto data = Copy( encrypted_data );
			DecryptMinimapCache( data, key );

			for( size_t attempts = 0, estimated_decompressed_size = data.size() * 3; attempts < 5; ++attempts, estimated_decompressed_size *= 3 )
			{
				std::vector< unsigned char > decompressed_data( estimated_decompressed_size );
				const int decompressed_size = LZ4_decompress_safe( (const char*)data.data(), (char*)decompressed_data.data(), (int)data.size(), (int)estimated_decompressed_size );
				if( decompressed_size > 0 )
				{
					decompressed_data.resize( decompressed_size );
					return decompressed_data;
				}
			}
			return data; //failure
		}
	}

	void Minimap::LoadCacheFile()
	{
		loaded_from_cache = false;

		if( cache_filename.empty() )
			return;

		if( !visibility_render_target || !lockable_target )
			return;

		PROFILE;

		try
		{
#if defined(WIN32) && !defined(_XBOX_ONE)
			std::ifstream stream( cache_filename, std::ios::binary | std::ios::in ); // On Windows, avoid WstringToString that could cause codepage conversion issues. #96852
#else
			std::ifstream stream( WstringToString( cache_filename ), std::ios::binary | std::ios::in );
#endif
			PROFILE_ONLY( File::RegisterFileAccess( cache_filename );)
			if( stream.fail() )
				return;

			unsigned data_size = 0, decompressed_data_hash = 0, terrain_hash = 0;
			unsigned short world_area_hash = 0;
			unsigned char mini_quest_state = 0;
			stream.read( (char*)&data_size, sizeof( data_size ) );
			stream.read( (char*)&decompressed_data_hash, sizeof( decompressed_data_hash ) );
			stream.read( (char*)&terrain_hash, sizeof( terrain_hash ) );
			stream.read( (char*)&world_area_hash, sizeof( world_area_hash ) );
			stream.read( (char*)&mini_quest_state, sizeof( mini_quest_state ) );
			data_size ^= decompressed_data_hash;
			terrain_hash ^= decompressed_data_hash;
			world_area_hash ^= decompressed_data_hash;
			mini_quest_state ^= decompressed_data_hash;
			if( stream.fail() || data_size == 0 )
				return;

			if( terrain_hash != data_source.GetTerrainHash() ||
				world_area_hash != data_source.GetWorldAreaHash() ||
				mini_quest_state != data_source.GetMiniQuestState() )
				return;

			std::vector< unsigned char > compressed_data( data_size );
			stream.read( (char*)compressed_data.data(), compressed_data.size() );
			if( stream.fail() )
				return;

			const unsigned encryption_key = GenerateMinimapCacheEncryptionKey( cache_filename, decompressed_data_hash );
			const auto decompressed_data = DecompressDecryptMinimapCache( compressed_data, encryption_key );

			const unsigned actual_data_hash = MurmurHash2( decompressed_data.data(), (int)decompressed_data.size(), minimap_cache_hash_key );
			if( decompressed_data_hash != actual_data_hash )
				return;

			Utility::BufferStreamIn buffer( decompressed_data.data(), decompressed_data.size() );
			ReadVisibilityFromStream( buffer, *lockable_target );

			lockable_target->CopyTo( visibility_render_target.get(), false );

			//visibility_render_target->SaveToFile(L"loaded_visibility_target.dds", Device::ImageFileFormat::DDS, nullptr, nullptr);
			//lockable_target->SaveToFile(L"loaded_lockable_target.dds", Device::ImageFileFormat::DDS, nullptr, nullptr);

			poi_base.Load( buffer );
			loaded_from_cache = true;
			tile_visibility_reset = false;
		}
		catch( const std::exception& e )
		{
			LOG_WARN( L"Failed to load minimap: " << StringToWstring( e.what() ) );
		}
	}

	void WriteVisibilityToStream( Utility::BufferStreamOut& dst_stream, Device::RenderTarget& src_lockable_target )
	{
		Device::LockedRect rect{};
		src_lockable_target.LockRect(&rect, Device::Lock::DEFAULT);
		TexUtil::LockedData< MinimapTexUtil::ColorR8G8B8A8 > visibility_data(rect.pBits, rect.Pitch);

		const size_t width = src_lockable_target.GetWidth();
		const size_t height = src_lockable_target.GetHeight();

		Engine::IOStatus::FileAccess file_access;
		Engine::IOStatus::LogFileWrite(width * height);

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				const char value = visibility_data.Get(simd::vector2_int( x, y ) ).r;
				dst_stream << value;
			}
		}

		file_access.Finish();
		src_lockable_target.UnlockRect();
	}

	void Minimap::SaveCacheFile() const
	{
		if( tile_visibility_reset )
			return;

		if( cache_filename.empty() )
			return;

		if( !visibility_render_target || !lockable_target )
			return;

		PROFILE;

		try
		{
			Utility::BufferStreamOut buffer( nullptr, 16 * Memory::KB );
			visibility_render_target->CopyToSysMem( lockable_target.get() );
			WriteVisibilityToStream( buffer, *lockable_target );
			poi_base.Save( buffer );

			const unsigned uncompressed_data_hash = MurmurHash2( buffer.GetBuffer(), (int)buffer.GetUsedSize(), minimap_cache_hash_key );

			const unsigned encryption_key = GenerateMinimapCacheEncryptionKey( cache_filename, uncompressed_data_hash );
			const auto compressed_data = CompressEncryptMinimapCache( buffer.GetBuffer(), buffer.GetUsedSize(), encryption_key );

#if defined(WIN32) && !defined(_XBOX_ONE)
			std::ofstream stream( cache_filename, std::ios::binary | std::ios::out ); // On Windows, avoid WstringToString that could cause codepage conversion issues. #96852
#else
			std::ofstream stream( WstringToString( cache_filename ), std::ios::binary | std::ios::out );
#endif
			if( stream.fail() )
				return;

			const unsigned data_size = (unsigned)compressed_data.size() ^ uncompressed_data_hash;
			const unsigned terrain_hash = data_source.GetTerrainHash() ^ uncompressed_data_hash;
			const unsigned short world_area_hash = data_source.GetWorldAreaHash() ^ uncompressed_data_hash;
			const unsigned char mini_quest_state = data_source.GetMiniQuestState() ^ uncompressed_data_hash;
			stream.write( (const char*)&data_size, sizeof( data_size ) );
			stream.write( (const char*)&uncompressed_data_hash, sizeof( uncompressed_data_hash ) );
			stream.write( (const char*)&terrain_hash, sizeof( terrain_hash ) );
			stream.write( (const char*)&world_area_hash, sizeof( world_area_hash ) );
			stream.write( (const char*)&mini_quest_state, sizeof( mini_quest_state ) );
			stream.write( (const char*)compressed_data.data(), compressed_data.size() );
		}
		catch( const std::exception& e )
		{
			LOG_WARN( L"Failed to save minimap: " << StringToWstring( e.what() ) );
		}
	}

} //namespace Terrain
