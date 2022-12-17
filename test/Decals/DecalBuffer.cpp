#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/ContainerOperations.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Entity/EntitySystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Mesh/TileMeshInstance.h"
#include "Visual/Mesh/TileMesh.h"
#include "Visual/Material/MaterialAtlas.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/Shader.h"
#include "Visual/Utility/DXBuffer.h"

#include "ClientInstanceCommon/Terrain/TileMetrics.h"

#include "DecalDataSource.h"
#include "Decal.h"
#include "DecalBuffer.h"

namespace Decals
{
	namespace
	{
		const Device::VertexElements vertex_elements =
		{
			{0, 0,						Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0}, //Position
			{0, 3 * sizeof(float),		Device::DeclType::BYTE4N, Device::DeclUsage::TANGENT, 0 }, //Tangent
			{0, 4 * sizeof(float),		Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0}, //Texture co-ords
			{0, 5 * sizeof(float),		Device::DeclType::FLOAT16_4, Device::DeclUsage::TEXCOORD, 1}, //Uvclamp
			{0, 7 * sizeof(float),		Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 2}, //fade_timing
		};

	#pragma pack(push, 1)
		struct DecalVertex
		{
			simd::vector3 position;
			Vector4dByte tangent;
			simd::vector2_half uv;
			simd::vector4_half uv_clamp;
			simd::vector2 fade_timing;
		};
	#pragma pack(pop)
	}

