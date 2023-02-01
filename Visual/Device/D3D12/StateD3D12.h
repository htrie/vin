#pragma once

namespace Device
{
	class IDeviceD3D12;

	struct BufferImageCopyD3D12
	{
		CD3DX12_TEXTURE_COPY_LOCATION src;
		CD3DX12_TEXTURE_COPY_LOCATION dst;
	};

	typedef Memory::SmallVector<BufferImageCopyD3D12, 64, Memory::Tag::Device> BufferImageCopiesD3D12;

	// States and transitions D3D12 only for now
	enum class ResourceState
	{
		Unknown = 0,

		// Read states
		CopySrc = 1 << 0,
		Present = 1 << 1,
		ShaderRead = 1 << 4,
		DepthRead = 1 << 8,
		GenericRead = 1 << 15,

		// Read-write states
		CopyDst = 1 << 2,
		UAV = 1 << 10,
		RenderTarget = 1 << 11,
		DepthWrite = 1 << 14,

		// Combined
		ShaderReadMask = ShaderRead | DepthRead
	};

	struct SubresourceRange
	{
		static constexpr uint32_t AllSubresources = (uint32_t)-1;

		uint32_t mip;
		uint32_t array_slice;

		SubresourceRange(uint32_t mip = AllSubresources, uint32_t array_slice = AllSubresources) : mip(mip), array_slice(array_slice) {}
	};

	class ResourceTransition
	{
	public:
		ResourceTransition(RenderTarget& render_target, ResourceState prev_state, ResourceState next_state);
		ResourceTransition(Texture& texture, ResourceState prev_state, ResourceState next_state, SubresourceRange range = SubresourceRange());
		ResourceTransition(StructuredBuffer& buffer, ResourceState prev_state, ResourceState next_state);
		ResourceTransition(ByteAddressBuffer& buffer, ResourceState prev_state, ResourceState next_state);
		ResourceTransition(TexelBuffer& buffer, ResourceState prev_state, ResourceState next_state);

		const SubresourceRange& GetRange() const { return range; }
		ResourceState GetPrevState() const { return prev_state; }
		ResourceState GetNextState() const { return next_state; }
		const auto& GetResource() const { return resource; }
		const RenderTarget* GetRenderTarget() const { return resource.index() == 0 ? std::get<0>(resource) : nullptr; }
		const Texture* GetTexture() const { return resource.index() == 1 ? std::get<1>(resource) : nullptr; }
		const StructuredBuffer* GetStructuredBuffer() const { return resource.index() == 2 ? std::get<2>(resource) : nullptr; }
		const ByteAddressBuffer* GetByteAddressBuffer() const { return resource.index() == 3 ? std::get<3>(resource) : nullptr; }
		const TexelBuffer* GetTexelBuffer() const { return resource.index() == 4 ? std::get<4>(resource) : nullptr; }

	private:
		// Need to store render target here because there is no way of knowing what texture resource was referenced by
		// a render target in the swapchain.
		typedef std::variant<RenderTarget*, Texture*, StructuredBuffer*, ByteAddressBuffer*, TexelBuffer*> ResourceType;

		ResourceTransition(ResourceType resource, ResourceState prev_state, ResourceState next_state);

		SubresourceRange range;
		ResourceState prev_state;
		ResourceState next_state;
		ResourceType resource;
	};

	struct ResourceTransitionD3D12
	{
		ID3D12Resource* resource = nullptr;
		ResourceState state_from = ResourceState::Unknown;
		ResourceState state_to = ResourceState::Unknown;
		uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	};

	struct ClearD3D12
	{
		DirectX::PackedVector::XMCOLOR color;
		UINT flags;
		float z;
		bool clear_color;
		bool clear_depth_stencil;
	};

	struct DependentBufferD3D12 {
		bool RequireWriteAccess = false;

		DependentBufferD3D12(bool write_access = false) : RequireWriteAccess(write_access) {}
	};

	struct DependentImageD3D12 {
		bool RequireWriteAccess = false;

		DependentImageD3D12(bool write_access = false) : RequireWriteAccess(write_access) {}
	};

	class PassD3D12 : public Pass
	{
		ClearD3D12 clear;

	public:
		PassD3D12(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float clear_z);

		const auto& GetClear() const { return clear; }
	};

	D3D12_RESOURCE_STATES ConvertResourceStateD3D12(const ResourceState state);

	class RootSignatureD3D12;

	class PipelineD3D12 : public Pipeline
	{
		const Pass* pass = nullptr;
		D3D12_PRIMITIVE_TOPOLOGY primitive_type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		RootSignatureD3D12* root_signature = nullptr;
		ComPtr<ID3D12PipelineState> pipeline_state;
		bool stencil_enabled = false;
		uint32_t stencil_ref = 0;

