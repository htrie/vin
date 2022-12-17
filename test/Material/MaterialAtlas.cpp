
#include "MaterialAtlas.h"

#include "Common/File/InputFileStream.h"
#include "Common/FileReader/AtlasReader.h"

#include "Material.h"

namespace Materials
{
	class MaterialAtlas::Reader : public FileReader::AtlasReader
	{
	private:
		MaterialAtlas* parent;
	public:
		Reader(MaterialAtlas* parent) : parent(parent) {}

		void SetMaterial(const std::wstring& filename) override 
		{
			parent->material = MaterialHandle(filename);
		}
		void SetSize(const unsigned width, const unsigned height) override
		{
			parent->dimensions.x = width;
			parent->dimensions.y = height;
		}
		void SetMinimumPlacementDistance(unsigned int distance) override
		{
			parent->minimum_placement_distance = distance;
		}
		void AddGroup(const std::wstring& name, const simd::vector4& coords, unsigned int default_size) override
		{
			auto& group = parent->groups[name];
			group.name = name;
			group.default_size = default_size;
			group.coords.push_back(coords);
		}
	};

	MaterialAtlas::MaterialAtlas( const std::wstring filename, Resource::Allocator& )
		: minimum_placement_distance( 0u )
	{
		FileReader::AtlasReader::Read(filename, Reader(this));
	}

	MaterialAtlas::~MaterialAtlas()
	{

	}

	AtlasGroupHandle GetAtlasGroup( const MaterialAtlasHandle& atlas, const std::wstring& group_name )
	{
		if( atlas.IsNull() )
			return AtlasGroupHandle();

		const auto found = atlas->groups.find( group_name );
		if( found == atlas->groups.end() )
			return AtlasGroupHandle();

		return AtlasGroupHandle( atlas, &found->second );
	}

	void GetAtlasGroupNames( const MaterialAtlasHandle& atlas, std::vector< std::wstring >& names_out )
	{
		names_out = { };
		if( atlas.IsNull() )
			return;

		for ( const auto& p : atlas->GetGroups() )
			names_out.push_back( p.first );
	}

}

