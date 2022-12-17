#pragma once

namespace simd
{ 
	class vector3;
	class matrix;
	class color;
}

namespace Device
{
	class IDevice;
	class Texture;
	class Line;

	enum class SpriteFlags : unsigned
	{
		NONE,
		DONOTSAVESTATE,
		ALPHABLEND,
	};

	class SpriteBatch
	{
		Memory::DebugStringA<> name;

	public:
		static std::unique_ptr<SpriteBatch> Create(IDevice* device, const Memory::DebugStringA<>& name);

		SpriteBatch(IDevice* device, const Memory::DebugStringA<>& name);
		~SpriteBatch();

		void OnLostDevice();
		void OnResetDevice();

		void Begin(SpriteFlags flags);
		void SetTransform(const simd::matrix* pTransform);
		void Draw(Texture* pTexture, const Rect* pSrcRect, const simd::vector3* pCenter, const simd::vector3* pPosition, const simd::color& Color);
		void End();
		void Flush();

		bool InBeginPair() const;

		std::unique_ptr<Line> line; // TODO: Merge Line/Font.
	};
}
