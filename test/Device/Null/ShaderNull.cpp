
namespace Device
{
	class ShaderNull : public Shader
	{
		IDevice* device = nullptr;
		ShaderData bytecode;

		ShaderType type = ShaderType::NULL_SHADER;

	public:
		ShaderNull(const Memory::DebugStringA<>& name, IDevice* device);

		virtual bool Load(const ShaderData& _bytecode) final;
		virtual bool Upload(ShaderType type) final;

		virtual bool ValidateShaderCompatibility(Handle<Shader> pixel_shader) final;

		virtual const ShaderType GetType() const final { return type; }

		virtual unsigned GetInstructionCount() const final;
	};

	ShaderNull::ShaderNull(const Memory::DebugStringA<>& name, IDevice* device)
		: Shader(name)
		, device(device)
	{
	}

	bool ShaderNull::Load(const ShaderData& bytecode)
	{
		return true;
	}

	bool ShaderNull::Upload(ShaderType type)
	{
		return true;
	}

	bool ShaderNull::ValidateShaderCompatibility(Handle<Shader> pixel_shader)
	{
		return true;
	}

	unsigned ShaderNull::GetInstructionCount() const
	{
		return 0;
	}

}
