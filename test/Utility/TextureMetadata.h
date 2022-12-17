#pragma once

#include <rapidjson/document.h>
#include "Common/Objects/Type.h"
#include "Visual/Device/Texture.h"

namespace TextureExport
{
	using JsonEncoding = rapidjson::UTF16LE<>;
	using JsonDocument = rapidjson::GenericDocument<JsonEncoding>;
	using JsonAllocator = JsonDocument::AllocatorType;
	using JsonValue = rapidjson::GenericValue<JsonEncoding>;

	struct TextureSourceInfo
	{
		std::wstring filename;
		std::wstring src_mask;
		std::wstring dest_mask;
	};

	struct TextureMetadata
	{
		std::wstring filename;
		Device::TexResourceFormat format = Device::TexResourceFormat::DXT5;
		Memory::Vector<TextureSourceInfo, Memory::Tag::Effects> sources;
		uint8_t ref_count = 0;

#ifndef PRODUCTION_CONFIGURATION
		Objects::Origin origin = Objects::Origin::Client;
#endif
	};

	void SetEnableTextureExporting(bool b);
	bool IsTextureExportingEnabled();

	void Load(const JsonValue& obj, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
	void LoadFromBuffer(const std::wstring& filename, const std::wstring& buffer, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures );
	void Save(JsonValue& obj, JsonAllocator& allocator, const Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
	void SaveToStream(std::wostream& stream, const Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);

	void AddTextureMetadata(const TextureMetadata& new_texture, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
	void RemoveTextureMetadata(const std::wstring& filename, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
	const Memory::Vector<TextureSourceInfo, Memory::Tag::Effects>& GetSourcePathFromDDS(std::wstring dds_file, const Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
	std::wstring GetDDSFromSourcePath(const std::wstring& tex_filename, Device::TexResourceFormat format);

	// Used for conversion purposes only
	void CreateTextureMetadataFromParameter(const JsonValue& param_obj, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
	void CreateTextureMetadataFromFragmentTex(const std::wstring& dds_filename, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures);
}