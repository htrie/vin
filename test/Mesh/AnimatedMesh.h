#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "Common/Utility/NonCopyable.h"
#include "Common/File/InputFileStream.h"
#include "Common/Resource/Handle.h"
#include "Common/Resource/DeviceUploader.h"
#include "Common/Geometry/Bounding.h"

#include "Visual/Material/Material.h"

#include "AnimatedMeshRawData.h"

class AnimatedMeshRenderer;

///Resource formats for mesh data
namespace Mesh
{
	//class AnimatedSkeleton;

	///Provides a resource that links mesh data with texture data
	class AnimatedMesh : public NonCopyable
	{
	private:
		class Reader;
	public:
		AnimatedMesh( const std::wstring& filename, Resource::Allocator& );
		~AnimatedMesh();

		/// @name Forwarding functions for data about the mesh
		//@{

		struct BoneGroup
		{
			std::string name;
			Memory::SmallVector<std::string, 16, Memory::Tag::Mesh> bone_names;
		};

		Device::Handle<Device::IndexBuffer> GetIndexBuffer() const { return mesh->GetIndexBuffer(); }
		unsigned GetIndexCount() const { return mesh->GetIndexCount(); }
		Device::Handle<Device::VertexBuffer> GetVertexBuffer() const { return mesh->GetVertexBuffer(); }
		unsigned GetVertexCount() const { return mesh->GetVertexCount(); }

		const BoundingBox& GetBoundingBox( ) const;
		unsigned char GetNumInfluences() const { return mesh.IsNull() ? 0 : mesh->GetNumInfluences(); }
		size_t GetNumSegments() const { return materials.size(); }
		const AnimatedMeshRawData::Segment& GetSegment( size_t i ) const;
		Memory::SmallVector<AnimatedMeshRawData::Segment, 16, Memory::Tag::Mesh>::const_iterator SegmentsBegin( ) const { return mesh->SegmentsBegin(); }
		Memory::SmallVector<AnimatedMeshRawData::Segment, 16, Memory::Tag::Mesh>::const_iterator SegmentsEnd( ) const { return mesh->SegmentsEnd(); }
		unsigned GetSegmentIndexByName( const std::wstring& name ) const;
		std::vector<unsigned> GetSegmentIndicesByNameSubstring( const std::wstring& name ) const;

		const std::vector<::File::PhysicalVertexData>     &GetPhysicalVertices() const {return mesh->GetPhysicalVertices();}
		const std::vector<int32_t>                        &GetPhysicalIndices() const {return mesh->GetPhysicalIndices();}
		const std::vector<::File::CollisionEllipsoidData> &GetCollisionEllipsoids() const { return mesh->GetCollisionEllipsoids(); }
		const std::vector<::File::CollisionSphereData>    &GetCollisionSpheres() const { return mesh->GetCollisionSpheres(); }
		const std::vector<::File::CollisionCapsuleData>   &GetCollisionCapsules() const { return mesh->GetCollisionCapsules(); }
		const std::vector<::File::CollisionBoxData>       &GetCollisionBoxes() const { return mesh->GetCollisionBoxes(); }
		bool                                              ColliderNamesSet() const { return mesh->ColliderNamesSet(); }
		const std::vector<int32_t>                        &GetPhysicalAttachmentVertices() const {return mesh->GetPhysicalAttachmentVertices();}
		//@}

		Resource::Handle< AnimatedMeshRawData, Resource::DeviceUploader > GetMesh() const { return mesh; }

		MaterialHandle GetMaterial( unsigned i ) const { return materials[ i ]; }
		Memory::Span<const BoneGroup> GetBoneGroups() const { return bone_groups; }

	private:
		friend AnimatedMeshRenderer;

		Resource::Handle< AnimatedMeshRawData, Resource::DeviceUploader > mesh;

		// Materials
		Memory::SmallVector<Resource::Handle< Material>, 16, Memory::Tag::Mesh> materials;
		Memory::Vector<BoneGroup, Memory::Tag::Mesh> bone_groups;

		// Bounding box
		BoundingBox bounding_box;
		bool bounding_box_set = false;
	};

	typedef Resource::Handle< AnimatedMesh > AnimatedMeshHandle;
}