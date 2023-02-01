#include <fstream>
#include <iostream>
#include <algorithm>

#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Counters.h"
#include "Common/FileReader/SMReader.h"

#include "AnimatedMesh.h"

namespace Mesh
{
	class AnimatedMesh::Reader : public FileReader::SMReader
	{
	private:
		AnimatedMesh* parent;
	public:
		Reader(AnimatedMesh* parent) : parent(parent) {}
		
		void SetMeshFile(const std::wstring& filename) override 
		{
			parent->mesh = Resource::Handle< AnimatedMeshRawData, Resource::DeviceUploader >(filename);
		}
		void SetMaterials(const Materials& materials) override
		{
			parent->materials.resize(materials.size());
			for (size_t a = 0; a < materials.size(); a++)
			{
				if (materials[a].length() > 0)
				{
					parent->materials[a] = MaterialDesc(materials[a]);
				}
				// else will be a null material
			}
		}
		void SetBoundingBox(const simd::vector3& min, const simd::vector3& max) override
		{
			parent->bounding_box = BoundingBox(min, max);
			assert(parent->bounding_box.valid());
			parent->bounding_box_set = true;
		}

		void SetBoneGroupCount(uint32_t count) override 
		{
			parent->bone_groups.resize(count);
		}

		void SetBoneGroup(uint32_t index, std::wstring name, BoneGroups bone_groups) override
		{
			parent->bone_groups[index].name = WstringToString(name);
			parent->bone_groups[index].bone_names.resize(bone_groups.size());
			for (uint32_t i = 0; i < bone_groups.size(); i++)
			{
				parent->bone_groups[index].bone_names[i] = WstringToString(bone_groups[i]);
			}
		}
	};

	COUNTER_ONLY(auto& am_counter = Counters::Add(L"AnimatedMesh");)

	AnimatedMesh::AnimatedMesh( const std::wstring& filename, Resource::Allocator& )
	{
		PROFILE;

		COUNTER_ONLY(Counters::Increment(am_counter);)

		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Mesh));

		FileReader::SMReader::Read(filename, Reader(this));
	}

	AnimatedMesh::~AnimatedMesh()
	{
		COUNTER_ONLY(Counters::Decrement(am_counter);)
	}

	const BoundingBox& AnimatedMesh::GetBoundingBox() const
	{
		if( !bounding_box_set )
		{
			if( !mesh.IsNull() )
			{
				const auto nonconst_this = const_cast< AnimatedMesh* >( this );
				nonconst_this->bounding_box = mesh->GetBoundingBox();
				nonconst_this->bounding_box_set = true;
			}
		}
		assert(bounding_box.valid());
		return bounding_box;
	}

	const AnimatedMeshRawData::Segment& AnimatedMesh::GetSegment( size_t i ) const
	{
		static AnimatedMeshRawData::Segment empty;

		return mesh.IsNull() ? empty : mesh->GetSegment( i );
	}

	unsigned AnimatedMesh::GetSegmentIndexByName( const std::wstring& name ) const
	{
		const auto found = std::find_if( mesh->SegmentsBegin(), mesh->SegmentsEnd(), [&]( const Mesh::AnimatedMeshRawData::Segment& segment )
		{
			return segment.name == name;
		});
		if( found != mesh->SegmentsEnd() )
			return static_cast< unsigned >( found - mesh->SegmentsBegin() );
		else
			return static_cast< unsigned >( mesh->GetNumSegments() );
	}

	std::vector<unsigned> AnimatedMesh::GetSegmentIndicesByNameSubstring( const std::wstring& name ) const
	{
		std::vector<unsigned> out;

		unsigned index = 0;
		std::for_each( mesh->SegmentsBegin(), mesh->SegmentsEnd(), [&]( const Mesh::AnimatedMeshRawData::Segment& segment )
		{
			if( segment.name.find( name ) != -1 )
				out.push_back( index );

			++index;
		});

		return out ;
	}

}

