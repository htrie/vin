#pragma once

#include <istream>
#include <string>

#include "Common/File/InputFileStream.h"
#include "Common/Resource/Handle.h"
#include "Common/FileReader/BlendMode.h"

#include "Visual/Device/State.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"
#include "Visual/Renderer/DrawCalls/Uniform.h"
#include "Visual/Utility/TextureMetadata.h"

namespace File
{
	class InputFileStream;
}

namespace Renderer
{
	namespace DrawCalls
	{
		class EffectGraph;
		struct InstanceDesc;

		typedef std::shared_ptr<EffectGraph> EffectGraphHandle;
		typedef Memory::SmallVector<EffectGraphHandle, 4, Memory::Tag::EffectGraphs> EffectGraphHandles;

		typedef Memory::SmallStringW<64, Memory::Tag::EffectGraphs> Filename;
		typedef Memory::SmallVector<Filename, 4, Memory::Tag::EffectGraphs> Filenames;
	}
}

constexpr unsigned MAT_VERSION_NUMBER = 4;

// TODO: conversion code, remove once conversion is done
class MaterialCreator
{
public:
	MaterialCreator(std::wstring_view filename, const Renderer::DrawCalls::InstanceDescs& graphs) : filename(filename), graphs(graphs) { };
	const std::wstring GetCacheKey() const { return std::wstring(filename.data()); };
	Renderer::DrawCalls::Filename filename;
	Renderer::DrawCalls::InstanceDescs graphs;
};

class Material
{
	class Reader;

public:
	Material( std::wstring_view filename, Resource::Allocator& );
	~Material();

	// TODO: conversion code, remove once conversion is done
	Material(const MaterialCreator& creator, Resource::Allocator&);

	unsigned GetId() const { return id; }

	Renderer::DrawCalls::BlendMode GetBlendMode(const bool skip_main_graph = false) const;
	std::wstring_view GetShaderType() const { return shader_type.to_view(); }
	bool IsMinimapOnly() const;
	bool IsRoofSection() const { return is_roof_section; }
	bool HasConstant() const { return has_constant; }
	simd::vector4 GetMinimapColour() const;

	const Renderer::DrawCalls::Bindings& GetTileGroundBindings() const { return tile_ground_bindings; }
	const Renderer::DrawCalls::Uniforms& GetTileGroundUniforms() const { return tile_ground_uniforms; }
	const Renderer::DrawCalls::Bindings& GetMaterialOverrideBindings() const { return mat_override_bindings; }
	const Renderer::DrawCalls::Uniforms& GetMaterialOverrideUniforms() const { return mat_override_uniforms; }

	///@name Used only in the asset viewer
	//@{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
	static inline bool disable_minimap_only = false;
#endif
	std::wstring_view GetFilename( ) const { return filename.to_view(); }
	void SetMinimapColour( const DWORD colour );
	void SetMinimapColourMultiplier( const float multiplier );
	DWORD GetBaseMinimapColour( ) const { return minimap_colour; }
	float GetMinimapColourMultiplier( ) const { return minimap_colour_multiplier; }
	void SetMinimapOnly( const bool value ) { minimap_only = value; }
	float GetMinimapOnly( ) const { return minimap_only; }
	void SetRoofSection( bool _is_roof_section ) { is_roof_section = _is_roof_section; }
	bool GetRoofSection() const { return is_roof_section; }
	//@}

	// Effect graph related functions
	void LoadFromBuffer(std::wstring_view buffer);
	void RemoveEffectGraph(const unsigned i);
	void AddEffectGraph(std::shared_ptr<Renderer::DrawCalls::InstanceDesc>& desc, const unsigned index);
	const Renderer::DrawCalls::InstanceDescs& GetInstanceDescs() const { return instance_descs; }
	Renderer::DrawCalls::GraphDescs GetGraphDescs() const; // Slow.
	Renderer::DrawCalls::EffectGraphHandles GetEffectGraphs() const; // Slow. // [TODO] Remove and use filenames instead.
	Renderer::DrawCalls::Filenames GetEffectGraphFilenames() const;
	
	// for texture export
	Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects>& GetSourceTextures();
	const Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects>& GetSourceTextures() const;

	void RefreshMaterial();

private:
	using JsonEncoding = rapidjson::UTF16LE<>;
	using JsonDocument = rapidjson::GenericDocument<JsonEncoding>;
	using JsonValue = rapidjson::GenericValue<JsonEncoding>;

	static std::tuple<Renderer::DrawCalls::Bindings, Renderer::DrawCalls::Uniforms, Renderer::DrawCalls::Bindings, Renderer::DrawCalls::Uniforms> FindBaseTextures(const Material& material);
	
	unsigned id = 0;
	Renderer::DrawCalls::Filename filename;

	Renderer::DrawCalls::Bindings tile_ground_bindings;
	Renderer::DrawCalls::Bindings mat_override_bindings;
	Renderer::DrawCalls::Uniforms tile_ground_uniforms;
	Renderer::DrawCalls::Uniforms mat_override_uniforms;

	unsigned version_number;
	Renderer::DrawCalls::InstanceDescs instance_descs;
	Memory::SmallStringW<16, Memory::Tag::Material> shader_type;
	DWORD minimap_colour = -1;
	float minimap_colour_multiplier = 1.f;
	bool minimap_only = false;
	bool is_roof_section = false;
	bool has_constant = false;

	// for texture export
	Memory::Vector<TextureExport::TextureMetadata, Memory::Tag::Effects> textures;
};

typedef Resource::Handle< Material > MaterialHandle;
