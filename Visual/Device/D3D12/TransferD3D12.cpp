namespace Device
{
	struct LoadD3D12
	{
		Memory::DebugStringA<> name;

		LoadD3D12(const Memory::DebugStringA<>& name) : name(name) {}
		virtual ~LoadD3D12() {};
		virtual size_t Size() = 0;

		virtual void Pre(CommandBufferD3D12* cmd_list) = 0;
		virtual void Push(CommandBufferD3D12* cmd_list) = 0;
		virtual void Post(CommandBufferD3D12* cmd_list) = 0;
	};

	struct BufferCopyD3D12
	{
		uint64_t src_offset;
		uint64_t dst_offset;
		uint64_t size;
	};

	struct CopyDataToBufferLoadD3D12 : public LoadD3D12
	{
		ComPtr<ID3D12Resource> buffer;
		std::shared_ptr<AllocatorD3D12::Data> data;
		const BufferCopyD3D12 copy;

		CopyDataToBufferLoadD3D12(
			const Memory::DebugStringA<>& name,
			ComPtr<ID3D12Resource> buffer,
			std::shared_ptr<AllocatorD3D12::Data> data,
			const BufferCopyD3D12& copy
		)
			: LoadD3D12(name), copy(copy), buffer(buffer), data(data)
		{}

		~CopyDataToBufferLoadD3D12() = default;

		size_t Size() final
		{
			return copy.size;
		}

		void Pre(CommandBufferD3D12* cmd_list) final
		{
			cmd_list->Transition(ResourceTransitionD3D12{ buffer.Get(), ResourceState::ShaderRead, ResourceState::CopyDst });
		}

		void Push(CommandBufferD3D12* cmd_list) final
		{
			cmd_list->GetCommandList()->CopyBufferRegion(buffer.Get(), copy.dst_offset, data->GetBuffer(), copy.src_offset, copy.size);
		}

		void Post(CommandBufferD3D12* cmd_list) final
		{
			cmd_list->Transition(ResourceTransitionD3D12{ buffer.Get(), ResourceState::CopyDst, ResourceState::ShaderRead });
		}
	};

	struct CopyDataToImageLoadD3D12 : public LoadD3D12
	{
		ComPtr<ID3D12Resource> texture;
		std::shared_ptr<AllocatorD3D12::Data> data;
		const BufferImageCopiesD3D12 copies;

		CopyDataToImageLoadD3D12(const Memory::DebugStringA<>& name, ComPtr<ID3D12Resource> texture, std::shared_ptr<AllocatorD3D12::Data> data, BufferImageCopiesD3D12 copies)
			: LoadD3D12(name), texture(texture), data(data), copies(std::move(copies)) {}

		virtual size_t Size()
		{
			size_t size = 0;
			for (auto& copy : copies)
				size += copy.src.PlacedFootprint.Footprint.RowPitch * copy.src.PlacedFootprint.Footprint.Height;
			return size;
		}

		void Pre(CommandBufferD3D12* cmd_list) final
		{
			cmd_list->Transition(ResourceTransitionD3D12{ texture.Get(), ResourceState::ShaderRead, ResourceState::CopyDst });
		}

		void Push(CommandBufferD3D12* cmd_list) final
		{
			for (auto& copy : copies)
			{
				cmd_list->GetCommandList()->CopyTextureRegion(&copy.dst, 0, 0, 0, &copy.src, nullptr);
			}
		}

		void Post(CommandBufferD3D12* cmd_list) final
		{
			cmd_list->Transition(ResourceTransitionD3D12{ texture.Get(), ResourceState::CopyDst, ResourceState::ShaderRead });
		}
	};

	struct CopyImageToImageLoadD3D12 : public LoadD3D12
	{
		ComPtr<ID3D12Resource> src_texture;
		ComPtr<ID3D12Resource> dst_texture;
		const BufferImageCopiesD3D12 copies;
		const bool src_is_backbuffer = false;

		CopyImageToImageLoadD3D12(const Memory::DebugStringA<>& name, ComPtr<ID3D12Resource> src_texture, ComPtr<ID3D12Resource> dst_texture, BufferImageCopiesD3D12 copies, bool src_is_backbuffer = false)
			: LoadD3D12(name), src_texture(src_texture), dst_texture(dst_texture), copies(std::move(copies)), src_is_backbuffer(src_is_backbuffer) {}

		virtual size_t Size()
		{
			size_t size = 0;
			for (auto& copy : copies)
				size += copy.src.PlacedFootprint.Footprint.RowPitch * copy.src.PlacedFootprint.Footprint.Height;
			return size;
		}

		void Pre(CommandBufferD3D12* cmd_list) final
		{
			const auto src_state = src_is_backbuffer ? ResourceState::Present : ResourceState::ShaderRead;
			const std::array<ResourceTransitionD3D12, 2> transitions = {
				ResourceTransitionD3D12{ src_texture.Get(), src_state, ResourceState::CopySrc },
				ResourceTransitionD3D12{ dst_texture.Get(), ResourceState::ShaderRead, ResourceState::CopyDst }
			};

			cmd_list->Transition(transitions);
		}

		void Push(CommandBufferD3D12* cmd_list) final
		{
			for (auto& copy : copies)
			{
				cmd_list->GetCommandList()->CopyTextureRegion(&copy.dst, 0, 0, 0, &copy.src, nullptr);
			}
		}

		void Post(CommandBufferD3D12* cmd_list) final
		{
			const auto src_state = src_is_backbuffer ? ResourceState::Present : ResourceState::ShaderRead;
			const std::array<ResourceTransitionD3D12, 2> transitions = {
				ResourceTransitionD3D12{ src_texture.Get(), ResourceState::CopySrc, src_state },
				ResourceTransitionD3D12{ dst_texture.Get(), ResourceState::CopyDst, ResourceState::ShaderRead }
			};

			cmd_list->Transition(transitions);
		}
	};

	struct CopyImageToDataLoadD3D12 : public LoadD3D12
	{
		ComPtr<ID3D12Resource> src_texture;
		std::shared_ptr<AllocatorD3D12::Data> dst_data;
		const BufferImageCopiesD3D12 copies;
		const bool src_is_backbuffer = false;

		CopyImageToDataLoadD3D12(const Memory::DebugStringA<>& name, ComPtr<ID3D12Resource> src_texture, std::shared_ptr<AllocatorD3D12::Data> dst_data, BufferImageCopiesD3D12 copies, bool src_is_backbuffer = false)
			: LoadD3D12(name), src_texture(src_texture), dst_data(dst_data), copies(std::move(copies)), src_is_backbuffer(src_is_backbuffer) {}

		virtual size_t Size()
		{
			size_t size = 0;
			for (auto& copy : copies)
				size += copy.src.PlacedFootprint.Footprint.RowPitch * copy.src.PlacedFootprint.Footprint.Height;
			return size;
		}

		void Pre(CommandBufferD3D12* cmd_list) final
		{
			const auto src_state = src_is_backbuffer ? ResourceState::Present : ResourceState::ShaderRead;
			const std::array<ResourceTransitionD3D12, 2> transitions = {
				ResourceTransitionD3D12{ src_texture.Get(), src_state, ResourceState::CopySrc },
				ResourceTransitionD3D12{ dst_data->GetBuffer(), ResourceState::GenericRead, ResourceState::CopyDst }
			};

			cmd_list->Transition(transitions);
		}

		void Push(CommandBufferD3D12* cmd_list) final
		{
			for (auto& copy : copies)
			{
				cmd_list->GetCommandList()->CopyTextureRegion(&copy.dst, 0, 0, 0, &copy.src, nullptr);
			}
		}

		void Post(CommandBufferD3D12* cmd_list) final
		{
			const auto src_state = src_is_backbuffer ? ResourceState::Present : ResourceState::ShaderRead;
			const std::array<ResourceTransitionD3D12, 2> transitions = {
				ResourceTransitionD3D12{ src_texture.Get(), ResourceState::CopySrc, src_state },
				ResourceTransitionD3D12{ dst_data->GetBuffer(), ResourceState::CopyDst, ResourceState::GenericRead }
			};
			
			cmd_list->Transition(transitions);
		}
	};

	class TransferGenerationD3D12 : public NonCopyable
	{
		Handle<CommandBuffer> command_buffer;
		IDeviceD3D12* device = nullptr;
		ComPtr<ID3D12Fence> fence;
		uint64_t fence_value = 1;
		HANDLE fence_event;

		std::list<std::unique_ptr<LoadD3D12>> gathered_loads;
		std::list<std::unique_ptr<LoadD3D12>> submitted_loads;

		CommandBufferD3D12* GetCommandBuffer() const { return static_cast<CommandBufferD3D12*>(command_buffer.Get()); }

		void Clear(std::list<std::unique_ptr<LoadD3D12>>& loads)
		{
			PROFILE_NAMED("Clear loads");
			loads.clear();
		}

		void Push(std::list<std::unique_ptr<LoadD3D12>>& loads)
		{
			PROFILE;
			
			auto* cmd_buffer = GetCommandBuffer();
			cmd_buffer->Begin();

			for (auto& load : loads)
			{
				load->Pre(cmd_buffer);
				load->Push(cmd_buffer);
				load->Post(cmd_buffer);
			}
			
			cmd_buffer->End();
		}

		void Submit(ID3D12CommandQueue* queue)
		{
			PROFILE;

			ID3D12CommandList* command_lists[] = { GetCommandBuffer()->GetCommandList() };
			queue->ExecuteCommandLists(1, command_lists);
			queue->Signal(fence.Get(), fence_value);
			fence_value += 1;
		}

	public:
		TransferGenerationD3D12(IDeviceD3D12* device)
			: device(device)
		{
			command_buffer = CommandBuffer::Create("Load Command List", device);

			if (FAILED(device->GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(fence.GetAddressOf()))))
				throw ExceptionD3D12("TransferGenerationD3D12::TransferGenerationD3D12() CreateFence failed");
			IDeviceD3D12::DebugName(fence, "Load Fence");

			fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fence_event == nullptr)
				throw ExceptionD3D12("TransferGenerationD3D12::TransferGenerationD3D12() CreateEvent failed");
		}

		~TransferGenerationD3D12()
		{
			CloseHandle(fence_event);
		}

		void Add(ID3D12CommandQueue* queue, std::unique_ptr<LoadD3D12> load)
		{
			gathered_loads.emplace_back(std::move(load));
		}

		void Wait()
		{
			if (submitted_loads.size() > 0)
			{
				PROFILE;

				const auto previous_fence_value = fence_value - 1;

				if (fence->GetCompletedValue() < previous_fence_value)
				{
					if (FAILED(fence->SetEventOnCompletion(previous_fence_value, fence_event)))
						throw ExceptionD3D12("fence->SetEventOnCompletion failed");
					WaitForSingleObject(fence_event, INFINITE);
				}

				Clear(submitted_loads);
			}
		}

		bool Flush(ID3D12CommandQueue* queue)
		{
			Wait();

			if (gathered_loads.size() > 0)
			{
				Push(gathered_loads);
				Submit(queue);
				submitted_loads = std::move(gathered_loads);
			}

			return submitted_loads.size() > 0;
		}

	};

	class TransferD3D12 : public NonCopyable
	{
		IDeviceD3D12* device = nullptr;
		ID3D12CommandQueue* transfer_queue;

		Memory::SmallVector<std::unique_ptr<TransferGenerationD3D12>, 3, Memory::Tag::Device> generations;
		unsigned current = 0;

		Memory::Mutex mutex;

	public:
		TransferD3D12(IDeviceD3D12* device, ID3D12CommandQueue* transfer_queue)
			: device(device)
			, transfer_queue(transfer_queue)
		{
			for (unsigned i = 0; i < IDeviceD3D12::FRAME_QUEUE_COUNT; ++i)
				generations.emplace_back(std::make_unique<TransferGenerationD3D12>(device));
		}

		void Add(std::unique_ptr<LoadD3D12> load)
		{
			PROFILE;
			Memory::Lock lock(mutex);
			generations[current]->Add(transfer_queue, std::move(load));
		}

		void Flush()
		{
			PROFILE;
			Memory::Lock lock(mutex);
			if (generations[current]->Flush(transfer_queue))
				current = (current + 1) % generations.size();
		}

		void Wait()
		{
			PROFILE;
			Memory::Lock lock(mutex);
			for (auto& generation : generations)
				generation->Wait();
		}

		Memory::Mutex& GetMutex() { return mutex; }
	};
}