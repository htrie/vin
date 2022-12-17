#pragma once

namespace Renderer
{
	namespace DrawCalls
	{
		namespace EffectOrder
		{
			///Used to order draw calls within an blend mode.
			enum Value
			{
				Ground,
				Decals, 
				Water,
				PreEffect,
				Effects,
				Default,
				// 
				// https://redmine.office.grindinggear.com/issues/106904
				// Used to push some objects last (backfaced-culled or zero-area triangles).
				// - art/textures/masks/transparentobjects_nooutlinec.mat
				// - art/textures/masks/transparentobjects_nooutline2c.mat
				// They crash the subsequent draw calls on macOS 10.15 and AMD series 5000 GPUs.
				Last, 

				Count
			};

			extern const wchar_t* EffectOrderNames[(unsigned)Value::Count];
			extern const wchar_t** EffectOrderNamesEnd;
		}
	}
}
