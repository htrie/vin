#pragma once

#if defined(_XBOX_ONE)

namespace Device
{

	class ESRAM
	{
		ESRAM();
		ESRAM(const ESRAM&) = delete;
		ESRAM& operator=(const ESRAM&) = delete;
		virtual ~ESRAM() = default;

	public:
		static ESRAM* Get() {
			auto console_type = GetConsoleType();
			if (console_type <= CONSOLE_TYPE_XBOX_ONE_S)
			{
				static ESRAM instance;
				return &instance;
			}
			return nullptr;
		}

		bool CreateTexture2D(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc, ID3D11Texture2D** texture);
	};

}

#endif
