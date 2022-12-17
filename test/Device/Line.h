#pragma once

#include "Visual/Device/Constants.h"
#include "State.h"

namespace simd
{
	class vector2;
	class vector3;
	class matrix;
	class color;
}

namespace Device
{
	class IDevice;

	namespace Internal
	{
		class LineImpl;
	}

	class Line
	{
		static Memory::FlatSet<Line*, Memory::Tag::Device> lines;
		std::unique_ptr<Internal::LineImpl> impl;

	public:
		Line( IDevice* device, const Memory::DebugStringA<>& name, const BlendState& blend_state = DisableBlendState() );
		~Line();

		static std::unique_ptr<Line> Create(IDevice* device, const Memory::DebugStringA<>& name, const BlendState& blend_state = DisableBlendState() );

		static void SwapAll();

		void Swap();
		void OnResetDevice();
		void OnLostDevice();

		void Begin();
		void End();

		void Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& Color);
		void Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& StartColor, const simd::color& EndColor);
		void DrawTransform(const simd::vector3* pVertexList, DWORD dwVertexListCount, const simd::matrix* pTransform, const simd::color& Color);

		void SetPattern(DWORD dwPattern);
		DWORD GetPattern();
		void SetPatternScale(float fPatternScale);
		float GetPatternScale();
		void SetWidth(float width);
		float GetWidth();
		void SetAntialias(bool antialias);
		bool GetAntialias();
	};

}
