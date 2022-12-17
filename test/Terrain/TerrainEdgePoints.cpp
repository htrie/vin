#include "TerrainEdgePoints.h"
#include "Common/FileReader/EDPReader.h"
#include "Common/Utility/StringManipulation.h"

namespace Terrain
{
	class TerrainEdgePoints::Reader : public FileReader::EDPReader
	{
	private:
		TerrainEdgePoints* parent;
		Resource::Allocator& allocator;

	public:
		Reader(TerrainEdgePoints* parent, Resource::Allocator& allocator) : parent(parent), allocator(allocator) {}

		void SetEdgePoints(const EdgePoints& points)
		{
			parent->edge_point_data.reserve(points.size());
			for (const auto& point : points)
			{
				parent->edge_point_data.emplace_back(EdgePointDatum(point.name.c_str(), allocator));
				parent->edge_point_data.back().edge_types.reserve(point.edge_types.size());
				for (const auto& edge : point.edge_types)
					parent->edge_point_data.back().edge_types.emplace_back(edge.flipped, edge.filename);
			}
		}
	};

	TerrainEdgePoints::TerrainEdgePoints( const std::wstring& filename, Resource::Allocator& allocator )
		: edge_point_data( allocator )
	{
		FileReader::EDPReader::Read(filename, Reader(this, allocator));
	}
}