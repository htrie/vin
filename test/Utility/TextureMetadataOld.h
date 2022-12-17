#pragma once

#include <map>
#include "Common/Resource/Handle.h"
#include "Visual/Device/Texture.h"

// NODEGRAPH_TODO: To be deprecated class, retained for transition purposes (can be removed once full transition is done)

namespace Texture
{
	struct TextureDescriptionOld
	{
		Device::TexResourceFormat format;
		TextureDescriptionOld(Device::TexResourceFormat format) : format(format) {}
		inline bool operator == (const TextureDescriptionOld& other) const
		{
			return format == other.format;
		}
		inline bool operator<(const TextureDescriptionOld& other) const
		{
			return format < other.format;
		}
	};

	class TextureMetadataOld
	{
	public:
		TextureMetadataOld(const std::wstring& filename, Resource::Allocator&);

		void AddTextureDescription(Device::TexResourceFormat format);
		void RemoveTextureDescription(Device::TexResourceFormat format);
		void ProcessTextureInfos(std::function<void(const TextureDescriptionOld&)> func) const;
		void Save(std::wostream& stream) const;
		bool IsEmpty() const { return texture_infos.empty(); }

	private:
		std::map<TextureDescriptionOld, unsigned> texture_infos;
	};

	typedef Resource::Handle<TextureMetadataOld> TextureMetadataHandle;
}