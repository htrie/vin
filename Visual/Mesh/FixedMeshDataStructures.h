#pragma once

#include "FixedMesh.h"
#include "Common/FileReader/FMTReader.h"

namespace File
{
	typedef FileReader::FMTReader::FixedMeshHeader FixedMeshHeader;
	typedef FileReader::FMTReader::FixedMeshSegmentMetadata FixedMeshSegmentMetadata;
	typedef FileReader::FMTReader::FixedMeshParticleEmitterMetadata FixedMeshParticleEmitterMetadata;
	typedef FileReader::FMTReader::FixedMeshParticleEmitterPoint FixedMeshParticleEmitterPoint;
}
