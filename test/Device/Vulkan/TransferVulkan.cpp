
namespace Device
{

	struct LoadVulkan
	{
		Memory::DebugStringA<> name;

		LoadVulkan(const Memory::DebugStringA<>& name) : name(name) {}
		virtual ~LoadVulkan() {}

		virtual size_t Size() = 0;

		virtual void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) = 0;
		virtual void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) = 0;
		virtual void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) = 0;
	};

	struct CopyStaticDataToBufferLoadVulkan : public LoadVulkan
	{
		vk::Buffer buffer;
		std::shared_ptr<AllocatorVulkan::Data> data;
		const vk::BufferCopy copy;

		CopyStaticDataToBufferLoadVulkan(const Memory::DebugStringA<>& name, const vk::Buffer& buffer, std::shared_ptr<AllocatorVulkan::Data> data, const vk::BufferCopy& copy)
			: LoadVulkan(name), buffer(buffer), data(data), copy(copy) {}

		virtual size_t Size()
		{
			return copy.size;
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::BufferMemoryBarrier()
			.setSrcAccessMask((vk::AccessFlagBits)0)
			.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setBuffer(buffer)
			.setOffset(0)
			.setSize(copy.size);

			transfer_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &barrier, 0, nullptr);
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			transfer_cmd_buf.copyBuffer(data->GetBuffer().get(), buffer, 1, &copy);
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto release_barrier = vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask((vk::AccessFlagBits)0)
				.setSrcQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetTransferQueueFamilyIndex())
				.setDstQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetGraphicsQueueFamilyIndex())
				.setBuffer(buffer)
				.setOffset(0)
				.setSize(copy.size);

			transfer_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &release_barrier, 0, nullptr);

			const auto acquire_barrier = vk::BufferMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setSrcQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetTransferQueueFamilyIndex())
				.setDstQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetGraphicsQueueFamilyIndex())
				.setBuffer(buffer)
				.setOffset(0)
				.setSize(copy.size);

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &acquire_barrier, 0, nullptr);
		}
	};

	struct CopyStaticDataToImageLoadVulkan : public LoadVulkan
	{
		vk::Image image;
		unsigned mip_level;
		unsigned mip_count;
		unsigned array_count;
		std::shared_ptr<AllocatorVulkan::Data> data;
		const BufferImageCopies copies;

		CopyStaticDataToImageLoadVulkan(const Memory::DebugStringA<>& name, const vk::Image& image, unsigned mip_level, unsigned mip_count, unsigned array_count, std::shared_ptr<AllocatorVulkan::Data> data, const BufferImageCopies& copies)
			: LoadVulkan(name), image(image), mip_level(mip_level), mip_count(mip_count), array_count(array_count), data(data), copies(copies) {}

		virtual size_t Size()
		{
			size_t size = 0;
			for (auto& copy : copies)
				size += copy.bufferRowLength * copy.bufferImageHeight;
			return size;
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mip_level, mip_count, 0, array_count));

			transfer_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			transfer_cmd_buf.copyBufferToImage(data->GetBuffer().get(), image, vk::ImageLayout::eTransferDstOptimal, (uint32_t)copies.size(), copies.data());
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto release_barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask((vk::AccessFlagBits)0)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetTransferQueueFamilyIndex())
				.setDstQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetGraphicsQueueFamilyIndex())
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mip_level, mip_count, 0, array_count));

			transfer_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &release_barrier);

			const auto acquire_barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetTransferQueueFamilyIndex())
				.setDstQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetGraphicsQueueFamilyIndex())
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mip_level, mip_count, 0, array_count));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &acquire_barrier);
		}
	};

	struct CopyDynamicDataToBufferLoadVulkan : public LoadVulkan
	{
		vk::Buffer buffer;
		std::shared_ptr<AllocatorVulkan::Data> data;
		const vk::BufferCopy copy;

		CopyDynamicDataToBufferLoadVulkan(const Memory::DebugStringA<>& name, const vk::Buffer& buffer, std::shared_ptr<AllocatorVulkan::Data> data, const vk::BufferCopy& copy)
			: LoadVulkan(name), buffer(buffer), data(data), copy(copy) {}

		virtual size_t Size()
		{
			return copy.size;
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::BufferMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setBuffer(buffer)
				.setOffset(0)
				.setSize(copy.size);

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &barrier, 0, nullptr);
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			graphics_cmd_buf.copyBuffer(data->GetBuffer().get(), buffer, 1, &copy);
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setBuffer(buffer)
				.setOffset(0)
				.setSize(copy.size);

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 1, &barrier, 0, nullptr);
		}
	};

	struct CopyDynamicDataToImageLoadVulkan : public LoadVulkan
	{
		vk::Image image;
		unsigned mip_level;
		unsigned mip_count;
		unsigned array_count;
		std::shared_ptr<AllocatorVulkan::Data> data;
		const BufferImageCopies copies;

		CopyDynamicDataToImageLoadVulkan(const Memory::DebugStringA<>& name, const vk::Image& image, unsigned mip_level, unsigned mip_count, unsigned array_count, std::shared_ptr<AllocatorVulkan::Data> data, const BufferImageCopies& copies)
			: LoadVulkan(name), image(image), mip_level(mip_level), mip_count(mip_count), array_count(array_count), data(data), copies(copies) {}

		virtual size_t Size()
		{
			size_t size = 0;
			for (auto& copy : copies)
				size += copy.bufferRowLength * copy.bufferImageHeight;
			return size;
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mip_level, mip_count, 0, array_count));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			graphics_cmd_buf.copyBufferToImage(data->GetBuffer().get(), image, vk::ImageLayout::eTransferDstOptimal, (uint32_t)copies.size(), copies.data());
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
				.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mip_level, mip_count, 0, array_count));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &barrier);
		}
	};

	struct CopyImageToDataLoadVulkan : public LoadVulkan
	{
		vk::Image image;
		unsigned mip_count;
		unsigned array_count;
		std::shared_ptr<AllocatorVulkan::Data> data;
		const BufferImageCopies copies;
		bool is_backbuffer = false;

		CopyImageToDataLoadVulkan(const Memory::DebugStringA<>& name, const vk::Image& image, unsigned mip_count, unsigned array_count, std::shared_ptr<AllocatorVulkan::Data> data, const BufferImageCopies& copies, bool is_backbuffer)
			: LoadVulkan(name), image(image), mip_count(mip_count), array_count(array_count), data(data), copies(copies), is_backbuffer(is_backbuffer) {}

		virtual size_t Size()
		{
			size_t size = 0;
			for (auto& copy : copies)
				size += copy.bufferRowLength * copy.bufferImageHeight;
			return size;
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eTransferRead)
				.setOldLayout(is_backbuffer ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal)
				.setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mip_count, 0, array_count));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			graphics_cmd_buf.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, data->GetBuffer().get(), (uint32_t)copies.size(), copies.data());
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
				.setNewLayout(is_backbuffer ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, mip_count, 0, array_count));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &barrier);
		}
	};

	struct TransitionImageLoadVulkan : public LoadVulkan
	{
		vk::Image image;
		vk::ImageAspectFlags aspect;
		unsigned mip_count = 0;
		unsigned array_count = 0;
		bool is_backbuffer = false;

		TransitionImageLoadVulkan(const Memory::DebugStringA<>& name, const vk::Image& image, vk::ImageAspectFlags aspect, unsigned mip_count, unsigned array_count, bool is_backbuffer)
			: LoadVulkan(name), image(image), aspect(aspect), mip_count(mip_count), array_count(array_count), is_backbuffer(is_backbuffer) {}

		virtual size_t Size()
		{
			return 0;
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			const auto barrier = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(is_backbuffer ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(image)
				.setSubresourceRange(vk::ImageSubresourceRange(aspect, 0, mip_count, 0, array_count));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &barrier);
		}
	};

	struct CopyImageToImageLoadVulkan : public LoadVulkan
	{
		vk::Image src_image;
		vk::Image dst_image;
		const vk::ImageCopy copy;
		bool is_src_backbuffer = false;

		CopyImageToImageLoadVulkan(const Memory::DebugStringA<>& name, const vk::Image& src_image, const vk::Image& dst_image, const vk::ImageCopy& copy, bool is_src_backbuffer)
			: LoadVulkan(name), src_image(src_image), dst_image(dst_image), copy(copy), is_src_backbuffer(is_src_backbuffer) {}

		virtual size_t Size()
		{
			return 0; // Doesn't matter for now since we flush right away.
		}

		void Pre(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			std::array<vk::ImageMemoryBarrier, 2> barriers;
			barriers[0] = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eTransferRead)
				.setOldLayout(is_src_backbuffer ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal)
				.setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(src_image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			barriers[1] = vk::ImageMemoryBarrier()
				.setSrcAccessMask((vk::AccessFlagBits)0)
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(dst_image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(), barriers.data());
		}

		void Push(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			graphics_cmd_buf.copyImage(src_image, vk::ImageLayout::eTransferSrcOptimal, dst_image, vk::ImageLayout::eTransferDstOptimal, 1, &copy);
		}

		void Post(IDevice* device, vk::CommandBuffer& graphics_cmd_buf, vk::CommandBuffer& transfer_cmd_buf) final
		{
			std::array<vk::ImageMemoryBarrier, 2> barriers;
			barriers[0] = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
				.setNewLayout(is_src_backbuffer ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(src_image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			barriers[1] = vk::ImageMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
				.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(dst_image)
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

			graphics_cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(), barriers.data());
		}
	};


	class GenerationVulkan
	{
		IDevice* device = nullptr;

		vk::UniqueCommandBuffer graphics_cmd_buf;
		vk::UniqueCommandBuffer transfer_cmd_buf;

		vk::UniqueFence transfer_fence;
		vk::UniqueFence graphics_fence;

		vk::UniqueSemaphore transfer_semaphore;

		std::list<std::unique_ptr<LoadVulkan>> submitted_loads;

		void Clear(std::list<std::unique_ptr<LoadVulkan>>& loads)
		{
			PROFILE_NAMED("Clear loads");
			loads.clear();
		}

		void Push(std::list<std::unique_ptr<LoadVulkan>>& loads)
		{
			PROFILE;

			const auto cmd_buf_info = vk::CommandBufferBeginInfo();
			if (auto res = graphics_cmd_buf->begin(&cmd_buf_info); res == vk::Result::eSuccess)
			{
				if (auto res = transfer_cmd_buf->begin(&cmd_buf_info); res == vk::Result::eSuccess)
				{
					for (auto& load : loads) // TODO: Split, and call pipelineBarrier once before, and once after. Need to guarantee that the same dst does not appear twice in the list.
					{
						load->Pre(device, graphics_cmd_buf.get(), transfer_cmd_buf.get());
						load->Push(device, graphics_cmd_buf.get(), transfer_cmd_buf.get());
						load->Post(device, graphics_cmd_buf.get(), transfer_cmd_buf.get());
					}

					transfer_cmd_buf->end();
				}

				graphics_cmd_buf->end();
			}
		}

		void Submit()
		{
			PROFILE;

			{
				Memory::SmallVector<vk::Semaphore, 1, Memory::Tag::Device> signal_semaphores;
				signal_semaphores.emplace_back(transfer_semaphore.get());

				std::array<vk::SubmitInfo, 1> submit_infos;
				submit_infos[0]
					.setSignalSemaphoreCount((uint32_t)signal_semaphores.size())
					.setPSignalSemaphores(signal_semaphores.data())
					.setCommandBufferCount(1)
					.setPCommandBuffers(&transfer_cmd_buf.get());
				static_cast<IDeviceVulkan*>(device)->GetTransferQueue().submit(submit_infos, transfer_fence.get());
			}

			{
				Memory::SmallVector<vk::Semaphore, 1, Memory::Tag::Device> wait_semaphores;
				Memory::SmallVector<vk::PipelineStageFlags, 1, Memory::Tag::Device> pipe_stage_flags;
				wait_semaphores.emplace_back(transfer_semaphore.get());
				pipe_stage_flags.push_back(vk::PipelineStageFlagBits::eAllCommands);

				std::array<vk::SubmitInfo, 1> submit_infos;
				submit_infos[0]
					.setWaitSemaphoreCount((uint32_t)wait_semaphores.size())
					.setPWaitSemaphores(wait_semaphores.data())
					.setPWaitDstStageMask(pipe_stage_flags.data())
					.setCommandBufferCount(1)
					.setPCommandBuffers(&graphics_cmd_buf.get());
				static_cast<IDeviceVulkan*>(device)->GetGraphicsQueue().submit(submit_infos, graphics_fence.get());
			}
		}

	public:
		GenerationVulkan() {}
		GenerationVulkan(IDevice* device, vk::CommandPool& graphics_cmd_pool, vk::CommandPool& transfer_cmd_pool)
			: device(device)
		{
			const auto graphics_cmd_buf_info = vk::CommandBufferAllocateInfo()
				.setCommandPool(graphics_cmd_pool)
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(1);
			graphics_cmd_buf = std::move(static_cast<IDeviceVulkan*>(device)->GetDevice().allocateCommandBuffersUnique(graphics_cmd_buf_info).back());
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)graphics_cmd_buf.get().operator VkCommandBuffer(), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, "Graphics Command Buffer");

			const auto transfer_cmd_buf_info = vk::CommandBufferAllocateInfo()
				.setCommandPool(transfer_cmd_pool)
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(1);
			transfer_cmd_buf = std::move(static_cast<IDeviceVulkan*>(device)->GetDevice().allocateCommandBuffersUnique(transfer_cmd_buf_info).back());
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)transfer_cmd_buf.get().operator VkCommandBuffer(), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, "Transfer Command Buffer");

			transfer_fence = static_cast<IDeviceVulkan*>(device)->GetDevice().createFenceUnique(vk::FenceCreateInfo());
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)transfer_fence.get().operator VkFence(), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, "Transfer Fence");
			graphics_fence = static_cast<IDeviceVulkan*>(device)->GetDevice().createFenceUnique(vk::FenceCreateInfo());
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)graphics_fence.get().operator VkFence(), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, "Graphics Fence");

			transfer_semaphore = static_cast<IDeviceVulkan*>(device)->GetDevice().createSemaphoreUnique(vk::SemaphoreCreateInfo());
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)transfer_semaphore.get().operator VkSemaphore(), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, "Transfer Semaphore");
		}

		void Wait()
		{
			if (submitted_loads.size() > 0)
			{
				PROFILE;
				std::array<vk::Fence, 2> fences = { transfer_fence.get(), graphics_fence.get() };
				if (auto res = static_cast<IDeviceVulkan*>(device)->GetDevice().waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max()); res == vk::Result::eSuccess)
					static_cast<IDeviceVulkan*>(device)->GetDevice().resetFences(fences);

				Clear(submitted_loads);
			}
		}

		void Flush(std::list<std::unique_ptr<LoadVulkan>>&& gathered_loads)
		{
			if (gathered_loads.size() > 0)
			{
				Push(gathered_loads);
				Submit();
				submitted_loads = std::move(gathered_loads);
			}
		}
	};


	class TransferVulkan
	{
		IDevice* device = nullptr;

		vk::UniqueCommandPool graphics_cmd_pool;
		vk::UniqueCommandPool transfer_cmd_pool;

		Memory::SmallVector<GenerationVulkan, 3, Memory::Tag::Device> generations;

		std::list<std::unique_ptr<LoadVulkan>> gathered_loads;

		Memory::Mutex mutex;

	public:
		TransferVulkan() {}
		TransferVulkan(IDevice* device)
			: device(device)
		{
			const auto graphics_cmd_pool_info = vk::CommandPoolCreateInfo()
				.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
				.setQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetGraphicsQueueFamilyIndex());
			graphics_cmd_pool = static_cast<IDeviceVulkan*>(device)->GetDevice().createCommandPoolUnique(graphics_cmd_pool_info);
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)graphics_cmd_pool.get().operator VkCommandPool(), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, "Graphics Command Pool");

			const auto transfer_cmd_pool_info = vk::CommandPoolCreateInfo()
				.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
				.setQueueFamilyIndex(static_cast<IDeviceVulkan*>(device)->GetTransferQueueFamilyIndex());
			transfer_cmd_pool = static_cast<IDeviceVulkan*>(device)->GetDevice().createCommandPoolUnique(transfer_cmd_pool_info);
			static_cast<IDeviceVulkan*>(device)->DebugName((uint64_t)transfer_cmd_pool.get().operator VkCommandPool(), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, "Transfer Command Pool");

			for (unsigned i = 0; i < IDeviceVulkan::FRAME_QUEUE_COUNT; ++i)
				generations.emplace_back(device, graphics_cmd_pool.get(), transfer_cmd_pool.get());
		}

		void Add(std::unique_ptr<LoadVulkan> load)
		{
			PROFILE;
			Memory::Lock lock(mutex);
			gathered_loads.emplace_back(std::move(load));
		}

		void WaitAll()
		{
			PROFILE;
			Memory::Lock lock(mutex);
			for (auto& generation : generations)
				generation.Wait();
		}

		void Wait()
		{
			PROFILE;
			Memory::Lock lock(mutex);
			generations[static_cast<IDeviceVulkan*>(device)->GetBufferIndex()].Wait();
		}

		void Flush()
		{
			PROFILE;
			Memory::Lock lock(mutex);
			generations[static_cast<IDeviceVulkan*>(device)->GetBufferIndex()].Flush(std::move(gathered_loads));
		}

		Memory::Mutex& GetMutex() { return mutex; } 
	};

}
