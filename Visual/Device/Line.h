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
		std::unique_ptr<Internal::LineImpl> impl;

	public:
		Line(IDevice* device);
		~Line();

		static std::unique_ptr<Line> Create(IDevice* device);

		void Swap();
		void OnResetDevice();
		void OnLostDevice();

		void Begin();
		void End();

		void Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& Color, float thickness = 1.0f);
		void DrawTransform(const simd::vector3* pVertexList, DWORD dwVertexListCount, const simd::matrix* pTransform, const simd::color& Color, float thickness = 1.0f);
	};

}
