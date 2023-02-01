#pragma once

namespace Device
{
	RootSignatureD3D12Preset::RootSignatureD3D12Preset(uint32_t num_constant_buffers, uint32_t num_srv_vertex, uint32_t num_srv_pixel, uint32_t num_uav_vertex, uint32_t num_uav_pixel, uint32_t num_samplers)
	{
		is_compute = false;
		constant_buffer_count = num_constant_buffers;
		sampler_count = num_samplers;
		srv_count.fill(0);
		uav_count.fill(0);

		srv_count.at(ShaderType::VERTEX_SHADER) = num_srv_vertex;
		srv_count.at(ShaderType::PIXEL_SHADER) = num_srv_pixel;
		uav_count.at(ShaderType::VERTEX_SHADER) = num_uav_vertex;
		uav_count.at(ShaderType::PIXEL_SHADER) = num_uav_pixel;
	}

	RootSignatureD3D12Preset::RootSignatureD3D12Preset(uint32_t num_constant_buffers, uint32_t num_srv, uint32_t num_uav, uint32_t num_samplers)
	{
		is_compute = true;
		constant_buffer_count = num_constant_buffers;
		sampler_count = num_samplers;
		srv_count.fill(0);
		uav_count.fill(0);

		srv_count.at(ShaderType::COMPUTE_SHADER) = num_srv;
		uav_count.at(ShaderType::COMPUTE_SHADER) = num_uav;
	}

	RootSignatureD3D12Preset::RootSignatureD3D12Preset()
	{
		is_compute = false;
		constant_buffer_count = 0;
		sampler_count = 0;
		srv_count.fill(0);
		uav_count.fill(0);
	}

