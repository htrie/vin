

#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/OsAbstraction.h"

#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/Texture.h"

#include "ShadowManager.h"
#include "TargetResampler.h"

namespace Renderer
{
	constexpr uint32_t SHADOWMAP_SIZE_HIGH = 4096;
	constexpr uint32_t SHADOWMAP_SIZE_MEDIUM = 2048;
	constexpr uint32_t SHADOWMAP_SIZE_LOW = 1024;

	constexpr uint32_t SHADOWMAP_ROW_COUNT = 2;

	// Recommended near plane for our shadow projections
	const float ShadowManager::near_plane = 5.f;

	// Offset to apply for the depth comparison
#ifdef _XBOX_ONE
	const float ShadowManager::depth_bias = 0.0045f;
#else
	const float ShadowManager::depth_bias = 0.0025f;
#endif

	ShadowManager::ShadowManager()
		: shadow_atlas_size(SHADOWMAP_SIZE_LOW)
	{
	}

	void ShadowManager::OnResetDevice(Device::IDevice* device, bool high_quality)
	{
#if defined(_XBOX_ONE)
		shadow_atlas_size = IsScorpioOrEquivalent() ? SHADOWMAP_SIZE_HIGH : SHADOWMAP_SIZE_MEDIUM;
#elif defined(PS4)
		shadow_atlas_size = SHADOWMAP_SIZE_MEDIUM;
#elif defined(MOBILE)
		shadow_atlas_size = SHADOWMAP_SIZE_LOW;
#else
		shadow_atlas_size = high_quality ? SHADOWMAP_SIZE_HIGH : SHADOWMAP_SIZE_MEDIUM;
#endif

		//Create shadow maps and depth stencils
		shadow_texture = Device::Texture::CreateTexture("ShadowAtlas", device, shadow_atlas_size, shadow_atlas_size, 1, Device::UsageHint::DEPTHSTENCIL, Device::PixelFormat::D24S8, Device::Pool::DEFAULT, false, true, false);
		if (shadow_texture)
		{
			depth_stencil = Device::RenderTarget::Create("ShadowAtlas", device, shadow_texture, 0);
		}
	}

	simd::vector2_int ShadowManager::GetShadowAtlasCellSize() const
	{
		const simd::vector2_int atlas_size = simd::vector2_int(shadow_atlas_size, shadow_atlas_size);
		return simd::vector2_int(atlas_size.x / SHADOWMAP_ROW_COUNT, atlas_size.y / SHADOWMAP_ROW_COUNT);
	}

	simd::vector4 ShadowManager::GetShadowAtlasOffsetScale(const uint32_t index) const
	{
		const auto rect = GetShadowAtlasRect(index);
		const simd::vector2 scale = simd::vector2((float)rect.right - (float)rect.left, (float)rect.bottom - (float)rect.top) / (float)shadow_atlas_size;
		const simd::vector2 offset = simd::vector2((float)rect.left, (float)rect.top) / (float)shadow_atlas_size;

		return simd::vector4(offset.x, offset.y, scale.x, scale.y);
	}

	Device::Rect ShadowManager::GetShadowAtlasRect(const uint32_t index) const
	{
		const simd::vector2_int shadowmap_cell = simd::vector2_int(index % SHADOWMAP_ROW_COUNT, index / SHADOWMAP_ROW_COUNT);
		const simd::vector2_int shadow_cell_size = GetShadowAtlasCellSize();
		const simd::vector2_int offset = simd::vector2_int(shadowmap_cell.x * shadow_cell_size.x, shadowmap_cell.y * shadow_cell_size.y);

		return Device::Rect(offset.x, offset.y, offset.x + shadow_cell_size.x, offset.y + shadow_cell_size.y);
	}

	void ShadowManager::OnLostDevice()
	{
		depth_stencil.reset();
		// release this after 'depth_stencil' because that is the surface of this texture
		shadow_texture.Reset();
	}

	void ShadowManager::OnCreateDevice(Device::IDevice* device)
	{
	}

	void ShadowManager::OnDestroyDevice()
	{
	}
}
