#include <iostream>
#include <fstream>
#include <locale>
#include <codecvt>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/FileReader/Uncompress.h"
#include "Common/FileReader/MATReader.h"
#include "Common/Objects/Type.h"
#include "Common/Utility/uwofstream.h"

#include "Visual/Utility/TextureMetadata.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "Visual/Material/Material.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/EffectGraphNodeFactory.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Utility/GraphInstanceCollection.h"

#if !defined( CONSOLE ) && !defined( __APPLE__ )
#include "Visual/Utility/AssetCompression.h"
#endif

const float DefaultSpecularExponent = 5.0f;

namespace
{
	struct BaseTextureInfo
	{
		::Texture::Handle texture;
		size_t name_id;
		Renderer::DrawCalls::Stage stage;

		template <typename N>
		void Update(const N& params, size_t name_id, const Renderer::DrawCalls::Stage node_stage)
		{
			if (texture.IsNull() || stage < node_stage)
			{
				for (const auto& a : params)
				{
					if (auto b = a->GetBindingInputInfo(); b.id != Renderer::DrawDataNames::DrawDataIdentifiers::None && b.input.type == Device::BindingInput::Type::Texture && !b.input.texture_handle.IsNull())
					{
						texture = b.input.texture_handle;
						this->name_id = name_id;
						stage = node_stage;
					}
				}
			}
		}
	};

	std::optional<BaseTextureInfo> GetBaseTexture(const Renderer::DrawCalls::CustomParameterMap& parent_parameters, const Renderer::DrawCalls::InstanceDesc::InstanceParameterMap& parameters, const unsigned* names, size_t num_names)
	{
		BaseTextureInfo res;

		for (size_t a = 0; a < num_names; a++)
		{
			if (const auto& f = parameters.find(names[a]); f != parameters.end())
				res.Update(f->second, a, Renderer::DrawCalls::Stage::NumStage);
			else if (const auto& f = parent_parameters.find(names[a]); f != parent_parameters.end())
				res.Update(*f->second, a, f->second->GetStage());
		}

		if (res.texture.IsNull())
			return std::nullopt;

		return res;
	}
}

class Material::Reader : public FileReader::MATReader
{
private:
	Material* parent;
	std::wstring_view filename;

public:
	Reader(Material* parent, std::wstring_view filename)
		: parent(parent), filename(filename)
	{
		parent->textures.clear();
		parent->instance_descs.clear();
		parent->has_constant = false;
	}

	void SetTextureMetadata(rapidjson::GenericValue<rapidjson::UTF16LE<>>& data) override final
	{
		TextureExport::Load(data, parent->textures);
	}

	void SetDefaultGraph(std::wstring_view graph_name, const rapidjson::GenericValue<rapidjson::UTF16LE<>>& data) override final
	{
		parent->instance_descs.push_back(std::make_shared<Renderer::DrawCalls::InstanceDesc>(graph_name));
	}

	void AddGraphInstance(rapidjson::GenericValue<rapidjson::UTF16LE<>>& data) override final
	{
		auto instance_desc = std::make_shared<Renderer::DrawCalls::InstanceDesc>(filename, data);
		const auto graph = EffectGraph::System::Get().FindGraph(instance_desc->GetGraphFilename());
		if (!parent->has_constant || graph->IsIgnoreConstant())
		{
			parent->has_constant |= graph->IsConstant();	
			const auto index = static_cast<unsigned>(parent->instance_descs.size());
			parent->instance_descs.push_back(instance_desc);
		}
	}

	void SetMetadata(const Metadata& data) override final
	{
		parent->shader_type = data.shader_type;
		parent->minimap_colour = data.minimap_color;
		parent->minimap_colour_multiplier = data.minimap_color_factor;
		parent->minimap_only = data.minimap_only;
		parent->is_roof_section = data.roof_section;
	}
};

Material::Material( std::wstring_view filename, Resource::Allocator& )
	: id(Objects::Type::GetTypeNameHash(filename.data()))
	, filename(filename)
{
	PROFILE;

#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)	
	Utility::MaterialCollection::OnAddMaterial(this);
#endif

	RefreshMaterial();
}

Material::~Material()
{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
	Utility::MaterialCollection::OnRemoveMaterial(this);
#endif
}