	DecalBuffer::DecalBuffer( Device::IDevice* device, Device::Handle<Device::IndexBuffer> index_buffer, const DecalDataSource& data_source )
		: data_source( data_source ), index_buffer(index_buffer)
		, entity_id(Entity::System::Get().GetNextEntityID())
	{
		vertex_buffer = Device::VertexBuffer::Create("VB Decal Buffer", device, DecalBufferSize * sizeof(DecalVertex), (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::Pool::DEFAULT, nullptr );

		Reset();
	}


	void DecalBuffer::Reset()
	{
		Entity::System::Get().Destroy(entity_id);

		vertex_count = 0;
		index_count = 0;
		bounding_box.minimum_point.x = bounding_box.minimum_point.y = bounding_box.minimum_point.z = std::numeric_limits< float >::infinity();
		bounding_box.maximum_point.x = bounding_box.maximum_point.y = bounding_box.maximum_point.z = -std::numeric_limits< float >::infinity();
	}


	DecalBuffer::~DecalBuffer()
	{
		vertex_buffer.Reset();
	}

	void DecalBuffer::SetupSceneObjects(uint8_t scene_layers, const MaterialHandle& material)
	{
		this->material = material;
		this->scene_layers = scene_layers;

		auto shader_base = Shader::Base()
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph", L"Metadata/EngineGraphs/decal.fxgraph" }, 0)
			.AddEffectGraphs(material->GetEffectGraphFilenames(), 0)
			.SetBlendMode(material->GetBlendMode());
		Shader::System::Get().Warmup(shader_base); // [TODO] Do earlier.

		auto renderer_base = Renderer::Base()
			.SetShaderBase(shader_base)
			.SetVertexElements(vertex_elements);
		Renderer::System::Get().Warmup(renderer_base);

		if( vertex_count )
			CreateDrawCall();
	}


	void DecalBuffer::CreateDrawCall()
	{
		auto desc = Entity::Temp<Entity::Desc>(entity_id);
		desc->SetVertexElements(vertex_elements)
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph", L"Metadata/EngineGraphs/decal.fxgraph" }, 0)
			.AddEffectGraphs(material->GetGraphDescs(), 0)
			.SetType(Renderer::DrawCalls::Type::Decal)
			.SetVertexBuffer(vertex_buffer, vertex_count)
			.SetIndexBuffer(index_buffer, index_count, 0)
			.SetBoundingBox(bounding_box)
			.SetLayers(scene_layers)
			.SetBlendMode(material->GetBlendMode())
			.AddObjectUniforms(material->GetMaterialOverrideUniforms())
			.AddObjectBindings(material->GetMaterialOverrideBindings());
		Entity::System::Get().Create(entity_id, std::move(desc));
	}


	void DecalBuffer::DestroySceneObjects()
	{
		Entity::System::Get().Destroy(entity_id);

		material.Release();
		vertex_count = 0;
	}

	Location ToLocation( const Vector2d& pos )
	{
		return Location( int( floor( pos.x ) ), int( floor( pos.y ) ) );
	}

	namespace
	{
		float Sat( const float x )
		{
			return std::max( std::min( x, 1.0f ), 0.0f );
		}
	}


	//Checks if a line segment intersects with a square around the origin.
	bool LineSquareIntersects( simd::vector2 a, simd::vector2 b, const float size )
	{
		//Do x axis first.
		//Make sure that a and b are in order on y.
		if( a.y > b.y )
			std::swap( a, b );
		//We want to get the portion of a-b that is between -size / +size in x
		simd::vector2 a_b = b - a;
		float xhigh = a.x + a_b.x * Sat( ( -a.y + size ) / a_b.y );
		float xlow = b.x + (-a_b.x) * Sat( ( -b.y - size ) / (-a_b.y) );
		if( xlow > xhigh )
			std::swap( xlow, xhigh );
		if( xlow > size )
			return false;
		if( xhigh < -size )
			return false;
		//Y axis.
		//Make sure that the a and b are in order on x
		if( a.x > b.x )
		{
			std::swap( a, b );
			a_b = -a_b; //Distance vec also reverses
		}
		float yhigh = a.y + a_b.y * Sat( ( -a.x + size ) / a_b.x );
		float ylow = b.y + (-a_b.y) * Sat( ( -b.x - size ) / (-a_b.x) );
		if( ylow > yhigh )
			std::swap( ylow, yhigh );
		if( ylow > size )
			return false;
		if( yhigh < -size )
			return false;
		return true;
	}

	float Dot( const simd::vector2& a, const simd::vector2& b )
	{
		return a.x * b.x + a.y * b.y;
	}

	bool OriginInTri( const simd::vector2& a, const simd::vector2& b, const simd::vector2& c )
	{
		// Compute vectors        
		simd::vector2 v0 = c - a;
		simd::vector2 v1 = b - a;
		simd::vector2 v2 = -a;

		// Compute dot products
		float dot00 = Dot(v0, v0);
		float dot01 = Dot(v0, v1);
		float dot02 = Dot(v0, v2);
		float dot11 = Dot(v1, v1);
		float dot12 = Dot(v1, v2);

		// Compute barycentric coordinates
		float invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
		float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
		float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

		// Check if point is in triangle
		return (u > 0) && (v > 0) && (u + v < 1);
	}

	void DecalBuffer::AddDecal( const Decal& decal, const unsigned coord_index )
	{
		PROFILE;

		if( vertex_count == DecalBufferSize )
			return;

		const float flip_mult = decal.flipped ? -1.0f : 1.0f;

		const float sd = sin( decal.angle ), cd = cos( decal.angle );

		const float bounding_radius = ( abs( sd ) + abs( cd ) ) * decal.size;

		auto tangent = simd::vector4( flip_mult * -cd * decal.size, flip_mult * -sd * decal.size, 0.0f, 0.0f );
		tangent = tangent.normalize3();
		Vector4dByte tangent_4byte; tangent_4byte.setSignedNormalizedFromSignedFloat(Vector4d(tangent.x, tangent.y, tangent.z, flip_mult));

		const simd::vector4 uv_clamp = decal.texture->coords[ coord_index ];
		const simd::vector4_half uv_clamp_16f(uv_clamp.x, uv_clamp.y, uv_clamp.z, uv_clamp.w);

		const simd::vector2 clamp_diff( uv_clamp.z - uv_clamp.x, uv_clamp.w - uv_clamp.y );
		const simd::vector2 fade_timing( Renderer::Scene::System::Get().GetFrameTime(), decal.fade_in_time );

		// size_ratio is how much longer/wider the decal is on the x or y axis. size_long_x is whether the x is longer, or y is longer (or neither, if optional is empty)
		// size_ratio is always less than 1 if the decal is rectangular, so we can multiply each triangle in the vertex buffer to shrink it the appropriate amount (rather than divide)
		std::optional< bool > size_long_x;
		float size_ratio = 1.0f;
		const auto& dim = decal.texture.GetParent()->GetDimensions();
		const auto dx = clamp_diff.x * dim.x, dy = clamp_diff.y * dim.y;
		if( dx > dy )
		{
			size_ratio = dy / dx;
			size_long_x = true;
		}
		else if( dx < dy )
		{
			size_ratio = dx / dy;
			size_long_x = false;
		}

		const auto final_decal_width = ( size_long_x && *size_long_x ) ? decal.size / size_ratio : decal.size;
		const auto final_decal_height = ( size_long_x && !*size_long_x ) ? decal.size / size_ratio : decal.size;
		const float xmul = ( size_long_x && *size_long_x ) ? size_ratio : 1.0f;
		const float ymul = ( size_long_x && !*size_long_x ) ? size_ratio : 1.0f;

		// Limit tiles to check decals on. To prevent too many with certain ratios of rectangular decals, limit how far it can check
		const Utility::Bound potential_tiles(
			ToLocation( ( decal.position - bounding_radius / std::max( size_ratio, 0.075f ) ) / static_cast<float>(Terrain::WorldSpaceTileSize) ),
			ToLocation( ( decal.position + bounding_radius / std::max( size_ratio, 0.075f ) ) / static_cast<float>(Terrain::WorldSpaceTileSize) ) + 1
			);

		Utility::LockBuffer< Device::VertexBuffer, DecalVertex > lock
			( 
			vertex_buffer, 
			vertex_count, 
			DecalBufferSize - vertex_count,
			(vertex_count ? Device::Lock::NOOVERWRITE : Device::Lock::DISCARD )
			);

		auto current_vert = lock.begin();

		const auto tile_end = potential_tiles.end();
		for( auto tile = potential_tiles.begin(); tile != tile_end; ++tile )
		{
			if( !data_source.TileInstanceInBounds( *tile ) )
				continue;

			const auto& tile_instance = data_source.GetTileInstance( *tile );
			if( tile_instance.GetMesh().IsNull() )
				continue;

			const auto& world_matrix = tile_instance.GetWorldMatrix();

			const auto CheckMesh = [&]( const Device::Handle<Device::VertexBuffer>& vertex_buffer, const Device::Handle<Device::IndexBuffer>& index_buffer, const unsigned base_index, const unsigned index_count, const uint32_t flags )
			{
				bool added_vert = false;

				Utility::ForEachTriangleInBuffer
				(
					vertex_buffer,
					index_buffer,
					base_index,
					index_count / 3,
					Mesh::MaterialBlockTypes::GetMaterialBlockTypeVertexSize( flags ),
					[&]( const simd::vector3& a, const simd::vector3& b, const simd::vector3& c )
				{
					if( ( vertex_count + 3 ) > DecalBufferSize )
						return true; //stop iterating

					const auto z = simd::vector3( 0.0f, 0.0f, -0.1f ); // slightly offset decals vertically to avoid Z-fighting.

					const auto aw = world_matrix.mulpos( a + z );
					const auto bw = world_matrix.mulpos( b + z );
					const auto cw = world_matrix.mulpos( c + z );

					const simd::vector2 ac = simd::vector2( decal.position.x, decal.position.y ) - simd::vector2( aw.x, aw.y );
					const simd::vector2 bc = simd::vector2( decal.position.x, decal.position.y ) - simd::vector2( bw.x, bw.y );
					const simd::vector2 cc = simd::vector2( decal.position.x, decal.position.y ) - simd::vector2( cw.x, cw.y );

					const simd::vector2 aligned_a( flip_mult * ( cd * ac.x + sd * ac.y ) * xmul, ( -sd * ac.x + cd * ac.y ) * ymul );
					const simd::vector2 aligned_b( flip_mult * ( cd * bc.x + sd * bc.y ) * xmul, ( -sd * bc.x + cd * bc.y ) * ymul );
					const simd::vector2 aligned_c( flip_mult * ( cd * cc.x + sd * cc.y ) * xmul, ( -sd * cc.x + cd * cc.y ) * ymul );

					if( LineSquareIntersects( aligned_a, aligned_b, decal.size ) ||
						LineSquareIntersects( aligned_b, aligned_c, decal.size ) ||
						LineSquareIntersects( aligned_c, aligned_a, decal.size ) ||
						OriginInTri( aligned_a, aligned_b, aligned_c ) )
					{
						added_vert = true;
						simd::vector2 uv;

						current_vert->position = aw;
						current_vert->tangent = tangent_4byte;
						uv = clamp_diff * ( simd::vector2( 0.5f, 0.5f ) + aligned_a / ( 2 * decal.size ) ) + simd::vector2( uv_clamp.x, uv_clamp.y );
						current_vert->uv = simd::vector2_half( uv.x, uv.y );
						current_vert->uv_clamp = uv_clamp_16f;
						current_vert->fade_timing = fade_timing;
						++current_vert;
						++vertex_count;

						current_vert->position = bw;
						current_vert->tangent = tangent_4byte;
						uv = clamp_diff * ( simd::vector2( 0.5f, 0.5f ) + aligned_b / ( 2 * decal.size ) ) + simd::vector2( uv_clamp.x, uv_clamp.y );
						current_vert->uv = simd::vector2_half( uv.x, uv.y );
						current_vert->uv_clamp = uv_clamp_16f;
						current_vert->fade_timing = fade_timing;
						++current_vert;
						++vertex_count;

						current_vert->position = cw;
						current_vert->tangent = tangent_4byte;
						uv = clamp_diff * ( simd::vector2( 0.5f, 0.5f ) + aligned_c / ( 2 * decal.size ) ) + simd::vector2( uv_clamp.x, uv_clamp.y );
						current_vert->uv = simd::vector2_half( uv.x, uv.y );
						current_vert->uv_clamp = uv_clamp_16f;
						current_vert->fade_timing = fade_timing;
						++current_vert;
						++vertex_count;
					}
					return false; //keep iterating
				} );

				return added_vert;
			};

			const auto& ground_section = tile_instance.GetMesh()->mesh->GetGroundSection();
			if( ground_section.vertex_buffer && ground_section.index_count > 0 && ground_section.segments.size() > 0 )
			{
				if( CheckMesh( ground_section.vertex_buffer, ground_section.index_buffer, ground_section.base_index, ground_section.index_count, ground_section.flags ) )
				{
					BoundingBox new_bounding_box = Copy( ground_section.segments.front().bounding_box );
					new_bounding_box.transform( world_matrix );
					bounding_box.Merge( new_bounding_box );
				}
			}

			const auto& normal_section = tile_instance.GetMesh()->mesh->GetNormalSection();
			if( normal_section.vertex_buffer && normal_section.index_count > 0 && normal_section.segments.size() > 0 )
			{
				if( normal_section.all_decal_normal_segments ) //check entire section
				{
					if( CheckMesh( normal_section.vertex_buffer, normal_section.index_buffer, normal_section.base_index, normal_section.index_count, normal_section.flags ) )
					{
						BoundingBox new_bounding_box = normal_section.segments.front().bounding_box;
						for( size_t i = 1; i < normal_section.segments.size(); ++i )
							new_bounding_box.Merge( normal_section.segments[ i ].bounding_box );
						new_bounding_box.transform( world_matrix );
						bounding_box.Merge( new_bounding_box );
					}
				}
				else if( normal_section.any_decal_normal_segments ) //check each segment
				{
					for( const auto& segment : normal_section.segments )
					{
						if( segment.IsCreateNormalSectionDecalsSegment() && CheckMesh( normal_section.vertex_buffer, segment.index_buffer, segment.base_index, segment.index_count, normal_section.flags ) )
						{
							BoundingBox new_bounding_box = Copy( segment.bounding_box );
							new_bounding_box.transform( world_matrix );
							bounding_box.Merge( new_bounding_box );
						}
					}
				}
			}
		}

		index_count = vertex_count;

		Entity::System::Get().Destroy(entity_id);

		if( !material.IsNull() && vertex_count)
			CreateDrawCall();
	}

}
