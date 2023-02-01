
namespace Device
{

	char SafeChar(const std::string& semantic, unsigned index)
	{
		return index < semantic.size() ? semantic[index] : ' ';
	}

	std::pair<DeclUsage, unsigned> ParseSemantic(const std::string& semantic_combined)
	{
		size_t pos;
		for (pos = semantic_combined.size(); pos != 0; --pos)
			if (!isdigit(semantic_combined[pos - 1]))
				break;

		const std::string semantic(semantic_combined, 0, pos);
		const unsigned index = std::atoi((semantic_combined.substr(pos)).c_str());
		return {  UnconvertDeclUsageVulkan(semantic.c_str()), index };
	}


	uint32_t ReadU32(const char*& in)
	{
		uint32_t u = *(uint32_t*)in;
		in += sizeof(uint32_t);
		return u;
	}

	template <typename T>
	void ReadBlob(const char*& in, size_t count, Memory::Vector<T, Memory::Tag::ShaderBytecode>& out)
	{
		out.insert(out.end(), (T*)in, (T*)in + count);
		in += count * sizeof(T);
	}

	std::string ReadString(const char*& in)
	{
		const auto length = ReadU32(in);
		std::string s(in, length);
		in += length;
		return s;
	}


	struct ShaderBuiltin
	{
		std::string semantic;
		std::string type;
		
		void Read(const char*& in)
		{
			semantic = ReadString(in);
			type = ReadString(in);
		}
	};

	struct ShaderResource
	{
		uint32_t original_binding = 0;
		uint32_t binding = 0;

		void Read(const char*& in)
		{
			original_binding = ReadU32(in);
			binding = ReadU32(in);
		}
	};

	struct ShaderInput : public ShaderResource
	{
		std::string semantic;
		unsigned location = 0;
		std::pair<DeclUsage, unsigned> usage = { DeclUsage::UNUSED, 0 };
		
		void Read(const char*& in)
		{
			ShaderResource::Read(in);
			semantic = ReadString(in);
			location = ReadU32(in);
			usage = ParseSemantic(semantic);
		}
	};

	struct ShaderOutput : public ShaderResource
	{
		std::string semantic;
		std::string type;
		unsigned location = 0;
		std::pair<DeclUsage, unsigned> usage = { DeclUsage::UNUSED, 0 };
		
		void Read(const char*& in)
		{
			ShaderResource::Read(in);
			semantic = ReadString(in);
			type = ReadString(in);
			location = ReadU32(in);
			usage = ParseSemantic(semantic);
		}
	};
	
	struct ShaderUniform
	{
		std::string name;
		size_t size = 0;
		off_t offset = 0;
		unsigned buffer_index = 0;

		std::string Read(const char*& in)
		{
			name = ReadString(in);
			size = ReadU32(in);
			offset = ReadU32(in);
			return name;
		}
	};

	struct ShaderUniformBuffer : public ShaderResource
	{
		std::string name;
		size_t size = 0;

		void Read(const char*& in, Memory::FlatMap<std::string, ShaderUniform, Memory::Tag::ShaderMetadata>& shader_uniforms)
		{
			ShaderResource::Read(in);
			name = ReadString(in);
			size = ReadU32(in);

			const auto member_count = ReadU32(in);
			for (unsigned j = 0; j < member_count; ++j)
			{
				ShaderUniform uniform;
				const auto member_name = uniform.Read(in);
				uniform.buffer_index = original_binding;
				shader_uniforms[member_name] = uniform;
			}
		}
	};

	struct ShaderSampler : public ShaderResource
	{
		std::string name;

		void Read(const char*& in)
		{
			ShaderResource::Read(in);
			name = ReadString(in);
		}
	};

	struct ShaderImage : public ShaderResource
	{
		std::string name;
		ImageDimension dimension;

		void Read(const char*& in)
		{
			ShaderResource::Read(in);
			name = ReadString(in);
			dimension = (ImageDimension)ReadU32(in);
		}
	};

	struct ShaderStorageBuffer : public ShaderResource
	{
		std::string name;

		void Read(const char*& in)
		{
			ShaderResource::Read(in);
			name = ReadString(in);
		}
	};