		void Create(IDeviceD3D12* device, const Pass* pass, PrimitiveType primitive_type, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		void Create(const Memory::DebugStringA<>& name, IDeviceD3D12* device, Shader* compute_shader);

	public:
		PipelineD3D12(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		PipelineD3D12(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader);
		~PipelineD3D12();

		virtual bool IsValid() const { return !!pipeline_state; }

		const Pass* GetPass() const { return pass; }

		bool GetStencilEnabled() const { return stencil_enabled; }
		uint32_t GetStencilRef() const { return stencil_ref; }

		D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveType() const { return primitive_type; }
		RootSignatureD3D12* GetRootSignature() const { return root_signature; };
		ID3D12PipelineState* GetPipelineState() const { return pipeline_state.Get(); }
	};

	struct UniformOffsetsD3D12 : public UniformOffsets
	{
		static Offset Allocate(IDevice* device, size_t size);
	};

	class DescriptorSetD3D12: public DescriptorSet
	{
		struct FrameDescriptors
		{
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_start;
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_start;
			uint64_t frame_index = -1;
		};

		mutable FrameDescriptors frame_descriptors;

		Memory::SpinMutex mutex;
		Memory::SmallVector<ID3D12Resource*, 8, Memory::Tag::Device> uav_textures;
		Memory::SmallVector<ID3D12Resource*, 8, Memory::Tag::Device> uav_buffers;
		Memory::Vector<D3D12_CPU_DESCRIPTOR_HANDLE, Memory::Tag::Device> copy_descriptors; // descriptors used as copy src every frame

	public:

		DescriptorSetD3D12(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets);

		const auto& GetUAVTextures() const { return uav_textures; }
		const auto& GetUAVBuffers() const { return uav_buffers; }
		uint32_t GetDescriptorCount() const { return (uint32_t)copy_descriptors.size(); }

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetDescriptorsGPUAddress(IDeviceD3D12* device) const;
	};

	class CommandBufferD3D12 : public CommandBuffer
	{
		IDeviceD3D12* device = nullptr;
		PassD3D12* current_pass = nullptr;
		bool is_in_compute_pass = false;

		Memory::SmallVector<ComPtr<ID3D12CommandAllocator>, 3, Memory::Tag::Device> cmd_allocators;
		ComPtr<ID3D12GraphicsCommandList> cmd_list;

		Memory::Array<D3D12_RESOURCE_BARRIER, 16> pending_barriers;
		Memory::VectorSet<ID3D12Resource*, Memory::Tag::Device> pass_uav_textures;
		Memory::VectorSet<ID3D12Resource*, Memory::Tag::Device> pass_uav_buffers;

		void AddRenderTargetBarriers(Pass* pass, bool is_begin);

		void AddBarrier(const D3D12_RESOURCE_BARRIER& transition);
		void FlushBarriers();

		void AddUAVBarriersBegin(DescriptorSetD3D12& descriptor_set);
		void AddUAVBarriersEnd();

		ID3D12CommandAllocator* GetCommandAllocator();

	private:
		void SetRootSignature(const RootSignatureD3D12& root_signature);
		void SetConstantBufferView(UINT root_parameter_index, D3D12_GPU_VIRTUAL_ADDRESS location);
		void SetDescriptorTable(UINT root_parameter_index, D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor);
		ID3D12Resource* GetTransitionResource(const ResourceTransition& transition);
		std::optional<D3D12_RESOURCE_BARRIER> GetTransitionBarrier(const ResourceTransition& transition);
	public:
		CommandBufferD3D12(const Memory::DebugStringA<>& name, IDevice* device);

		bool IsCompute() const { return is_in_compute_pass; }

		virtual bool Begin() override final;
		virtual void End() override final;

		virtual void BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags, simd::color clear_color, float clear_z) final;
		virtual void EndPass() final;

		virtual void BeginComputePass() override final;
		virtual void EndComputePass() override final;

		virtual void SetScissor(Rect rect) override final;

		void Transition(const ResourceTransition& transition);
		void Transition(Memory::Span<const ResourceTransition> transitions);
		void Transition(const ResourceTransitionD3D12& transition);
		void Transition(Memory::Span<const ResourceTransitionD3D12> transitions);

		virtual bool BindPipeline(Pipeline* pipeline) override final;
		virtual void BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets = nullptr, const DynamicUniformBuffers& dynamic_uniform_buffers = DynamicUniformBuffers()) override final;
		virtual void BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride) override final;

		virtual void Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start) override final;
		virtual void DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start) override final;
		virtual void Dispatch(unsigned group_count_x, unsigned group_count_y = 1, unsigned group_count_z = 1) override final;

		ID3D12GraphicsCommandList* GetCommandList();
		//virtual void SetCounter(StructuredBuffer* buffer, uint32_t value) override final;
	};

}