// TODO: conversion code, remove once conversion is done
Material::Material(const MaterialCreator& creator, Resource::Allocator&)
	: filename(creator.filename), id(Objects::Type::GetTypeNameHash(creator.filename.data()))
{
	PROFILE;

	const auto graph_name = std::wstring(creator.filename.data()) + L".matgraph";
	instance_descs.emplace_back(std::make_shared<Renderer::DrawCalls::InstanceDesc>(graph_name));  // Main graph of the material
	instance_descs.insert(instance_descs.end(), creator.graphs.begin(), creator.graphs.end());
	std::tie(mat_override_bindings, mat_override_uniforms, tile_ground_bindings, tile_ground_uniforms) = FindBaseTextures(*this);
}

void Material::LoadFromBuffer(std::wstring_view buffer)
{
	PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::Material));
	Reader::ReadBuffer(buffer, Reader(this, filename.to_view()), filename.to_view());
	std::tie(mat_override_bindings, mat_override_uniforms, tile_ground_bindings, tile_ground_uniforms) = FindBaseTextures(*this);
}

void Material::RefreshMaterial()
{
	PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::Material));
	Reader::Read(filename.to_view(), Reader(this, filename.to_view()));
	std::tie(mat_override_bindings, mat_override_uniforms, tile_ground_bindings, tile_ground_uniforms) = FindBaseTextures(*this);
}

std::tuple<Renderer::DrawCalls::Bindings, Renderer::DrawCalls::Uniforms, Renderer::DrawCalls::Bindings, Renderer::DrawCalls::Uniforms> Material::FindBaseTextures(const Material& material)
{
	const unsigned AlbedoNames[] = { 
		Renderer::DrawCalls::EffectGraphUtil::HashString("ColorHeight_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("ColourHeight_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("AlbedoTransparency_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("AlbedoSpecMask_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("base_color_texture"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("Material01_AlbedoAO_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("ColourMetal_TEX"),
	};
	const unsigned NormalNames[] = { 
		Renderer::DrawCalls::EffectGraphUtil::HashString("NormalGlossAO_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("base_normalspec_texture"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("Material01_NormalHeightGloss_TEX"),
		Renderer::DrawCalls::EffectGraphUtil::HashString("NormalHeightGloss"),
	};

	BaseTextureInfo base_albedo_texture;
	BaseTextureInfo base_normal_texture;
	for (const auto& a : material.instance_descs)
	{
		Renderer::DrawCalls::CustomParameterMap parent_parameters;
		const auto graph = EffectGraph::System::Get().FindGraph(a->GetGraphFilename());
		Renderer::DrawCalls::GetParentCustomParameters(parent_parameters, graph);

		if (const auto albedo = GetBaseTexture(parent_parameters, a->parameters, AlbedoNames, std::size(AlbedoNames)))
			if (base_albedo_texture.texture.IsNull() || base_albedo_texture.stage <= albedo->stage)
				base_albedo_texture = *albedo;

		if (const auto normal = GetBaseTexture(parent_parameters, a->parameters, NormalNames, std::size(NormalNames)))
			if (base_normal_texture.texture.IsNull() || base_normal_texture.stage <= normal->stage)
				base_normal_texture = *normal;
	}

	Renderer::DrawCalls::Bindings mat_override_bindings;
	mat_override_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::OverrideAlbedoTexture), base_albedo_texture.texture);
	mat_override_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::OverrideNormalTexture), base_normal_texture.texture);

	Renderer::DrawCalls::Bindings tile_ground_bindings;
	tile_ground_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::TileGroundTexture), base_albedo_texture.texture);
	tile_ground_bindings.emplace_back(Device::IdHash(Renderer::DrawDataNames::TileGroundNormalTexture), base_normal_texture.texture);


	Renderer::DrawCalls::Uniforms mat_override_uniforms;
	mat_override_uniforms.Add(Renderer::DrawDataNames::OverrideAlbedoOpaque, Renderer::DrawCalls::Uniform(material.GetBlendMode() == Renderer::DrawCalls::BlendMode::Opaque));
	mat_override_uniforms.Add(Renderer::DrawDataNames::OverrideNormalSwizzle, Renderer::DrawCalls::Uniform(base_normal_texture.name_id == 1));

	Renderer::DrawCalls::Uniforms tile_ground_uniforms;
	mat_override_uniforms.Add(Renderer::DrawDataNames::TileGroundNormalSwizzle, Renderer::DrawCalls::Uniform(base_normal_texture.name_id == 1));

	return std::make_tuple(mat_override_bindings, mat_override_uniforms, tile_ground_bindings, tile_ground_uniforms);
}

