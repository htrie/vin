#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "Common/Utility/NonCopyable.h"
#include "Common/Utility/WindowsHeaders.h"
#include "Common/File/InputFileStream.h"
#include "Common/Geometry/Bounding.h"
#include "Common/Resource/Handle.h"

#include "Visual/Device/Resource.h"
#include "Common/Resource/DeviceUploader.h"
#include "Visual/Mesh/SkinnedMeshDataStructures.h"
#include "Visual/Device/VertexDeclaration.h"

namespace Device
{
	class VertexBuffer;
	class IndexBuffer;
}

namespace Mesh
{

	class AnimatedMesh;
	//class AnimatedSkeleton;

	///Vertex data for an animated mesh
	class AnimatedMeshRawData : public NonCopyable
	{
	private:
		class Reader;
	public:

		///Struct that represents a segment of the mesh that can be rendered independantly
		struct Segment
		{
			unsigned int base_index;
			unsigned int num_indices;
			std::wstring name;
		};

		AnimatedMeshRawData( const std::wstring& _filename, Resource::Allocator& );
		~AnimatedMeshRawData();

		BoundingBox GetBoundingBox( ) const;

		unsigned char GetNumInfluences() const { return max_influence_count; }

		Device::Handle<Device::IndexBuffer> GetIndexBuffer( ) const { return index_buffer; }
		unsigned GetIndexCount() const { return index_count; }
		Device::Handle<Device::VertexBuffer> GetVertexBuffer() const { return vertex_buffer; }
		unsigned GetVertexCount() const { return vertex_count; }
		
		uint32_t GetFlags() const { return flags; }
		const Device::VertexElements& GetVertexElements() const { return vertex_elements; }

		///@name Getting segments
		//@{
		size_t GetNumSegments() const { return segments.size(); }
		const Segment& GetSegment( size_t i ) const { return segments[i]; }
		Memory::SmallVector<Segment, 16, Memory::Tag::Mesh>::const_iterator SegmentsBegin( ) const { return segments.begin(); }
		Memory::SmallVector<Segment, 16, Memory::Tag::Mesh>::const_iterator SegmentsEnd( ) const { return segments.end(); }
		//@}

		///@name Getting physics data
		//@{
		const std::vector<::File::PhysicalVertexData>     &GetPhysicalVertices()    const { return physical_vertices; }
		const std::vector<int32_t>                        &GetPhysicalIndices()     const { return physical_indices; }
		const std::vector<::File::CollisionEllipsoidData> &GetCollisionEllipsoids() const { return collision_ellipsoids; }
		const std::vector<::File::CollisionSphereData>    &GetCollisionSpheres()    const { return collision_spheres; }
		const std::vector<::File::CollisionCapsuleData>   &GetCollisionCapsules()   const { return collision_capsules; }
		const std::vector<::File::CollisionBoxData>       &GetCollisionBoxes()      const { return collision_boxes; }
		const bool										  ColliderNamesSet()		const { return collider_names_set; }
		const std::vector<int32_t>                        &GetPhysicalAttachmentVertices() const {return physical_attachment_vertices;}
		//@}

		///@name Device callbacks
		//@{
		void OnCreateDevice( Device::IDevice* const device );
		void OnResetDevice( Device::IDevice* const device ) { }
		void OnLostDevice() { /* EMPTY */ }
		void OnDestroyDevice();
		//@}

	private:

		friend AnimatedMesh;

		unsigned char max_influence_count;  ///< Maximum number of influences per vertex for this mesh
		Memory::SmallVector<Segment, 16, Memory::Tag::Mesh> segments;  ///< List of segments (distinct parts of a mesh)
		Device::VertexElements vertex_elements;

		std::wstring filename;  ///< Mesh data filename

		Device::Handle<Device::VertexBuffer> vertex_buffer;
		unsigned vertex_count = 0;
		Device::Handle<Device::IndexBuffer> index_buffer;
		unsigned index_count = 0;
		unsigned base_index = 0;

		float minX, maxX, minY, maxY, minZ, maxZ;  ///< Bounding values for each axis
		uint32_t flags = 0;

		///@name Loads the mesh from a file
		//@{
		void LoadBinary( Device::IDevice* device );

		std::vector<::File::PhysicalVertexData> physical_vertices;
		std::vector<::File::CollisionEllipsoidData> collision_ellipsoids;
		std::vector<::File::CollisionSphereData> collision_spheres;
		std::vector<::File::CollisionCapsuleData> collision_capsules;
		std::vector<::File::CollisionBoxData> collision_boxes;
		std::vector<int32_t> physical_indices;
		std::vector<int32_t> physical_attachment_vertices;
		bool collider_names_set = false;
		//@}
	};

	typedef Resource::Handle< AnimatedMeshRawData, Resource::DeviceUploader > AnimatedMeshRawDataHandle;

}
