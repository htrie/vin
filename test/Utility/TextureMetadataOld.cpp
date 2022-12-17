#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include "Common/File/InputFileStream.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/Utility/DXHelperFunctions.h"

#include "TextureMetadataOld.h"

namespace Texture
{
	TextureMetadataOld::TextureMetadataOld(const std::wstring& filename, Resource::Allocator&)
	{
		File::InputFileStream stream(filename);

		std::wstring buffer;
		buffer.resize(stream.GetFileSize());
		stream.read(&buffer[0], buffer.size());

		rapidjson::GenericDocument<rapidjson::UTF16<>> doc;
		doc.Parse<rapidjson::kParseTrailingCommasFlag>(&buffer[0]);

		if(doc.HasParseError() && doc.GetParseError() != rapidjson::kParseErrorDocumentEmpty)
			throw Resource::Exception(filename) << StringToWstring(std::string(rapidjson::GetParseError_En(doc.GetParseError())));

		for(auto& node_obj : doc[L"texture_descriptions"].GetArray())
		{
			std::string format = WstringToString(node_obj[L"format"].GetString());
			TextureDescriptionOld new_tex_desc(Utility::StringToTexResourceFormat(format));
			texture_infos[new_tex_desc] = 1;
		}
	}

	void TextureMetadataOld::AddTextureDescription(Device::TexResourceFormat format)
	{
		TextureDescriptionOld new_tex_desc(format);
		if(texture_infos.find(new_tex_desc) == texture_infos.end())
			texture_infos[new_tex_desc] = 1;
		else
			++texture_infos[new_tex_desc];
	}

	void TextureMetadataOld::RemoveTextureDescription(Device::TexResourceFormat format)
	{
		TextureDescriptionOld tex_desc(format);
		auto found = texture_infos.find(tex_desc);
		if(found != texture_infos.end())
		{
			--found->second;
			if(found->second == 0)
				texture_infos.erase(found);
		}
	}

	void TextureMetadataOld::ProcessTextureInfos(std::function<void(const TextureDescriptionOld&)> func) const
	{
		for(auto itr = texture_infos.begin(); itr != texture_infos.end(); ++itr)
			func(itr->first);
	}

	void TextureMetadataOld::Save(std::wostream& stream) const
	{
		using namespace rapidjson;
		Document doc;
		doc.SetObject();

		Document::AllocatorType& allocator = doc.GetAllocator();

		bool dirty = false;

		Value desc_array(kArrayType);
		for(auto itr = texture_infos.begin(); itr != texture_infos.end(); ++itr)
		{
			Value desc_obj(kObjectType);

			// format
			const std::string& format_str = Utility::TexResourceFormats[(unsigned)itr->first.format].name;
			desc_obj.AddMember("format", Value().SetString(format_str.c_str(), (int)format_str.length(), allocator), allocator);

			desc_array.PushBack(desc_obj, allocator);
		}

		if(!texture_infos.empty())
		{
			doc.AddMember("texture_descriptions", desc_array, allocator);
			dirty = true;
		}

		if(dirty)
		{
			StringBuffer buffer;
			PrettyWriter<StringBuffer> writer(buffer);
			doc.Accept(writer);
			stream << buffer.GetString();
		}
	}
}