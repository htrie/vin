#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>
#include "Common/Utility/StringManipulation.h"
#include "Common/File/PathHelper.h"
#include "Common/File/FileSystem.h"
#include "Common/FileReader/TextureMetadataParser.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "TextureMetadata.h"

namespace TextureExport
{
	bool enable_texture_exporting = true;

	void SetEnableTextureExporting(bool b)
	{
		enable_texture_exporting = b;
	}

	bool IsTextureExportingEnabled()
	{
		return enable_texture_exporting;
	}

	class Parser : public FileReader::TextureMetadataParser
	{
	private:
		Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures;

	public:
		Parser(Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures) : textures(textures) {}

		void SetData(const Data& data) override final
		{
			if (!enable_texture_exporting)
				return;

			for (const auto& texture_obj : data)
			{
				TextureMetadata new_texture;
				new_texture.filename = texture_obj.filename;
				new_texture.format = Utility::StringToTexResourceFormat(WstringToString(texture_obj.format));
				new_texture.ref_count = texture_obj.ref_count;
				for (const auto& source_obj : texture_obj.sources)
				{
					TextureSourceInfo source_info;
					source_info.filename = source_obj.filename;
					source_info.src_mask = source_obj.src_mask;
					source_info.dest_mask = source_obj.dst_mask;
					new_texture.sources.push_back(std::move(source_info));
				}

				textures.push_back(std::move(new_texture));
			}
		}
	};

	void Load(const JsonValue& obj, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		Parser::Parse(obj, Parser(textures));
	}

	void LoadFromBuffer(const std::wstring& filename, const std::wstring& buffer, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		Parser::Parse(buffer, Parser(textures), filename);
	}

	void Save(JsonValue& obj, JsonAllocator& allocator, const Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		if (!enable_texture_exporting)
			return;

		using namespace rapidjson;

		JsonValue tex_array(kArrayType);
		for (const auto& texture : textures)
		{
			JsonValue texture_obj(kObjectType);

			const auto& filename = texture.filename;
			texture_obj.AddMember(L"filename", JsonValue().SetString(filename.c_str(), (int)filename.length(), allocator), allocator);
			const auto format = StringToWstring(Utility::TexResourceFormats[(unsigned)texture.format].name);
			texture_obj.AddMember(L"format", JsonValue().SetString(format.c_str(), (int)format.length(), allocator), allocator);

			JsonValue source_array(kArrayType);
			for (const auto& source : texture.sources)
			{
				JsonValue source_obj(kObjectType);

				const auto& source_filename = source.filename;
				source_obj.AddMember(L"filename", JsonValue().SetString(source_filename.c_str(), (int)source_filename.length(), allocator), allocator);
				const auto& src_mask = source.src_mask;
				if(!src_mask.empty())
					source_obj.AddMember(L"src_mask", JsonValue().SetString(src_mask.c_str(), (int)src_mask.length(), allocator), allocator);
				const auto& dest_mask = source.dest_mask;
				if(!dest_mask.empty())
					source_obj.AddMember(L"dest_mask", JsonValue().SetString(dest_mask.c_str(), (int)dest_mask.length(), allocator), allocator);

				source_array.PushBack(source_obj, allocator);
			}
			if(!texture.sources.empty())
				texture_obj.AddMember(L"sources", source_array, allocator);

			texture_obj.AddMember(L"count", JsonValue().SetUint(texture.ref_count), allocator);

			tex_array.PushBack(texture_obj, allocator);
		}
		if(!textures.empty())
			obj.AddMember(L"textures", tex_array, allocator);
	}

	void SaveToStream(std::wostream& stream, const Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		using namespace rapidjson;
		JsonDocument doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		Save(doc, allocator, textures);

		GenericStringBuffer<JsonEncoding> buffer;
		Writer<GenericStringBuffer<JsonEncoding>, JsonEncoding, JsonEncoding> writer(buffer);
		doc.Accept(writer);
		stream << buffer.GetString();
	}

	void AddTextureMetadata(const TextureMetadata& new_texture, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		auto found = std::find_if(textures.begin(), textures.end(), [&](const TextureMetadata& tex)
		{
			return tex.filename == new_texture.filename;
		});
		if (found == textures.end())
		{
			textures.push_back(new_texture);
			textures.back().ref_count = 1;
		}
		else
			++found->ref_count;
	}

