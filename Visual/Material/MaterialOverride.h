#pragma once

#include "Common/Utility/Enumerate.h"
#include "Visual/Material/Material.h"
#include "Visual/Terrain/TileMaterialOverrides.h"

inline const MaterialDesc& DetermineMaterialOverride( const MaterialDesc& original, const Terrain::TileMaterialOverridesHandle& overrides1, const Terrain::TileMaterialOverridesHandle& overrides2 )
{
	if( overrides1 )
	{
		const auto& mat = overrides1->GetMaterial( original );
		if( !(mat == original) )
			return mat;
	}
	if( overrides2 )
		return overrides2->GetMaterial( original );
	else
		return original;
}

template< typename MeshHandleType >
class MaterialOverride
{
public:
	const MaterialDesc& GetSegmentMaterial( const MeshHandleType& mesh, const unsigned segment_index ) const
	{
		const auto found = material_infos.find( mesh );
		if( found != material_infos.end() )
		{
			for( const auto& material_info : found->second )
				if( material_info.segment_index == segment_index )
					return material_info.material;
		}
		return mesh->GetMaterial( segment_index );
	}

	std::vector< MaterialDesc > GetAllMaterials( const MeshHandleType& mesh, const Terrain::TileMaterialOverridesHandle& fallback_overrides ) const
	{
		std::vector< MaterialDesc > materials;
		for( unsigned i = 0; i < mesh->GetNumSegments(); ++i )
			materials.push_back( GetMaterial( mesh, i, fallback_overrides ) );
		return materials;
	}

	const MaterialDesc& GetMaterial( const MeshHandleType& mesh, const unsigned segment_index, const Terrain::TileMaterialOverridesHandle& fallback_overrides ) const
	{
		const auto found = material_infos.find( mesh );
		if (found != material_infos.end())
		{
			for (const auto& material_info : found->second)
				if (material_info.segment_index == segment_index)
					return material_info.material;
		}
		return DetermineMaterialOverride( mesh->GetMaterial( segment_index ), static_overrides, fallback_overrides );
	}

	void AddMaterialOverrideInfo( const MeshHandleType& mesh, const unsigned segment_index, const MaterialDesc& material )
	{
		auto& infos = material_infos[mesh];
		for (auto& info : infos)
		{
			if (info.segment_index == segment_index)
			{
				info.material = std::move( material );
				return;
			}
		}

		Info info{};
		info.segment_index = segment_index;
		info.material = std::move( material );
		infos.push_back( std::move( info ) );
	}

#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
	Memory::SmallVector<std::pair<MeshHandleType, std::wstring>, 16, Memory::Tag::Mesh> errors;
#endif
	void SetTMO( Terrain::TileMaterialOverridesHandle mats ) { static_overrides = std::move( mats ); }
	const Terrain::TileMaterialOverridesHandle& GetTMO() const { return static_overrides; }

protected:

	struct Info
	{
		unsigned segment_index = 0;
		MaterialDesc material;
	};

	typedef Memory::SmallVector< Info, 8, Memory::Tag::Mesh > InfoList_t;
	Memory::FlatMap< MeshHandleType, InfoList_t, Memory::Tag::Mesh > material_infos;
	Terrain::TileMaterialOverridesHandle static_overrides;

	void LoadMaterialOverride( const MeshHandleType& mesh, const std::wstring& segment_name, const std::wstring& material )
	{
		const unsigned segment_index = mesh->GetSegmentIndexByName(segment_name);
		if( segment_index < mesh->GetNumSegments() )
			AddMaterialOverrideInfo( mesh, segment_index, MaterialDesc( material ) );
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
		else
			errors.push_back( std::make_pair( mesh, segment_name ) );
#endif
	}

	void SaveMaterialOverride( std::wostream& stream, const MeshHandleType& mesh ) const
	{
		if( !static_overrides.IsNull() )
			stream << L"\tmaterial_override = \"" << static_overrides.GetFilename() << L"\"";

		const auto found = material_infos.find(mesh);
		if (found != material_infos.end())
		{
			for (const auto& [index, info] : enumerate( found->second ) )
			{
				if (!(info.material == mesh->GetMaterial(info.segment_index)))
				{
					if( index == 0 && !static_overrides.IsNull() )
						stream << L"\r\n";

					stream << L"\t" << found->first->GetSegment(info.segment_index).name << L" = ";
					stream << L"\"" << std::wstring(info.material.GetFilename()) << L"\"";
						
					if( index < found->second.size() - 1 )
						stream << L"\r\n";
				}
			}
		}
	}
};