	D3D12_SHADER_VISIBILITY RootSignatureD3D12::ConvertShaderVisibilityD3D12(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::PIXEL_SHADER: return D3D12_SHADER_VISIBILITY_PIXEL;
		case ShaderType::VERTEX_SHADER: return D3D12_SHADER_VISIBILITY_VERTEX;
		case ShaderType::COMPUTE_SHADER:
		default: return D3D12_SHADER_VISIBILITY_ALL;
		}
	}

	void RootSignatureD3D12::AppendConstantBufferParameters(D3D12_SHADER_VISIBILITY visibility, uint32_t offset, uint32_t count, RootParametersVector& out_root_parameters)
	{
		for (uint32_t index = offset; index < count; index++)
		{
			CD3DX12_ROOT_PARAMETER p = {};
			p.InitAsConstantBufferView(index, 0, visibility);
			out_root_parameters.push_back(p);
		}

		max_uniform_root_parameters += count;
	}

	void RootSignatureD3D12::AppendTableParameters(ShaderType shader_type, uint32_t srv_count, uint32_t uav_count, uint32_t& in_out_descriptor_table_offset, RootParametersVector& out_root_parameters)
	{
		const auto visibility = ConvertShaderVisibilityD3D12(shader_type);

		// BaseShaderRegister is always 0 here
		const auto srv_table = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srv_count, 0, 0);
		tables.push_back(srv_table);
		CD3DX12_ROOT_PARAMETER srv_param;
		srv_param.InitAsDescriptorTable(1, &tables.back(), visibility);
		srv_parameters[shader_type].root_parameter_offset = (uint32_t)out_root_parameters.size();
		srv_parameters[shader_type].count = srv_table.NumDescriptors;
		srv_parameters[shader_type].descriptor_table_offset = in_out_descriptor_table_offset;
		srv_parameters[shader_type].register_offset = srv_table.BaseShaderRegister;
		in_out_descriptor_table_offset += srv_table.NumDescriptors;
		out_root_parameters.push_back(srv_param);

		const auto uav_table = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, uav_count, 0, 0);
		tables.push_back(uav_table);
		CD3DX12_ROOT_PARAMETER uav_param;
		uav_param.InitAsDescriptorTable(1, &tables.back(), visibility);
		uav_parameters[shader_type].root_parameter_offset = (uint32_t)out_root_parameters.size();
		uav_parameters[shader_type].count = uav_table.NumDescriptors;
		uav_parameters[shader_type].descriptor_table_offset = in_out_descriptor_table_offset;
		uav_parameters[shader_type].register_offset = uav_table.BaseShaderRegister;
		in_out_descriptor_table_offset += uav_table.NumDescriptors;
		out_root_parameters.push_back(uav_param);
	}

	void RootSignatureD3D12::AppendSamplers(uint32_t count, RootParametersVector& out_root_parameters)
	{
		if (!count) return;

		const auto sampler_table = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, count, 0, 0);
		tables.push_back(sampler_table);
		CD3DX12_ROOT_PARAMETER sampler_param;
		sampler_param.InitAsDescriptorTable(1, &tables.back(), D3D12_SHADER_VISIBILITY_ALL);
		sampler_parameters.root_parameter_offset = (uint32_t)out_root_parameters.size();
		sampler_parameters.count = sampler_table.NumDescriptors;
		sampler_parameters.descriptor_table_offset = 0;
		sampler_parameters.register_offset = sampler_table.BaseShaderRegister;
		out_root_parameters.push_back(sampler_param);
	}

	// device can be null if root signature only used for shader compilation (to generate string)
	RootSignatureD3D12::RootSignatureD3D12(IDeviceD3D12* device, const RootSignatureD3D12Preset& preset)
	{
		constant_buffer_count_per_shader_type = preset.constant_buffer_count;

		CD3DX12_ROOT_SIGNATURE_DESC root_signature_desc;

		// PS/VS
		if (!preset.is_compute)
		{
			AppendConstantBufferParameters(D3D12_SHADER_VISIBILITY_VERTEX, 0, preset.constant_buffer_count, root_parameters);
			AppendConstantBufferParameters(D3D12_SHADER_VISIBILITY_PIXEL, 0, preset.constant_buffer_count, root_parameters);

			uint32_t descriptor_table_offset = 0;
			AppendTableParameters(ShaderType::VERTEX_SHADER, preset.srv_count[ShaderType::VERTEX_SHADER], preset.uav_count[ShaderType::VERTEX_SHADER], descriptor_table_offset, root_parameters);
			AppendTableParameters(ShaderType::PIXEL_SHADER, preset.srv_count[ShaderType::PIXEL_SHADER], preset.uav_count[ShaderType::PIXEL_SHADER], descriptor_table_offset, root_parameters);
			AppendSamplers(preset.sampler_count, root_parameters);

			flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			root_signature_desc.Init((UINT)root_parameters.size(), root_parameters.data(), 0, nullptr, (D3D12_ROOT_SIGNATURE_FLAGS)flags);
		}
		// COMPUTE
		else
		{
			AppendConstantBufferParameters(D3D12_SHADER_VISIBILITY_ALL, 0, preset.constant_buffer_count, root_parameters);

			uint32_t descriptor_table_offset = 0;
			AppendTableParameters(ShaderType::COMPUTE_SHADER, preset.srv_count[ShaderType::COMPUTE_SHADER], preset.uav_count[ShaderType::COMPUTE_SHADER], descriptor_table_offset, root_parameters);
			AppendSamplers(preset.sampler_count, root_parameters);

			flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			root_signature_desc.Init((UINT)root_parameters.size(), root_parameters.data(), 0, nullptr, (D3D12_ROOT_SIGNATURE_FLAGS)flags);
		}

		if (device)
		{
			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			if (FAILED(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
				throw ExceptionD3D12("D3D12SerializeRootSignature failed");

			if (FAILED(device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(root_signature.GetAddressOf()))))
				throw ExceptionD3D12("CreateRootSignature failed");
		}
	}

	namespace RootSignatureStringHelper
	{
		const char* GetVisibilityString(D3D12_SHADER_VISIBILITY visibility)
		{
			return magic_enum::enum_name(visibility).substr(6).data();
		};

		// TODO: check if works on samplers
		const char* GetDescriptorRangeString(D3D12_DESCRIPTOR_RANGE_TYPE range_type)
		{
			return magic_enum::enum_name(range_type).substr(28).data();
		};

		const char* GetFilterType(FilterType filter)
		{
			switch (filter)
			{
			case FilterType::MIN_MAG_MIP_POINT:							return "FILTER_MIN_MAG_MIP_POINT"; break;
			case FilterType::MIN_MAG_POINT_MIP_LINEAR:					return "FILTER_MIN_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::MIN_POINT_MAG_LINEAR_MIP_POINT:			return "FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::MIN_POINT_MAG_MIP_LINEAR:					return "FILTER_MIN_POINT_MAG_MIP_LINEAR"; break;
			case FilterType::MIN_LINEAR_MAG_MIP_POINT:					return "FILTER_MIN_LINEAR_MAG_MIP_POINT"; break;
			case FilterType::MIN_LINEAR_MAG_POINT_MIP_LINEAR:			return "FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::MIN_MAG_LINEAR_MIP_POINT:					return "FILTER_MIN_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::MIN_MAG_MIP_LINEAR:						return "FILTER_MIN_MAG_MIP_LINEAR"; break;
			case FilterType::ANISOTROPIC:								return "FILTER_ANISOTROPIC"; break;
			case FilterType::COMPARISON_MIN_MAG_MIP_POINT:				return "FILTER_COMPARISON_MIN_MAG_MIP_POINT"; break;
			case FilterType::COMPARISON_MIN_MAG_POINT_MIP_LINEAR:		return "FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:	return "FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::COMPARISON_MIN_POINT_MAG_MIP_LINEAR:		return "FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR"; break;
			case FilterType::COMPARISON_MIN_LINEAR_MAG_MIP_POINT:		return "FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT"; break;
			case FilterType::COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:return "FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::COMPARISON_MIN_MAG_LINEAR_MIP_POINT:		return "FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::COMPARISON_MIN_MAG_MIP_LINEAR:				return "FILTER_COMPARISON_MIN_MAG_MIP_LINEAR"; break;
			case FilterType::COMPARISON_ANISOTROPIC:					return "FILTER_COMPARISON_ANISOTROPIC"; break;
			case FilterType::MINIMUM_MIN_MAG_MIP_POINT:					return "FILTER_MINIMUM_MIN_MAG_MIP_POINT"; break;
			case FilterType::MINIMUM_MIN_MAG_POINT_MIP_LINEAR:			return "FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return "FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::MINIMUM_MIN_POINT_MAG_MIP_LINEAR:			return "FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR"; break;
			case FilterType::MINIMUM_MIN_LINEAR_MAG_MIP_POINT:			return "FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT"; break;
			case FilterType::MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return "FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::MINIMUM_MIN_MAG_LINEAR_MIP_POINT:			return "FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::MINIMUM_MIN_MAG_MIP_LINEAR:				return "FILTER_MINIMUM_MIN_MAG_MIP_LINEAR"; break;
			case FilterType::MINIMUM_ANISOTROPIC:						return "FILTER_MINIMUM_ANISOTROPIC"; break;
			case FilterType::MAXIMUM_MIN_MAG_MIP_POINT:					return "FILTER_MAXIMUM_MIN_MAG_MIP_POINT"; break;
			case FilterType::MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:			return "FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:	return "FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:			return "FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR"; break;
			case FilterType::MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:			return "FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT"; break;
			case FilterType::MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:	return "FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR"; break;
			case FilterType::MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:			return "FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT"; break;
			case FilterType::MAXIMUM_MIN_MAG_MIP_LINEAR:				return "FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR"; break;
			case FilterType::MAXIMUM_ANISOTROPIC:						return "FILTER_MAXIMUM_ANISOTROPIC"; break;
			default: throw ExceptionD3D12("Unknown FilterType");
			}
		}

		const char* GetAddressMode(TextureAddress mode)
		{
			switch(mode)
			{
			case TextureAddress::WRAP: return "TEXTURE_ADDRESS_WRAP";
			case TextureAddress::MIRROR: return "TEXTURE_ADDRESS_MIRROR";
			case TextureAddress::CLAMP: return "TEXTURE_ADDRESS_CLAMP";
			case TextureAddress::BORDER: return "TEXTURE_ADDRESS_BORDER";
			case TextureAddress::MIRRORONCE: return "TEXTURE_ADDRESS_MIRROR_ONCE";
			default: throw ExceptionD3D12("Unknown TextureAddress");
			}
		}

		const char* GetComparisonFuncton(ComparisonFunc comparison)
		{
			switch (comparison)
			{
			case ComparisonFunc::NEVER: return "COMPARISON_NEVER";
			case ComparisonFunc::LESS: return "COMPARISON_LESS";
			case ComparisonFunc::EQUAL: return "COMPARISON_EQUAL";
			case ComparisonFunc::LESS_EQUAL: return "COMPARISON_LESS_EQUAL";
			case ComparisonFunc::GREATER: return "COMPARISON_GREATER";
			case ComparisonFunc::NOT_EQUAL: return "COMPARISON_NOT_EQUAL";
			case ComparisonFunc::GREATER_EQUAL: return "COMPARISON_GREATER_EQUAL";
			case ComparisonFunc::ALWAYS: return "COMPARISON_ALWAYS";
			default: throw ExceptionD3D12("Unknown ComparisonFunc");
			}
		}

		void WriteTable(std::wstringstream& stream, const D3D12_ROOT_PARAMETER& parameter)
		{
			const auto table = parameter.DescriptorTable.pDescriptorRanges;
			const auto range_type = table->RangeType;

			stream << L"DescriptorTable(" << GetDescriptorRangeString(range_type) << L"(";
			if (range_type == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
				stream << L"t";
			else if (range_type == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
				stream << L"u";
			else if (range_type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
				stream << L"s";
			else if (range_type == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
				stream << L"b";
			else throw ExceptionD3D12("Unknown D3D12_DESCRIPTOR_RANGE_TYPE");

			stream << table->BaseShaderRegister << L", numDescriptors=" << table->NumDescriptors 
			<< L", space=" << table->RegisterSpace << L", offset=" << table->OffsetInDescriptorsFromTableStart << L") "
			<< L", visibility=" << GetVisibilityString(parameter.ShaderVisibility) << L")";
		};

		void WriteCbv(std::wstringstream& stream, const D3D12_ROOT_PARAMETER& parameter)
		{
			stream << L"CBV(b" << parameter.Descriptor.ShaderRegister << L", space=" << parameter.Descriptor.RegisterSpace << L", visibility=" << GetVisibilityString(parameter.ShaderVisibility) << L")";
		};

		void WriteSampler(std::wstringstream& stream, uint32_t index, const Device::SamplerInfo& sampler)
		{
			stream << L"StaticSampler(s" << index << L", filter=" << GetFilterType(sampler.filter_type) << L", addressU=" << GetAddressMode(sampler.address_u)
				   << L", addressV=" << GetAddressMode(sampler.address_v) << L", addressW=" << GetAddressMode(sampler.address_w)
				   << L", mipLODBias=" << sampler.lod_bias << L", maxAnisotropy=" << sampler.anisotropy_level << L", comparisonFunc=" << GetComparisonFuncton(sampler.comparison_func)
				   << L", borderColor=STATIC_BORDER_COLOR_TRANSPARENT_BLACK" 
				   << L")";
		};
	}

	const std::wstring& RootSignatureD3D12::GetString() const
	{
		Memory::Lock lock(mutex);

		if (string_representation.empty())
		{
			std::wstringstream stream;

			// RootFlags
			stream << L"RootFlags(";
			if (flags)
			{
				bool first = true;
				for (auto& item : magic_enum::enum_values<D3D12_ROOT_SIGNATURE_FLAGS>())
				{
					if (!(flags & item)) continue;
					auto str = StringToWstring(magic_enum::enum_name(item).substr(26).data());
					if (!first)
						stream << L" | ";
					stream << str;
					first = false;
				}
			}
			else
			{
				stream << L"0";
			}
			stream << L")";

			// Root parameters
			for (auto& param : root_parameters)
			{
				stream << L", ";
				switch (param.ParameterType)
				{
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: RootSignatureStringHelper::WriteTable(stream, param); break;
				case D3D12_ROOT_PARAMETER_TYPE_CBV: RootSignatureStringHelper::WriteCbv(stream, param); break;
				default: throw ExceptionD3D12("Unknown root parameter type");
				}
			}

			string_representation = stream.str();
		}

		return string_representation;
	}

	void RootSignatureD3D12::DebugOutputRootSignature(std::wostream& stream, const D3D12_ROOT_SIGNATURE_DESC& desk)
	{
		stream << L"Flags = " << desk.Flags
			<< L", NumParameters: " << desk.NumParameters
			<< L", NumSamplers: " << desk.NumStaticSamplers
			<< L"\n";

		for (uint32_t i = 0; i < desk.NumParameters; i++)
		{
			auto& param = desk.pParameters[i];
			stream << L"Param " << i << L": visibility = " << param.ShaderVisibility << L", ParameterType = " << param.ParameterType;
			if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
			{
				stream << L", CBV register = " << param.Descriptor.ShaderRegister << L", space = " << param.Descriptor.RegisterSpace << L"\n";
			}
			else if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
			{
				stream << L", TABLE NumDescriptorRanges = " << param.DescriptorTable.NumDescriptorRanges << L"\n";
				for (uint32_t j = 0; j < param.DescriptorTable.NumDescriptorRanges; j++)
				{
					stream << L"\tRange " << j << L": BaseShaderRegister = " << param.DescriptorTable.pDescriptorRanges[j].BaseShaderRegister
						<< L", RegisterSpace = " << param.DescriptorTable.pDescriptorRanges[j].RegisterSpace
						<< L", NumDescriptors = " << param.DescriptorTable.pDescriptorRanges[j].NumDescriptors
						<< L", RangeType = " << param.DescriptorTable.pDescriptorRanges[j].RangeType
						<< L", OffsetInDescriptorsFromTableStart = " << param.DescriptorTable.pDescriptorRanges[j].OffsetInDescriptorsFromTableStart
						<< L"\n";
				}
			}
			else
			{
				stream << L", unknown param type\n";
			}
		}

		for (uint32_t i = 0; i < desk.NumStaticSamplers; i++)
		{
			auto& sampler = desk.pStaticSamplers[i];
			stream << L"Sampler " << i << L": register = " << sampler.ShaderRegister << L", RegisterSpace = " << sampler.RegisterSpace << L", visibility = " << sampler.ShaderVisibility
				<< L", Address = (" << sampler.AddressU << L", " << sampler.AddressV << L", " << sampler.AddressW << L")"
				<< L", BorderColor = " << sampler.BorderColor
				<< L", ComparisonFunc = " << sampler.ComparisonFunc
				<< L", Filter = " << sampler.Filter
				<< L", MaxAnisotropy = " << sampler.MaxAnisotropy
				<< L", MaxLOD = " << sampler.MaxLOD
				<< L", MinLOD = " << sampler.MinLOD
				<< L", MipLODBias = " << sampler.MipLODBias
				<< L"\n";
		}
	}

	void RootSignatureD3D12::DebugOutputRootSignature(std::wostream& stream, const void* data, size_t size)
	{
		ComPtr<ID3D12RootSignatureDeserializer> deserializer;

		D3D12CreateRootSignatureDeserializer(data, size, __uuidof(ID3D12RootSignatureDeserializer), &deserializer);

		auto deserialized_desc = deserializer->GetRootSignatureDesc();
		DebugOutputRootSignature(stream, *deserialized_desc);
	}
}