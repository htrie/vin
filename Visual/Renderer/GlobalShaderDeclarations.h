#pragma once

#include "Common/File/SHA-256.h"

namespace Device
{
	enum class Target : unsigned;
}

namespace Renderer
{
	const char* GetGlobalDeclaration(const Device::Target& target);
	const std::array<unsigned char, File::SHA256::Length>& GetGlobalDeclarationHash(const Device::Target& target);
	void SetupGlobalDeclarationHashes();
}