	struct ShaderStorageImage : public ShaderResource
	{
		std::string name;
		ImageDimension dimension;

		void Read(const char*& in)
		{
			ShaderResource::Read(in);
			name = ReadString(in);
			dimension = (ImageDimension)ReadU32(in);
		}
	};


	vk::ShaderStageFlagBits ConvertShaderType(ShaderType type)
	{
		switch (type)
		{
		case ShaderType::VERTEX_SHADER: return vk::ShaderStageFlagBits::eVertex;
		case ShaderType::PIXEL_SHADER: return vk::ShaderStageFlagBits::eFragment;
		case ShaderType::COMPUTE_SHADER: return vk::ShaderStageFlagBits::eCompute;
		default: throw ExceptionVulkan("Unsupported shader type");
		}
	}

	const vk::ImageView& GetDefaultImageView(IDevice* device, ImageDimension dimension)
	{
		switch (dimension)
		{
		case ImageDimension::_3D: return static_cast<TextureVulkan*>(device->GetDefaultVolumeTexture().Get())->GetImageView().get();
		case ImageDimension::Cube: return static_cast<TextureVulkan*>(device->GetDefaultCubeTexture().Get())->GetImageView().get();
		default: return static_cast<TextureVulkan*>(device->GetDefaultTexture().Get())->GetImageView().get(); // TODO: Actual 1D texture.
		}
	}


	class ShaderVulkan : public Shader
	{
		IDevice* device = nullptr;

		std::string main;
		ShaderType type = ShaderType::NULL_SHADER;

		Memory::Vector<ShaderBuiltin, Memory::Tag::ShaderMetadata> shader_builtins;
		Memory::Vector<ShaderInput, Memory::Tag::ShaderMetadata> shader_inputs;
		Memory::Vector<ShaderOutput, Memory::Tag::ShaderMetadata> shader_outputs;
		Memory::Vector<ShaderUniformBuffer, Memory::Tag::ShaderMetadata> shader_uniform_buffers;
		Memory::Vector<ShaderSampler, Memory::Tag::ShaderMetadata> shader_samplers;
		Memory::Vector<ShaderImage, Memory::Tag::ShaderMetadata> shader_images;
		Memory::Vector<ShaderStorageBuffer, Memory::Tag::ShaderMetadata> shader_storage_buffers;
		Memory::Vector<ShaderStorageImage, Memory::Tag::ShaderMetadata> shader_storage_images;

		Memory::Vector<char, Memory::Tag::ShaderBytecode> bytecode;

		typedef Memory::SmallVector<vk::DescriptorSetLayoutBinding, 32, Memory::Tag::Device> Bindings;
		Bindings layout_bindings;

		vk::UniqueShaderModule module;
		vk::PipelineShaderStageCreateInfo stage_info;
		
		void Read(const char* in);
		
		void GatherBindings();

	public:
		ShaderVulkan(const Memory::DebugStringA<>& name, IDevice* device);

		virtual bool Load(const ShaderData& _bytecode) final;
		virtual bool Upload(ShaderType type) final;

		virtual bool ValidateShaderCompatibility(Handle<Shader> pixel_shader) final;

		virtual const ShaderType GetType() const final { return type; }

		virtual unsigned GetInstructionCount() const final;

		const auto& GetShaderInputs() const { return shader_inputs; }
		const auto& GetShaderOutputs() const { return shader_outputs; }

		struct Sizes
		{
			unsigned uniform_buffer_count = 0;
			unsigned uniform_buffer_dynamic_count = 0;
			unsigned uniform_texel_buffer_count = 0;
			unsigned sampler_count = 0;
			unsigned sampled_image_count = 0;
			unsigned storage_buffer_count = 0;
			unsigned storage_texel_buffer_count = 0;
			unsigned storage_images_count = 0;
		};
		void UpdateSizes(Sizes& sizes) const;

		void UpdateWrites(unsigned backbuffer_index,
			const vk::DescriptorSet& desc_set,
			const ImageViewSlots& image_views,
			const BufferViewSlots& buffer_views,
			const BufferSlots& buffers,
			DependentBuffers& dependent_buffers,
			DependentImages& dependent_images);

