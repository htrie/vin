#pragma once

#include <cstdint>
#include "Visual/Physics/Maths/VectorMaths.h"
#include "Common/FileReader/SMDReader.h"

namespace Mesh
{
	const size_t MaxBoneInfluences = 4;
}

namespace File
{
	typedef FileReader::SMDReader::Header SkinnedMeshHeader;
	typedef FileReader::SMDReader::SkinnedMeshSegmentMetadata SkinnedMeshSegmentMetadata;
	typedef FileReader::SMDReader::PhysicalRepresentationHeader PhysicalRepresentationHeader;

	#pragma pack( push, 1 )

		struct PhysicalVertexData
		{
			Physics::Vector3f pos;
			int32_t bone_indices[4];
			float bone_weights[4];
		};

		struct CollisionEllipsoidData
		{
			Physics::Coords3f coords;
			Physics::Vector3f radii;
			int32_t bone_index;
			std::wstring name;
		};

		struct CollisionSphereData
		{
			Physics::Vector3f pos;
			float radius;
			int32_t bone_index;
			std::wstring name;
		};

		struct CollisionCapsuleData
		{
			int32_t sphere_indices[2];
		};

		struct CollisionBoxData
		{
			Physics::Coords3f coords;
			Physics::Vector3f half_size;
			int32_t bone_index;
			std::wstring name;
		};
		//@}

	#pragma pack( pop )
}
