
namespace Device
{

	std::unique_ptr<SpriteBatch> SpriteBatch::Create(IDevice* device, const Memory::DebugStringA<>& name)
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		return std::make_unique<SpriteBatch>(device, name);
	}

	SpriteBatch::SpriteBatch(IDevice* device, const Memory::DebugStringA<>& name)
		: line(Line::Create(device, name)), name(name)
	{
	}

	SpriteBatch::~SpriteBatch()
	{
	}

	bool SpriteBatch::InBeginPair() const
	{
		return false;
	}

	void SpriteBatch::OnLostDevice()
	{
		line->OnLostDevice();
	}

	void SpriteBatch::OnResetDevice()
	{
		line->OnResetDevice();
	}

	void SpriteBatch::Begin( SpriteFlags flags )
	{
		line->Begin();
	}

	void SpriteBatch::SetTransform( const simd::matrix* pTransform )
	{
	}

	void SpriteBatch::Draw( Texture* pTexture, const Rect *pSrcRect, const simd::vector3* pCenter, const simd::vector3* pPosition, const simd::color& Color )
	{
	}

	void SpriteBatch::End()
	{
		line->End();
	}

	void SpriteBatch::Flush()
	{
	}

}