		unsigned GetSemanticLocation(std::pair<DeclUsage, unsigned> usage);

		const Bindings& GetBindings() const { return layout_bindings; }

		const vk::PipelineShaderStageCreateInfo& GetStageInfo() const { return stage_info; }
	};

	ShaderVulkan::ShaderVulkan(const Memory::DebugStringA<>& name, IDevice* device)
		: Shader(name)
		,  device(device)
	{}

	bool ShaderVulkan::Load(const ShaderData& bytecode)
	{
		if (bytecode.size() < 4)
			return false;

		const char* data = (char*)bytecode.data();

		if (!Compiler::CheckVersion<Target::Vulkan>::Check(data))
			return false;

		Read(data);

		return true;
	}

	void ShaderVulkan::Read(const char* in)
	{
		main = ReadString(in);

		workgroup.x = ReadU32(in);
		workgroup.y = ReadU32(in);
		workgroup.z = ReadU32(in);

		const auto builtin_count = ReadU32(in);
		for (unsigned i = 0; i < builtin_count; ++i)
		{
			ShaderBuiltin builtin;
			builtin.Read(in);
			shader_builtins.push_back(builtin);
		}

		const auto input_count = ReadU32(in);
		for (unsigned i = 0; i < input_count; ++i)
		{
			ShaderInput input;
			input.Read(in);
			shader_inputs.push_back(input);
		}
		
		const auto output_count = ReadU32(in);
		for (unsigned i = 0; i < output_count; ++i)
		{
			ShaderOutput output;
			output.Read(in);
			shader_outputs.push_back(output);
		}

		Memory::FlatMap<std::string, ShaderUniform, Memory::Tag::ShaderMetadata> shader_uniforms;
		const auto uniform_buffer_count = ReadU32(in);
		for (unsigned i = 0; i < uniform_buffer_count; ++i)
		{
			ShaderUniformBuffer uniform_buffer;
			uniform_buffer.Read(in, shader_uniforms);
			shader_uniform_buffers.push_back(uniform_buffer);
		}
		
		const auto sampler_count = ReadU32(in);
		for (unsigned i = 0; i < sampler_count; ++i)
		{
			ShaderSampler sampler;
			sampler.Read(in);
			shader_samplers.push_back(sampler);
		}
		
		const auto image_count = ReadU32(in);
		for (unsigned i = 0; i < image_count; ++i)
		{
			ShaderImage image;
			image.Read(in);
			shader_images.push_back(image);
		}

		const auto storage_buffer_count = ReadU32(in);
		for (unsigned i = 0; i < storage_buffer_count; ++i)
		{
			ShaderStorageBuffer buffer;
			buffer.Read(in);
			shader_storage_buffers.push_back(buffer);
		}

		const auto storage_image_count = ReadU32(in);
		for (unsigned i = 0; i < storage_image_count; ++i)
		{
			ShaderStorageImage image;
			image.Read(in);
			shader_storage_images.push_back(image);
		}

		const auto bytecode_size = ReadU32(in);
		ReadBlob(in, bytecode_size, bytecode);

	#if 0//defined(__APPLE__iphoneos) // Temporarily disabled until post-process shaders are compiled on testing_mobile.
		const auto msl_size = ReadU32(in);
		if (msl_size > 0)
		{
			const auto msl_main = ReadString(in);
			main = msl_main; // 'main' is forbidden in MSL, so spirv-cross renames it to 'main0' for instance.

			static_assert(sizeof(MVKMSLSPIRVHeader) == sizeof(uint32_t));
			bytecode.clear();
			bytecode.reserve(sizeof(MVKMSLSPIRVHeader) + msl_size);
			bytecode.resize(sizeof(MVKMSLSPIRVHeader));
			*(uint32_t*)bytecode.data() = (uint32_t)MVKMSLMagicNumber::kMVKMagicNumberMSLSourceCode;
			ReadBlob(in, msl_size, bytecode);
			assert(bytecode[sizeof(MVKMSLSPIRVHeader) + msl_size - 1] == 0);
		}
	#endif

		for (auto& builtin : shader_builtins)
		{
			auto parameter = std::make_shared<ShaderParameter>();
			parameter->semantic = builtin.semantic;
			parameter->name = parameter->semantic;
			parameter->type = builtin.type;
			output_variables[parameter->semantic] = parameter;
		}

		for (auto& output : shader_outputs)
		{
			auto parameter = std::make_shared<ShaderParameter>();
			parameter->semantic = output.semantic;
			parameter->name = parameter->semantic;
			parameter->type = output.type;
			output_variables[parameter->semantic] = parameter;
		}

		uint32_t id = 0; uint8_t index = 0;

		for (auto& buffer : shader_uniform_buffers)
		{		
			std::tie(id, index) = Device::ExtractIdIndex(buffer.name.c_str());
			buffers.emplace_back(Device::IdHash(id, index), buffer.original_binding, buffer.size);
		}

		for (auto& uniform : shader_uniforms)
		{
			std::tie(id, index) = Device::ExtractIdIndex(uniform.first.c_str());
			uniforms.emplace(Device::IdHash(id, index), Uniform(uniform.second.buffer_index, uniform.second.offset, uniform.second.size, uniform.second.name));
		}

		for (auto& image : shader_images)
		{
			std::tie(id, index) = Device::ExtractIdIndex(image.name.c_str());
			bindings.emplace(Device::IdHash(id, index), ResourceBinding(image.original_binding, BindingType::Texture));
		}

		for (auto& image : shader_storage_images)
		{
			std::tie(id, index) = Device::ExtractIdIndex(image.name.c_str());
			bindings.emplace(Device::IdHash(id, index), ResourceBinding(image.original_binding, BindingType::Texture));
		}

		for (auto& buffer : shader_storage_buffers)
		{
			std::tie(id, index) = Device::ExtractIdIndex(buffer.name.c_str());
			bindings.emplace(Device::IdHash(id, index), ResourceBinding(buffer.original_binding, BindingType::Buffer));
		}

		Finalize();
	}

