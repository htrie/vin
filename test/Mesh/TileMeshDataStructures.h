#pragma once

#include "Common/FileReader/TGMReader.h"

namespace File
{
	typedef FileReader::TGMReader::BoundingBox TileMeshBoundingBox;
	typedef FileReader::TGMReader::Header TileMeshHeader;
	typedef FileReader::TGMReader::GroundSegment TileMeshGroundSegment;
	typedef FileReader::TGMReader::NormalSegment TileMeshNormalSegment;
}
