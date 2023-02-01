
namespace Device
{
	namespace
	{
		struct SIGNATURE_DESC
		{
			std::string					SemanticName;
			UINT                        SemanticIndex;
			UINT                        Register;
			D3D_REGISTER_COMPONENT_TYPE ComponentType;
			BYTE                        Mask;
		};
		typedef std::vector< SIGNATURE_DESC >   Signature;
    }

	class ShaderD3D11 : public Shader
	{
		IDevice* pDevice;

		std::shared_ptr<std::vector<char>> metadata;

		uint32_t constantDataSize[UniformBufferSlotCount];
		uint8_t num_constantdata;
		Memory::VectorMap<uint32_t, ReflectionD3D::ConstantInfo, Memory::Tag::ShaderMetadata> constantInfos;	// Key is hash of the entire name
		uint32_t instruction_count = 0;

	public:
		ShaderD3D11(const Memory::DebugStringA<>& name, IDevice* pDevice);
		virtual ~ShaderD3D11();

		virtual bool Load(const ShaderData& _bytecode) final;
		virtual bool Upload(ShaderType type) final;

		virtual bool ValidateShaderCompatibility(Handle<Shader> pixel_shader) final;

		virtual const ShaderType GetType() const final;

		virtual unsigned GetInstructionCount() const final;

		size_t GetConstantDataSize(unsigned index) const { return constantDataSize[index]; }
		unsigned GetConstantDataCount() const { return num_constantdata; }

		ShaderData bytecode;

		std::unique_ptr<ID3D11VertexShader, Utility::GraphicsComReleaser> pVS;
		std::unique_ptr<ID3D11PixelShader, Utility::GraphicsComReleaser> pPS;
		std::unique_ptr<ID3D11ComputeShader, Utility::GraphicsComReleaser> pCS;

#ifdef _DEBUG
		Signature input_signature;
		Signature output_signature;
		HRESULT ExtractShaderValidationInfo();
#endif
	};
	
	ShaderD3D11::ShaderD3D11( const Memory::DebugStringA<>& name, IDevice* pDevice)
		: Shader(name)
		, pDevice( pDevice )
		, num_constantdata( 0 )
	{
		ZeroMemory( constantDataSize, sizeof( constantDataSize ) );
	}

	ShaderD3D11::~ShaderD3D11( )
	{
#ifdef _DEBUG
		input_signature.clear();
		output_signature.clear();
#endif
	}

#ifdef _DEBUG
	HRESULT ShaderD3D11::ExtractShaderValidationInfo()
	{
		HRESULT hres = S_OK;

		// do the extraction if the signatures are not yet set
		if (input_signature.size() || output_signature.size())
			return hres;

		std::unique_ptr< ID3D11ShaderReflection, Utility::ComReleaser > pReflect;
		ID3D11ShaderReflection* reflect_data = nullptr;
		hres = D3DReflect(bytecode.data(), bytecode.size(), IID_ID3D11ShaderReflection, (void**)&reflect_data);
		pReflect.reset(reflect_data);

		// Note: If this shader was compiled on Release, which means it's reflection data is stripped out, then we can't do the extraction
		if (SUCCEEDED(hres))
		{
			D3D11_SHADER_DESC shaderdesc;
			if (SUCCEEDED(pReflect->GetDesc(&shaderdesc)))
			{

				for (unsigned slot = 0; slot < shaderdesc.InputParameters; ++slot)
				{
					D3D11_SIGNATURE_PARAMETER_DESC desc;
					if (SUCCEEDED(pReflect->GetInputParameterDesc(slot, &desc)))
					{
						SIGNATURE_DESC temp_desc;
						temp_desc.SemanticName = desc.SemanticName;
						temp_desc.SemanticIndex = desc.SemanticIndex;
						temp_desc.Register = desc.Register;
						temp_desc.ComponentType = desc.ComponentType;
						temp_desc.Mask = desc.Mask;
						input_signature.push_back(std::move(temp_desc));
					}
				}

				for (unsigned slot = 0; slot < shaderdesc.OutputParameters; ++slot)
				{
					// Query the parameter from the signature.
					D3D11_SIGNATURE_PARAMETER_DESC desc;
					if (FAILED(pReflect->GetOutputParameterDesc(slot, &desc)))
					{
						assert(0); //@todo handle fail case.  Should this be throwing?
						continue;
					}

					SIGNATURE_DESC temp_desc;
					temp_desc.SemanticName = desc.SemanticName;
					temp_desc.SemanticIndex = desc.SemanticIndex;
					temp_desc.Register = desc.Register;
					temp_desc.ComponentType = desc.ComponentType;
					temp_desc.Mask = desc.Mask;
					output_signature.push_back(std::move(temp_desc));
				}
			}
		}

		return hres;
	}
#endif

