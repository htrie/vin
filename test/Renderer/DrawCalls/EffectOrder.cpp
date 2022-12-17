#include "EffectOrder.h"

namespace Renderer
{
	namespace DrawCalls
	{
		namespace EffectOrder
		{
			const wchar_t* EffectOrderNames[(unsigned)Value::Count] =
			{
				L"Ground",
				L"Decals",
				L"Water",
				L"PreEffect",
				L"Effects",
				L"Default",
				L"Last"
			};

			const wchar_t** EffectOrderNamesEnd = EffectOrderNames + (sizeof(EffectOrderNames) / sizeof(wchar_t*));
		}
	}
}
