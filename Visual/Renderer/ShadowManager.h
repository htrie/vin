#pragma once

#include "Visual/Device/Resource.h"
#include "Visual/Device/Constants.h"

namespace Device
{
	class IDevice;
	class Texture;
	class RenderTarget;
}

namespace Renderer
{

	class ShadowManager
	{
	public:
		ShadowManager();

		void OnCreateDevice(Device::IDevice* device);
		void OnResetDevice(Device::IDevice* device, bool high_quality);
		void OnLostDevice();
		void OnDestroyDevice();

		simd::vector4 GetShadowAtlasOffsetScale(const uint32_t index) const;
		Device::Rect GetShadowAtlasRect(const uint32_t index) const;
		Device::RenderTarget* GetShadowRenderTarget() const { return &*depth_stencil; }
		Device::Handle<Device::Texture> GetShadowAtlas() const { return shadow_texture; }

		simd::vector2_int GetShadowAtlasCellSize() const;
		unsigned GetShadowAtlasSize() const { return shadow_atlas_size; }

	private:
		std::shared_ptr<Device::RenderTarget> depth_stencil;
		Device::Handle<Device::Texture> shadow_texture;
		unsigned shadow_atlas_size;

	public:
		// Global settings for shadows
		static const float near_plane;
		static const float depth_bias;
	};

}