Renderer::DrawCalls::BlendMode Material::GetBlendMode(const bool skip_main_graph /*= false*/) const
{
	// Note: the "skip_main_graph" here is just for particle emitters since they have their own blendmode and can only be overwritten by non-main graphs.  All other drawcalls checks the blendmode starting from the main graph.
	// In the future, we might just want to remove the particle only blendmode and do a conversion pass where all the stored particle blendmode should just be transfered to the material. We can then remove this skipping when that time comes.
	Renderer::DrawCalls::BlendMode final_blend_mode = skip_main_graph ? Renderer::DrawCalls::BlendMode::NumBlendModes : Renderer::DrawCalls::BlendMode::Opaque;
	bool main_graph = true;
	for (const auto& desc : instance_descs)
	{
		if (skip_main_graph && main_graph)
		{
			main_graph = false;
			continue;
		}
		if (const auto graph = EffectGraph::System::Get().FindGraph(desc->GetGraphFilename()); graph->IsBlendModeOverriden())
			final_blend_mode = graph->GetOverridenBlendMode();
	}
	return final_blend_mode;
}

bool Material::IsMinimapOnly() const
{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
	return !disable_minimap_only && minimap_only;
#else
	return minimap_only;
#endif
}

void Material::RemoveEffectGraph(const unsigned i)
{
	// Note: this is used in tools only
	auto itr = instance_descs.begin() + i;
	has_constant &= !EffectGraph::System::Get().FindGraph((*itr)->GetGraphFilename())->IsConstant();
	instance_descs.erase(itr);
	std::tie(mat_override_bindings, mat_override_uniforms, tile_ground_bindings, tile_ground_uniforms) = FindBaseTextures(*this);
}

void Material::AddEffectGraph(std::shared_ptr<Renderer::DrawCalls::InstanceDesc>& instance_desc, const unsigned index)
{
	// Note: this is used in tools only
	const auto graph = EffectGraph::System::Get().FindGraph(instance_desc->GetGraphFilename());
	bool allow = !has_constant || graph->IsIgnoreConstant() || index < instance_descs.size();
	if (allow)
	{
		has_constant |= graph->IsConstant();
		instance_descs.insert(instance_descs.begin() + index, instance_desc);
		std::tie(mat_override_bindings, mat_override_uniforms, tile_ground_bindings, tile_ground_uniforms) = FindBaseTextures(*this);
	}
}

Renderer::DrawCalls::GraphDescs Material::GetGraphDescs() const
{
	Renderer::DrawCalls::GraphDescs graph_descs;
	graph_descs.reserve(instance_descs.size());
	for (auto& desc : instance_descs)
		graph_descs.emplace_back(desc);
	return graph_descs;
}

Renderer::DrawCalls::EffectGraphHandles Material::GetEffectGraphs() const
{
	Renderer::DrawCalls::EffectGraphHandles effect_graphs;
	for (auto& desc : instance_descs)
		effect_graphs.push_back(::EffectGraph::System::Get().FindGraph(desc->GetGraphFilename()));
	return effect_graphs;
}

Renderer::DrawCalls::Filenames Material::GetEffectGraphFilenames() const
{
	Renderer::DrawCalls::Filenames filenames;
	for (auto& desc : instance_descs)
		filenames.push_back(std::wstring(desc->GetGraphFilename()));
	return filenames;
}

simd::vector4 Material::GetMinimapColour() const
{
	const auto colour =  simd::color(minimap_colour).rgba();
	return simd::vector4( colour.x * minimap_colour_multiplier, colour.y * minimap_colour_multiplier, colour.z * minimap_colour_multiplier, 1.0f );
}

void Material::SetMinimapColour( const DWORD colour )
{
	minimap_colour = colour;
}

void Material::SetMinimapColourMultiplier( const float multiplier )
{
	minimap_colour_multiplier = multiplier;
}

// for texture export
Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects>& Material::GetSourceTextures() { return textures; }
const Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects>& Material::GetSourceTextures() const { return textures; }
