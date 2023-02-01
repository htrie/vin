
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

	void SpriteBatch::OnLostDevice()
	{
		line->OnLostDevice();
	}

	void SpriteBatch::OnResetDevice()
	{
		line->OnResetDevice();
	}

	void SpriteBatch::Begin()
	{
		line->Begin();
	}

	void SpriteBatch::End()
	{
		line->End();
	}
}