	void RemoveTextureMetadata(const std::wstring& filename, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		auto found = std::find_if(textures.begin(), textures.end(), [&](const TextureMetadata& tex)
		{
			return tex.filename == filename;
		});
		if (found != textures.end())
		{
			--found->ref_count;
			if (found->ref_count == 0)
				textures.erase(found);
		}
	}

	const Memory::Vector<TextureSourceInfo, Memory::Tag::Effects>& GetSourcePathFromDDS(std::wstring dds_file, const Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		auto found = std::find_if(textures.begin(), textures.end(), [&](const TextureMetadata& tex)
		{
			return tex.filename == dds_file;
		});
		if (found != textures.end())
			return found->sources;
		static Memory::Vector<TextureSourceInfo, Memory::Tag::Effects> empty;
		return empty;
	}

	std::wstring GetDDSFromSourcePath(const std::wstring& tex_filename, Device::TexResourceFormat format)
	{
		std::wstring exported_filename = L"Art/";
		if (EndsWith(tex_filename, L".dds"))
		{
			exported_filename += tex_filename;
			return exported_filename;
		}
		else
		{
			exported_filename += PathHelper::RemoveExtension(tex_filename);
			std::wstringstream merged_filename;
			merged_filename << exported_filename << L"_" << StringToWstring(Utility::TexResourceFormats[(unsigned)format].name) << L".dds";
			return merged_filename.str();
		}
	}

	// Used for conversion purposes only
	void CreateTextureMetadataFromParameter(const JsonValue& param_obj, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		const auto& path_obj = param_obj[L"path"];
		if (!path_obj.IsNull())
		{
			TextureMetadata new_texture;

			std::wstring dds_name = path_obj.GetString();
			if (dds_name.empty())
				return;
			new_texture.filename = dds_name;

			const auto& format_obj = param_obj[L"format"];
			if (!format_obj.IsNull())
			{
				const std::wstring iformat = format_obj.GetString();
				new_texture.format = Utility::StringToTexResourceFormat(WstringToString(iformat));
			}

			{
				std::wstring temp = new_texture.filename;
				std::wstring post_art = L"Art/";
				if (BeginsWith(temp, post_art))
					temp = temp.substr(post_art.length());

				std::wstring post_substr = L".dds";
				auto pos = temp.rfind('_');
				if (pos != std::wstring::npos)
				{
					std::wstring to_compare = temp.substr(pos);

					for (unsigned i = 0; i < (unsigned)Device::TexResourceFormat::NumTexResourceFormat; ++i)
					{
						if (BeginsWith(to_compare, StringToWstring("_" + Utility::TexResourceFormats[i].name)))
						{
							post_substr = to_compare;
							break;
						}
					}

					std::wstring old_formats[] = { L"DXT1", L"DXT2", L"DXT3", L"DXT4", L"DXT5" };
					for (unsigned i = 0; i < (unsigned)(std::end(old_formats) - std::begin(old_formats)); ++i)
					{
						if (BeginsWith(to_compare, L"_" + old_formats[i]))
						{
							post_substr = to_compare;
							break;
						}
					}
				}
				RemoveSubstring(temp, post_substr);
				temp += L".png";

				TextureSourceInfo new_source;
				new_source.filename = temp;
				new_texture.sources.push_back(std::move(new_source));
			}

			AddTextureMetadata(new_texture, textures);
		}
	}

	void CreateTextureMetadataFromFragmentTex(const std::wstring& dds_filename, Memory::Vector<TextureMetadata, Memory::Tag::Effects>& textures)
	{
		if (dds_filename.empty())
			return;

		TextureMetadata new_texture;

		new_texture.filename = dds_filename;

		std::wstring temp = new_texture.filename;
		std::wstring post_art = L"Art/";
		if (BeginsWith(temp, post_art))
			temp = temp.substr(post_art.length());
		temp = PathHelper::ChangeExtension(temp, L"png");

		TextureSourceInfo new_source;
		new_source.filename = temp;
		new_texture.sources.push_back(std::move(new_source));

		AddTextureMetadata(new_texture, textures);
	}
}