	bool ShaderD3D11::Load( const ShaderData& _bytecode )
	{
		if (_bytecode.size() == 0)
			return false;

		// Passed bytecode here is just a stream of data.  Extract the header information first
		std::string data( _bytecode.data(), _bytecode.size() );
		std::istringstream in_stream( data );

		// read the current version
		if (!Compiler::CheckVersion<Target::D3D11>::Check(in_stream))
			return false;

		uint32_t group_size[3] = { 0, 0, 0 };
		in_stream.read((char*)&group_size[0], sizeof(uint32_t));
		in_stream.read((char*)&group_size[1], sizeof(uint32_t));
		in_stream.read((char*)&group_size[2], sizeof(uint32_t));
		workgroup.x = group_size[0];
		workgroup.y = group_size[1];
		workgroup.z = group_size[2];

		uint8_t size = 0;
		in_stream.read((char*)&size, sizeof(uint8_t));
		for (unsigned i = 0; i < size; i++)
		{
			auto param = std::make_shared<ShaderParameter>();
			ReflectionD3D::ReadString(in_stream, param->type);
			ReflectionD3D::ReadString(in_stream, param->name);
			ReflectionD3D::ReadString(in_stream, param->semantic);

			output_variables[param->semantic] = param;
		}

		uint16_t resource_size = 0;
		in_stream.read((char*)&resource_size, sizeof(uint16_t));
		for (unsigned i = 0; i < resource_size; i++)
		{
			ReflectionD3D::ResourceInfo rsc;
			rsc.Read(in_stream);
			
			switch (rsc.type)
			{
				case D3D_SIT_CBUFFER:
					buffers.emplace_back(rsc.id_hash, rsc.binding_point, rsc.size);
					break;
				case D3D_SIT_TEXTURE:
					bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::SRV));
					break;
				case D3D_SIT_TBUFFER:
					bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::SRV));
					break;
				case D3D_SIT_SAMPLER:
					break;
				case D3D_SIT_UAV_RWTYPED:
					bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::UAV));
					break;
				case D3D_SIT_STRUCTURED:
				case D3D_SIT_BYTEADDRESS:
					bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::SRV));
					break;
				case D3D_SIT_UAV_RWSTRUCTURED:
				case D3D_SIT_UAV_RWBYTEADDRESS:
				case D3D_SIT_UAV_APPEND_STRUCTURED:
				case D3D_SIT_UAV_CONSUME_STRUCTURED:
				case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
					bindings.emplace(rsc.id_hash, ResourceBinding(rsc.binding_point, BindingType::UAV));
					break;
				default:
					break;
			}
			//TODO
		}

		uint16_t uniforms_size = 0;
		in_stream.read((char*)&uniforms_size, sizeof(uint16_t));
		for (unsigned i = 0; i < uniforms_size; i++)
		{
			ReflectionD3D::UniformInfo info;
			info.Read(in_stream);
			uniforms.emplace(info.id_hash, Uniform(info.slot, info.offset, info.size));
		}

		in_stream.read((char*)&instruction_count, sizeof(uint32_t));

		// Finally, extract the actual bytecode
		uint32_t bytecode_size = 0;
		in_stream.read( ( char* )&bytecode_size, sizeof( uint32_t ) );
		char* compiled_data = new char[ bytecode_size ];
		in_stream.read( compiled_data, bytecode_size );
		bytecode = ShaderData( compiled_data, compiled_data + bytecode_size );
		delete[] compiled_data;

		Finalize();
		return true;
	}

	bool ShaderD3D11::Upload( ShaderType type )
	{
		PROFILE;

		if (bytecode.size() == 0)
			return false;

		if (type == VERTEX_SHADER)
		{
			PROFILE_NAMED("Upload VS");

			ID3D11VertexShader* pVS = nullptr;
			const auto hr = static_cast<IDeviceD3D11*>(pDevice)->GetDevice()->CreateVertexShader(bytecode.data(), bytecode.size(), NULL, &pVS);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateVertexShader", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
			this->pVS.reset(pVS);
		}
		else if (type == PIXEL_SHADER)
		{
			PROFILE_NAMED("Upload PS");

			ID3D11PixelShader* pPS = nullptr;
			const auto hr = static_cast<IDeviceD3D11*>(pDevice)->GetDevice()->CreatePixelShader(bytecode.data(), bytecode.size(), NULL, &pPS);
			if (FAILED(hr))
				throw ExceptionD3D11("CreatePixelShader", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
			this->pPS.reset(pPS);
		}
		else if (type == COMPUTE_SHADER)
		{
			PROFILE_NAMED("Upload CS");

			ID3D11ComputeShader* pCS = nullptr;
			const auto hr = static_cast<IDeviceD3D11*>(pDevice)->GetDevice()->CreateComputeShader(bytecode.data(), bytecode.size(), NULL, &pCS);
			if (FAILED(hr))
				throw ExceptionD3D11("CreateComputeShader", hr, static_cast<IDeviceD3D11*>(pDevice)->GetDevice());
			this->pCS.reset(pCS);
		}

#ifdef _DEBUG
		// We need this for shader input/output signature validation which is enabled on Debug
		ExtractShaderValidationInfo();
#endif
#ifndef _XBOX_ONE
		if(pVS )
			D3D_SET_OBJECT_NAME_A(pVS, GetName().c_str() );

		if(pPS )
			D3D_SET_OBJECT_NAME_A(pPS, GetName().c_str() );

		if (pCS)
			D3D_SET_OBJECT_NAME_A(pCS, GetName().c_str());
#endif

		if (type != VERTEX_SHADER) // vertex shader bytecode is needed to create InputLayouts in CreateInputLayoutD3D11
			bytecode.clear();

		return true;
	}

	bool ShaderD3D11::ValidateShaderCompatibility(Handle<Shader> pixel_shader)
	{
	#ifdef _DEBUG
		// if either the vertex or pixel shader cannot be checked for validation ( i.e. compiled on release ), then skip this check
		if ((input_signature.size() || output_signature.size()) &&
			(static_cast<ShaderD3D11*>(pixel_shader.Get())->input_signature.size() || static_cast<ShaderD3D11*>(pixel_shader.Get())->output_signature.size()))
		{
			auto inputSignature = static_cast<ShaderD3D11*>(pixel_shader.Get())->input_signature;

			for (size_t i = 0, n = inputSignature.size(); i < n; ++i)
			{
				auto in = inputSignature[i];

				// look for an output parameter that matches this input parameter
				bool found = false;
				for (size_t j = 0, m = output_signature.size(); j < m; ++j)
				{
					auto out = output_signature[j];

					if (out.SemanticName == in.SemanticName &&
						out.SemanticIndex == in.SemanticIndex &&
						out.ComponentType == in.ComponentType &&
						in.Mask == out.Mask &&
						out.Register == in.Register)
					{
						found = true;
					}
				}

				if (!found)
					return false;
			}
		}
	#endif
		return true;
	}

	const ShaderType ShaderD3D11::GetType() const
	{
		if (pVS)
			return VERTEX_SHADER;
		else if (pPS)
			return PIXEL_SHADER;
		else if (pCS)
			return COMPUTE_SHADER;

		return NULL_SHADER;
	}

	unsigned ShaderD3D11::GetInstructionCount() const
	{
		return instruction_count;
	}

}