	bool ShaderVulkan::Upload(ShaderType type)
	{
		if (bytecode.size() == 0)
			return false;

		PROFILE;

		this->type = type;

		GatherBindings();

		try
		{
			const auto module_info = vk::ShaderModuleCreateInfo()
			.setCodeSize(bytecode.size())
			.setPCode((uint32_t*)bytecode.data());
			module = static_cast<IDeviceVulkan*>(device)->GetDevice().createShaderModuleUnique(module_info);
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)module.get().operator VkShaderModule(), VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, Memory::DebugStringA<>("Shader Module (") + GetName() + ")");
		}
		catch (vk::Error& e)
		{
			LOG_CRIT(L"[VULKAN] Shader module creation failed. Error: " << e.what());
			return false;
		}

		stage_info
			.setStage(ConvertShaderType(type))
			.setModule(module.get())
			.setPName(main.c_str());

		bytecode.clear();

		return true;
	}

	void ShaderVulkan::GatherBindings()
	{
		layout_bindings.clear();

		for (auto& uniform_buffer : shader_uniform_buffers)
		{
			const auto binding = vk::DescriptorSetLayoutBinding()
				.setBinding(uniform_buffer.binding)
				.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
				.setDescriptorCount(1)
				.setStageFlags(ConvertShaderType(type));
			layout_bindings.push_back(binding);
		}

		for (auto& sampler : shader_samplers)
		{
			const auto binding = vk::DescriptorSetLayoutBinding()
				.setBinding(sampler.binding)
				.setDescriptorType(vk::DescriptorType::eSampler)
				.setDescriptorCount(1)
				.setStageFlags(ConvertShaderType(type));
			layout_bindings.push_back(binding);
		}

		for (auto& image : shader_images)
		{
			const auto binding = vk::DescriptorSetLayoutBinding()
				.setBinding(image.binding)
				.setDescriptorType(image.dimension == ImageDimension::Buffer ? vk::DescriptorType::eUniformTexelBuffer : vk::DescriptorType::eSampledImage)
				.setDescriptorCount(1)
				.setStageFlags(ConvertShaderType(type));
			layout_bindings.push_back(binding);
		}

		for (auto& buffer : shader_storage_buffers)
		{
			const auto binding = vk::DescriptorSetLayoutBinding()
				.setBinding(buffer.binding)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setStageFlags(ConvertShaderType(type));
			layout_bindings.push_back(binding);
		}

		for (auto& image : shader_storage_images)
		{
			const auto binding = vk::DescriptorSetLayoutBinding()
				.setBinding(image.binding)
				.setDescriptorType(image.dimension == ImageDimension::Buffer ? vk::DescriptorType::eStorageTexelBuffer : vk::DescriptorType::eStorageImage)
				.setDescriptorCount(1)
				.setStageFlags(ConvertShaderType(type));
			layout_bindings.push_back(binding);
		}
	}

	void ShaderVulkan::UpdateSizes(Sizes& sizes) const
	{
		for (auto& uniform_buffer : shader_uniform_buffers)
		{
			sizes.uniform_buffer_dynamic_count++;
		}

		for (auto& sampler : shader_samplers)
		{
			sizes.sampler_count++;
		}

		for (auto& image : shader_images)
		{
			if (image.dimension == ImageDimension::Buffer)
			{
				sizes.uniform_texel_buffer_count++;
			}
			else
			{
				sizes.sampled_image_count++;
			}
		}

		for (auto& buffer : shader_storage_buffers)
		{
			sizes.storage_buffer_count++;
		}

		for (auto& image : shader_storage_images)
		{
			if (image.dimension == ImageDimension::Buffer)
			{
				sizes.storage_texel_buffer_count++;
			}
			else
			{
				sizes.storage_images_count++;
			}
		}
	}

	void ShaderVulkan::UpdateWrites(unsigned backbuffer_index,
		const vk::DescriptorSet& desc_set,
		const ImageViewSlots& image_views,
		const BufferViewSlots& buffer_views,
		const BufferSlots& buffers,
		DependentBuffers& dependent_buffers,
		DependentImages& dependent_images)
	{
		Memory::SmallVector<vk::WriteDescriptorSet, 32, Memory::Tag::Device> writes;

		std::array<vk::DescriptorBufferInfo, UniformBufferSlotCount + TextureSlotCount> buffer_infos;
		std::array<vk::DescriptorImageInfo, SamplerSlotCount + TextureSlotCount + TextureSlotCount> image_infos;

		unsigned buffer_count = 0;
		unsigned image_count = 0;

		for (auto& uniform_buffer : shader_uniform_buffers)
		{
			const auto& constant_buffer = static_cast<IDeviceVulkan*>(device)->GetConstantBuffer().GetBuffer(backbuffer_index).get();

			buffer_infos[buffer_count] = vk::DescriptorBufferInfo()
				.setBuffer(constant_buffer)
				.setOffset(0)
				.setRange(uniform_buffer.size);

			const auto write = vk::WriteDescriptorSet()
				.setDstSet(desc_set)
				.setDstBinding(uniform_buffer.binding)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
				.setPBufferInfo(&buffer_infos[buffer_count]);
			writes.push_back(write);

			buffer_count++;
		}

		for (auto& buffer : shader_storage_buffers)
		{
			if (!buffers[buffer.original_binding].Buffer)
				throw ExceptionVulkan("Buffer '" + buffer.name + "' is not bound to descriptor set input");

			buffer_infos[buffer_count] = vk::DescriptorBufferInfo()
				.setBuffer(buffers[buffer.original_binding].Buffer)
				.setOffset(buffers[buffer.original_binding].Offset)
				.setRange(buffers[buffer.original_binding].Size);

			dependent_buffers[buffer.original_binding].RequireWriteAccess = true;

			const auto write = vk::WriteDescriptorSet()
				.setDstSet(desc_set)
				.setDstBinding(buffer.binding)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setPBufferInfo(&buffer_infos[buffer_count]);
			writes.push_back(write);

			buffer_count++;
		}

		for (auto& image : shader_storage_images)
		{
			if (image.dimension == ImageDimension::Buffer)
			{
				if(!buffer_views[image.original_binding])
					throw ExceptionVulkan("Texel Buffer '" + image.name + "' is not bound to descriptor set input");

				dependent_buffers[image.original_binding].RequireWriteAccess = true;

				const auto write = vk::WriteDescriptorSet()
					.setDstSet(desc_set)
					.setDstBinding(image.binding)
					.setDstArrayElement(0)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eStorageTexelBuffer)
					.setPTexelBufferView(&buffer_views[image.original_binding]);
				writes.push_back(write);
			}
			else
			{
				if (!image_views[image.original_binding])
					throw ExceptionVulkan("Texture '" + image.name + "' is not bound to descriptor set input");

				dependent_images[image.original_binding].RequireWriteAccess = true;

				image_infos[image_count] = vk::DescriptorImageInfo()
					.setImageView(image_views[image.original_binding]) // No fall back to default textures, because we require write access to the texture
					.setImageLayout(vk::ImageLayout::eGeneral); //TODO: Is General the best layout here?

				const auto write = vk::WriteDescriptorSet()
					.setDstSet(desc_set)
					.setDstBinding(image.binding)
					.setDstArrayElement(0)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setPImageInfo(&image_infos[image_count]);
				writes.push_back(write);

				image_count++;
			}
		}

		for (auto& sampler : shader_samplers)
		{
			image_infos[image_count] = vk::DescriptorImageInfo()
				.setSampler(static_cast<SamplerVulkan*>(device->GetSamplers()[sampler.original_binding].Get())->sampler.get());

			const auto write = vk::WriteDescriptorSet()
				.setDstSet(desc_set)
				.setDstBinding(sampler.binding)
				.setDstArrayElement(0)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eSampler)
				.setPImageInfo(&image_infos[image_count]);
			writes.push_back(write);

			image_count++;
		}

		for (auto& image : shader_images)
		{
			if (image.dimension == ImageDimension::Buffer)
			{
				if (!buffer_views[image.original_binding])
					throw ExceptionVulkan("Texel Buffer '" + image.name + "' is not bound to descriptor set input");

				dependent_buffers[image.original_binding].RequireWriteAccess = false;

				const auto write = vk::WriteDescriptorSet()
					.setDstSet(desc_set)
					.setDstBinding(image.binding)
					.setDstArrayElement(0)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eUniformTexelBuffer)
					.setPTexelBufferView(&buffer_views[image.original_binding]);
				writes.push_back(write);
			}
			else
			{
				dependent_images[image.original_binding].RequireWriteAccess = false;

				image_infos[image_count]  = vk::DescriptorImageInfo()
					.setImageView(image_views[image.original_binding] ? image_views[image.original_binding] : GetDefaultImageView(device, image.dimension))
					.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				
				const auto write = vk::WriteDescriptorSet()
					.setDstSet(desc_set)
					.setDstBinding(image.binding)
					.setDstArrayElement(0)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eSampledImage)
					.setPImageInfo(&image_infos[image_count] );
				writes.push_back(write);

				image_count++;
			}
		}

		static_cast<IDeviceVulkan*>(device)->GetDevice().updateDescriptorSets((uint32_t)writes.size(), writes.data(), 0, nullptr);
	}

	unsigned ShaderVulkan::GetSemanticLocation(std::pair<DeclUsage, unsigned> usage) // TODO: Remove.
	{
		for (unsigned i = 0; i < shader_inputs.size(); ++i)
		{
			if (shader_inputs[i].usage == usage)
				return shader_inputs[i].location;
		}
		return (unsigned)-1;
	}

	bool ShaderVulkan::ValidateShaderCompatibility(Handle<Shader> pixel_shader)
	{
		for (auto& input : static_cast<ShaderVulkan*>(pixel_shader.Get())->shader_inputs)
		{
			bool found = false;

			for (auto& output : shader_outputs)
			{
				if (output.location == input.location)
				{
					assert(output.semantic == input.semantic);
					assert(output.usage == input.usage);
					found = true;
				}
			}

			if (!found)
				return false;
		}

		return true;
	}

	unsigned ShaderVulkan::GetInstructionCount() const
	{
		return 0;
	}

}
