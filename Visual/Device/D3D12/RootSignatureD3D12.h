#pragma once

namespace Device
{
	struct RootSignatureD3D12Preset
	{
		enum SignatureType
		{
			Main,
			Compute
		};

		RootSignatureD3D12Preset(uint32_t num_constant_buffers, uint32_t num_srv_vertex, uint32_t num_srv_pixel, uint32_t num_uav_vertex, uint32_t num_uav_pixel, uint32_t num_samplers);
		RootSignatureD3D12Preset(uint32_t num_constant_buffers, uint32_t num_srv, uint32_t num_uav, uint32_t num_samplers);
		RootSignatureD3D12Preset();

		static RootSignatureD3D12Preset MakeCompute(uint32_t num_constant_buffers, uint32_t num_srv, uint32_t num_uav, uint32_t num_samplers)
		{
			return RootSignatureD3D12Preset(num_constant_buffers, num_srv, num_uav, num_samplers);
		}

		static RootSignatureD3D12Preset MakeGraphics(uint32_t num_constant_buffers, uint32_t num_srv_vertex, uint32_t num_srv_pixel, uint32_t num_uav_vertex, uint32_t num_uav_pixel, uint32_t num_samplers)
		{
			return RootSignatureD3D12Preset(num_constant_buffers, num_srv_vertex, num_srv_pixel, num_uav_vertex, num_uav_pixel, num_samplers);
		}

		uint32_t constant_buffer_count;
		std::array<uint32_t, magic_enum::enum_count<Device::ShaderType>()> srv_count;
		std::array<uint32_t, magic_enum::enum_count<Device::ShaderType>()> uav_count;
		uint32_t sampler_count;
		bool is_compute;
	};

	static const std::array<RootSignatureD3D12Preset, magic_enum::enum_count<RootSignatureD3D12Preset::SignatureType>()> RootSignatureD3D12Presets = {
		RootSignatureD3D12Preset::MakeGraphics(6, 16, 32, 8, 8, 16), // Main. 64 descriptors (constant buffers and samplers don't use extra descriptors)
		RootSignatureD3D12Preset::MakeCompute(6, 8, 8, 0)
	};

	// According to microsoft using a single RootSignature per render type is preferable. Generating RootSignature from reflection is pointless.
	// Also xbox needs the root signature to be known during shader compilation so we use a single root signature for all the shaders that are generated from fragments.
	// https://forums.xboxlive.com/questions/131241/providing-d3d12-root-signature-for-dxcompiler.html
	class RootSignatureD3D12 : public NonCopyable
	{
		typedef Memory::SmallVector<CD3DX12_ROOT_PARAMETER, 8, Memory::Tag::Device> RootParametersVector;

		struct RootTableParameters
		{
			uint32_t root_parameter_offset = 0; // root signature parameter index for table
			uint32_t descriptor_table_offset = 0;
			uint32_t register_offset = 0; // e.g. srv table with register offset 3 and count 2 will be in range t3..t5. Always 0 at the moment.
			uint32_t count = 0;
		};

		Memory::Array<CD3DX12_DESCRIPTOR_RANGE, 16> tables;
		RootParametersVector root_parameters;

		uint32_t flags = 0;
		uint32_t max_uniform_root_parameters = 0; // number of root parameters reserved for constant buffers (constant_buffer_count * 2)
		uint32_t constant_buffer_count_per_shader_type = 0;
		std::array<RootTableParameters, magic_enum::enum_count<Device::ShaderType>()> srv_parameters;
		std::array<RootTableParameters, magic_enum::enum_count<Device::ShaderType>()> uav_parameters;
		RootTableParameters sampler_parameters;
		ComPtr<ID3D12RootSignature> root_signature;
		mutable std::wstring string_representation;
		Memory::Mutex mutex;

	private:
		D3D12_SHADER_VISIBILITY ConvertShaderVisibilityD3D12(ShaderType type);

		void AppendConstantBufferParameters(D3D12_SHADER_VISIBILITY visibility, uint32_t offset, uint32_t count, RootParametersVector& out_root_parameters);

		void AppendTableParameters(ShaderType shader_type, uint32_t srv_count, uint32_t uav_count, uint32_t& in_out_descriptor_table_offset, RootParametersVector& out_root_parameters);

		void AppendSamplers(uint32_t count, RootParametersVector& out_root_parameters);

	public:
		static void DebugOutputRootSignature(std::wostream& stream, const D3D12_ROOT_SIGNATURE_DESC& desk);
		static void DebugOutputRootSignature(std::wostream& stream, const void* data, size_t size); // For serialized signature

		auto& GetSRVParameters() const { return srv_parameters; }
		auto& GetUAVParameters() const { return uav_parameters; }
		auto& GetSamplerParameters() const { return sampler_parameters; }
		uint32_t GetMaximumUniformRootParameters() const { return max_uniform_root_parameters; }
		uint32_t GetMaximumRootParameters() const { return (uint32_t)root_parameters.size(); }
		uint32_t GetConstantBufferCount() const { return constant_buffer_count_per_shader_type; }

		// device can be null if root signature only used for shader compilation (to generate string)
		RootSignatureD3D12(IDeviceD3D12* device, const RootSignatureD3D12Preset& preset);

		ID3D12RootSignature* GetRootSignature() const { return root_signature.Get(); }

		const std::wstring& GetString() const;
	};
}