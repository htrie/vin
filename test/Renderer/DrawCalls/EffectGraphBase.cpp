#include "EffectGraphBase.h"

namespace Renderer
{
	namespace DrawCalls
	{
		//COUNTER_ONLY(auto& egdl_counter = Counters::Add(L"EffectGraphDrawDataList");)

		void EffectGraphStateOverwrite::Reset()
		{
			rasterizer.depth_bias.Reset();
			rasterizer.slope_scale.Reset();
			rasterizer.fill_mode.Reset();
			rasterizer.cull_mode.Reset();
			depth_stencil.depth_test_enable.Reset();
			depth_stencil.depth_write_enable.Reset();
			depth_stencil.stencil_state.Reset();
			depth_stencil.disable_depth_write_if_blending = false;
			blend.color_src.Reset();
		}

		unsigned int EffectGraphStateOverwrite::Hash() const
		{
			unsigned int hash = 0;
			hash = EffectGraphUtil::MergeTypeId(hash, rasterizer.depth_bias.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, rasterizer.slope_scale.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, rasterizer.fill_mode.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, rasterizer.cull_mode.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, depth_stencil.depth_test_enable.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, depth_stencil.depth_write_enable.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, depth_stencil.stencil_state.Hash());
			hash = EffectGraphUtil::MergeTypeId(hash, blend.color_src.Hash());
			if (depth_stencil.disable_depth_write_if_blending)
				hash = EffectGraphUtil::MergeTypeId(hash, 0xFF);

			return hash;
		}

		void EffectGraphStateOverwrite::Merge(const EffectGraphStateOverwrite& other)
		{
			rasterizer.depth_bias.Merge(other.rasterizer.depth_bias);
			rasterizer.slope_scale.Merge(other.rasterizer.slope_scale);
			rasterizer.fill_mode.Merge(other.rasterizer.fill_mode);
			rasterizer.cull_mode.Merge(other.rasterizer.cull_mode);
			depth_stencil.depth_test_enable.Merge(other.depth_stencil.depth_test_enable);
			depth_stencil.depth_write_enable.Merge(other.depth_stencil.depth_write_enable);
			depth_stencil.stencil_state.Merge(other.depth_stencil.stencil_state);
			depth_stencil.disable_depth_write_if_blending = depth_stencil.disable_depth_write_if_blending || other.depth_stencil.disable_depth_write_if_blending;
			blend.color_src.Merge(other.blend.color_src);
		}

		void EffectGraphStateOverwrite::Overwrite(Device::RasterizerState& state) const
		{
			rasterizer.depth_bias.Overwrite(state.depth_bias);
			rasterizer.slope_scale.Overwrite(state.slope_scale);
			rasterizer.fill_mode.Overwrite(state.fill_mode);
			rasterizer.cull_mode.Overwrite(state.cull_mode);
		}

		void EffectGraphStateOverwrite::Overwrite(Device::DepthStencilState& state, BlendMode blend_mode) const
		{
			depth_stencil.depth_test_enable.Overwrite(state.depth.test_enabled);
			depth_stencil.depth_write_enable.Overwrite(state.depth.write_enabled);
			depth_stencil.stencil_state.Overwrite(state.stencil);
			if (depth_stencil.disable_depth_write_if_blending && blend_mode > BlendMode::AlphaTest)
				state.depth.write_enabled = false;
		}

		void EffectGraphStateOverwrite::Overwrite(Device::BlendTargetState& state) const
		{
			blend.color_src.Overwrite(state.color.src);
		}

		void EffectGraphStateOverwrite::Overwrite(Device::BlendState& state) const
		{
			for (unsigned a = 0; a < Device::RenderTargetSlotCount; a++)
				Overwrite(state[a]);
		}
	}
}
