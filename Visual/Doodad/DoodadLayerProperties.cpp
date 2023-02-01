#include "DoodadLayerProperties.h"

#include <iostream>

#include "Common/Resource/Exception.h"
#include "Common/File/InputFileStream.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/Numeric.h"
#include "Common/FileReader/DLPReader.h"

#include "ClientInstanceCommon/Animation/AnimatedObjectType.h"
#include "ClientInstanceCommon/Animation/Components/AOSetComponent.h"

#include "Visual/Animation/Components/FixedMeshComponent.h"
#include "Visual/Mesh/FixedMesh.h"
#include "Visual/Mesh/VertexLayout.h"
#include "Visual/Doodad/DoodadCommon.h"
#include "Visual/Doodad/DoodadLayerDataSource.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Utility/DXBuffer.h"
#include "Visual/Utility/DXHelperFunctions.h"

namespace Doodad
{
	namespace
	{
#pragma pack(push, 1)
		struct DoodadVertex
		{
			simd::vector3 position;
			Vector4dByte normal;
			Vector4dByte tangent;
			simd::vector2_half uv;
			simd::vector3 center;
		};
#pragma pack(pop)
	}

	void FMDelete::operator()( Mesh::FixedMesh* m )
	{
		m->OnDestroyDevice(); delete m;
	}

	class DoodadLayerProperties::Reader : public FileReader::DLPReader
	{
	private:
		DoodadLayerProperties& parent;
	public:
		Reader( DoodadLayerProperties& parent ) : parent( parent ) {}

		void SetMetadata(const Metadata& metadata) override
		{
			parent.min_scale = metadata.min_scale;
			parent.max_scale = metadata.max_scale;
			parent.flags.set( DoodadLayerProperties::AllowWaving, metadata.waving );
			parent.flags.set( DoodadLayerProperties::AllowOnBlocking, metadata.allow_on_block );
			parent.max_rotation = metadata.max_rotation;
		}
		void AddDoodad(const Doodad& doodad) override
		{
			const unsigned doodads_per_group = Round< unsigned >( std::max( 0.0f, doodad.doodads_per_tile * ( DoodadLayerGroupSize * DoodadLayerGroupSize ) ) );

			parent.meshes.push_back( { doodad.mesh_filename, doodads_per_group, doodad.uv_coords } );
			parent.total_meshes += doodads_per_group;
		}
	};

	void DoodadLayerProperties::LoadDLPFromAO( const std::wstring& parent_filename, const Animation::AnimatedObjectTypeHandle& ao, Reader& file_reader )
	{
		if( const auto* aoset = ao->GetStaticComponent< Animation::Components::AOSetStatic >() )
		{
			if( aoset->fixed_aos.size() )
				throw Resource::Exception( parent_filename ) << L"DoodadLayerProperties may not contain fixed AOSets";
			for( const auto& weighted_ao : aoset->ao_set )
				LoadDLPFromAO( parent_filename, weighted_ao.first.ao, file_reader );
		}
		else if( const auto* fixed_mesh = ao->GetStaticComponent< Animation::Components::FixedMeshStatic >() )
		{
			for( const auto& mesh_info : fixed_mesh->fixed_meshes )
				file_reader.AddDoodad( { mesh_info.mesh.GetFilename().c_str(), 1.0f } );
		}
		else throw Resource::Exception( parent_filename ) << L"DoodadLayerProperties .ao file was not a fixed mesh or AOSet";
	}

	DoodadLayerProperties::DoodadLayerProperties( const std::wstring &filename, Resource::Allocator& )
		: layer_seed( MurmurHash2( filename.c_str(), static_cast< int >( filename.size() * sizeof( wchar_t ) ), 0x2321 ) )
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Doodad));

		Reader file_reader = Reader( *this );
		if( EndsWithCaseInsensitive( filename, L".dlp" ) )
		{
			FileReader::DLPReader::Read( filename, file_reader );
		}
		else if( EndsWithCaseInsensitive( filename, L".ao" ) )
		{
			LoadDLPFromAO( filename, filename, file_reader );
		}

		if( meshes.empty() )
			throw Resource::Exception( filename ) << L"There needs to be at least one doodad in the doodad layer properties";

		//Sort meshes in descending order of frequency so that later it is easier to do special round robin.
		std::stable_sort( meshes.begin(), meshes.end(), [&]( const MeshEntry& m1, const MeshEntry& m2 ) { return m1.count > m2.count; } );

		if( AllOfIf( meshes, []( const MeshEntry& me )
		{
			return me.count == 0;
		} ) )
		{
			flags.set( IsStatic );
			for( auto& me : meshes )
				me.count = 1;
		}
	}

	DoodadMesh DoodadLayerProperties::GetDoodadMesh(unsigned index) const 
	{
		if (index < doodad_meshes.size())
			return doodad_meshes[index];
		return {};
	}

	template <typename STORAGE>
	Memory::Vector<STORAGE, Memory::Tag::Doodad> ConcatenateIndices(
		const Memory::Vector< std::pair< simd::matrix, unsigned >, Memory::Tag::Doodad >& mesh_transforms,
		const Memory::Vector< std::unique_ptr< Mesh::FixedMesh, FMDelete >, Memory::Tag::Doodad >& loaded_meshes,
		const unsigned index_count )
	{
		//Build the index buffer
		Memory::Vector<STORAGE, Memory::Tag::Doodad> output( index_count, 0 );
		auto output_iterator = output.begin();

		//Loop through meshes
		unsigned base_index = 0;
		for( const auto& transform : mesh_transforms )
		{
			if( transform.second < loaded_meshes.size() )
			{
				//Copy indices across with base added in
				auto& lm = loaded_meshes[ transform.second ];
				Utility::LockBuffer< Device::IndexBuffer, unsigned short > input_lock( lm->Segments()[ 0 ].index_buffer, lm->Segments()[ 0 ].index_count, Device::Lock::READONLY );
				output_iterator = std::transform( input_lock.begin(), input_lock.end(), output_iterator, [&base_index]( const unsigned short index ) { return index + base_index; } );

				base_index += lm->GetVertexCount();
			}
		}

		//We should have reached the end of the output too.
		assert( output_iterator == output.end() );

		return output;
	}

	template <typename STORAGE>
	Device::Handle<Device::IndexBuffer> CreateIndexBuffer(Device::IDevice *device, const Memory::Vector<STORAGE, Memory::Tag::Doodad>& indices, unsigned index_count, Device::IndexFormat index_format)
	{
		Device::MemoryDesc sysMemDesc{};
		sysMemDesc.pSysMem          = indices.data();
		sysMemDesc.SysMemPitch      = 0;
		sysMemDesc.SysMemSlicePitch = 0;

		return Device::IndexBuffer::Create("IB Static Doodad Layer", device, index_count * sizeof(STORAGE), Device::UsageHint::WRITEONLY, index_format, Device::Pool::MANAGED, &sysMemDesc );
	}

	Device::Handle< Device::VertexBuffer > CreateVertexBuffer( Device::IDevice* device, const unsigned num_vertices )
	{
		const auto devicePool = Device::Pool::DEFAULT;
		const auto usageHint = (Device::UsageHint)( (unsigned)Device::UsageHint::WRITEONLY | (unsigned)Device::UsageHint::DYNAMIC );
		return Device::VertexBuffer::Create( "VB Static Doodad Layer", device, num_vertices * sizeof( DoodadVertex ), usageHint, devicePool, nullptr );
	}

	void DoodadLayerProperties::OnCreateDevice( Device::IDevice *device )
	{
		this->device = device;

		height_range.first = std::numeric_limits< float >::infinity();
		height_range.second = -std::numeric_limits< float >::infinity();

		//Load all the fixed meshes
		loaded_meshes.clear();
		loaded_meshes.resize( meshes.size() );
		auto lm = loaded_meshes.begin();
		for( auto m = meshes.begin(); m != meshes.end(); ++m, ++lm )
		{
			lm->reset( new Mesh::FixedMesh( m->filename, true ) );
			(*lm)->OnCreateDevice( device );
		}

		//Work out what material we are going to be using.
		if( loaded_meshes[ 0 ]->Segments().empty() )
			throw Resource::Exception( meshes[ 0 ].filename ) << " doesn't have exactly 1 material.";
		material = ConvertMaterialDesc(loaded_meshes[ 0 ]->Segments()[ 0 ].material);

		doodad_meshes.resize( loaded_meshes.size() );

		unsigned total_indices = 0;
		total_vertices = 0;

		lm = loaded_meshes.begin();
		auto mvb = doodad_meshes.begin();
		for( auto m = meshes.begin(); m != meshes.end(); ++m, ++lm, ++mvb )
		{
			//Make sure all of the meshes all have 1 segment using the material we are using.
			if( (*lm)->Segments().size() != 1 )
				throw Resource::Exception( m->filename ) << " doesn't have exactly 1 material.";

			if( ConvertMaterialDesc((*lm)->Segments()[ 0 ].material) != material )
				throw Resource::Exception( m->filename ) << " has a different material than the other meshes.";

			//Get the vertex / index count
			const unsigned num_vertices = (*lm)->GetVertexCount();
			const unsigned flags = (*lm)->GetFlags();
			const unsigned num_indices = (*lm)->Segments()[0].index_count;
			total_indices += num_indices * m->count;
			total_vertices += num_vertices * m->count;

			//Grab the vertex buffer from the fixed mesh
			mvb->vertex_buffer = (*lm)->GetVertexBuffer();
			mvb->index_buffer = (*lm)->GetIndexBuffer();
			mvb->vertex_count = num_vertices;
			mvb->index_count = num_indices;
			mvb->flags = flags;

			const auto& bounding_box = (*lm)->GetBoundingBox();

			height_range.first = std::min( height_range.first, bounding_box.minimum_point.z );
			height_range.second = std::max( height_range.second, bounding_box.maximum_point.z );

			assert((*lm)->Segments()[0].base_index == 0 );
		}
	}

	void DoodadLayerProperties::OnDestroyDevice()
	{
		device = nullptr;
		doodad_meshes.clear();
		material.Release();
		loaded_meshes.clear();
	}

	std::tuple< unsigned, unsigned, Device::Handle< Device::VertexBuffer >, Device::Handle< Device::IndexBuffer > > DoodadLayerProperties::FillStaticBuffer( const Memory::Vector< std::pair< simd::matrix, unsigned >, Memory::Tag::Doodad >& mesh_transforms, const DoodadLayerDataSource* terrain_height_projection ) const
	{
		PROFILE;

		assert( flags.test( IsStatic ) );

		unsigned num_vertices = 0;
		unsigned num_indices = 0;

		//Work out how many vertices and indices we will need
		for( const auto& [transform, index] : mesh_transforms )
		{
			if( index < doodad_meshes.size() )
			{
				const auto& mvb = doodad_meshes[ index ];
				num_vertices += mvb.vertex_count;
				num_indices += mvb.index_count;
			}
		}

		if( num_vertices == 0 )
			return { 0u, 0u, {}, {} };

		Device::Handle< Device::IndexBuffer > index_buffer = num_vertices > std::numeric_limits< unsigned short >::max() ?
			CreateIndexBuffer<unsigned int>( device, ConcatenateIndices<unsigned int>( mesh_transforms, loaded_meshes, num_indices ), num_indices, Device::IndexFormat::_32 ) :
			CreateIndexBuffer<unsigned short>( device, ConcatenateIndices<unsigned short>( mesh_transforms, loaded_meshes, num_indices ), num_indices, Device::IndexFormat::_16 );

		Device::Handle< Device::VertexBuffer > vertex_buffer = CreateVertexBuffer( device, num_vertices );
		Utility::LockBuffer< Device::VertexBuffer, DoodadVertex > output_lock( vertex_buffer, num_vertices, Device::Lock::DEFAULT );

		auto output = output_lock.begin();

#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
#define VALIDATE_VERTEX_LAYOUT
		unsigned validate_flags = 0;
		bool validate_first = true;
#endif

		for( const auto& [transform, index] : mesh_transforms )
		{
			if( index < doodad_meshes.size() )
			{
				const auto& mvb = doodad_meshes[ index ];
				const Mesh::VertexLayout layout( mvb.flags );
				const simd::vector3 centre_position( transform[ 3 ][ 0 ], transform[ 3 ][ 1 ], transform[ 3 ][ 2 ] );
				const float centre_height = terrain_height_projection ? terrain_height_projection->GetWorldspaceHeightAtPosition( Vector2d( centre_position.x, centre_position.y ) ) : 0.0f;
#ifdef VALIDATE_VERTEX_LAYOUT
				if( validate_first )
				{
					validate_first = false;
					validate_flags = mvb.flags;
				}
				else if( mvb.flags != validate_flags )
					throw Resource::Exception( meshes[ index ].filename ) << L" has different vertex layout than the other FixedMeshes";
#endif

				Utility::LockBuffer< Device::VertexBuffer, uint8_t > input_lock( mvb.vertex_buffer, mvb.vertex_count * layout.VertexSize(), Device::Lock::READONLY );
				for( int vertex_index = 0; vertex_index < (int)mvb.vertex_count; vertex_index++ )
				{
					const auto& src_position = layout.GetPosition( vertex_index, input_lock.data() );
					const auto& src_normal = layout.GetNormal( vertex_index, input_lock.data() );
					const auto& src_tangent = layout.GetTangent( vertex_index, input_lock.data() );
					const auto& src_uv = layout.GetUV1( vertex_index, input_lock.data() );

					DoodadVertex vo;
					vo.position = transform.mulpos( src_position );
					if( terrain_height_projection )
					{
						const float height_diff = terrain_height_projection->GetWorldspaceHeightAtPosition( Vector2d( vo.position.x, vo.position.y ) ) - centre_height;
						vo.position.z += ( abs( height_diff ) < 150.0f ) ? height_diff : 0.0f;
					}

					// process normal
					const Vector3d normal = src_normal.asSignedVector4dFromSignedNormalized().v3();
					simd::vector3 d3d_normal( normal.x, normal.y, normal.z );
					d3d_normal = transform.muldir( d3d_normal );
					vo.normal.setSignedNormalizedFromSignedFloat( Vector4d( d3d_normal[ 0 ], d3d_normal[ 1 ], d3d_normal[ 2 ], 0 ) );

					// process tangent
					const Vector3d tangent = src_tangent.asSignedVector4dFromSignedNormalized().v3();
					simd::vector3 d3d_tangent( tangent.x, tangent.y, tangent.z );
					d3d_tangent = transform.muldir( d3d_tangent );
					vo.tangent.setSignedNormalizedFromSignedFloat( Vector4d( d3d_tangent[ 0 ], d3d_tangent[ 1 ], d3d_tangent[ 2 ], 0 ) );
					vo.tangent.w = src_tangent.w; // keep the binormal direction
					vo.uv = src_uv;
					vo.center = centre_position;
					*output = vo;
					output++;
				}
			}
		}

		assert( output == output_lock.end() ); //Should be at the end of the locked area.

		return std::make_tuple( num_vertices, num_indices, vertex_buffer, index_buffer );
	}

	void DoodadLayerProperties::CheckAOFiles() const
	{
		for( auto iter = meshes.cbegin(); iter != meshes.cend(); ++iter )
		{
			File::InputFileStream fin( iter->filename );
		}
	}
}
