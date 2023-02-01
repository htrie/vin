
namespace Device
{
	namespace
	{

		bool use_constant_buffers_workaround = false;
	}
	
	class Command
	{
	public:
		enum class Type : unsigned int
		{
			None = 0,
			UpdateSubresource,
			SetRenderTargets,
			SetViewports,
			SetScissorRects,
			SetPipeline,
			SetPipelineCS,
			SetSamplerVS,
			SetSamplerPS,
			SetSamplerCS,
			SetSRVsVS,
			SetSRVsPS,
			SetSRVsCS,
			SetUAVsGraphic,
			SetUAVsCS,
			SetConstantBufferVS,
			SetConstantBufferPS,
			SetConstantBufferCS,
			SetConstantBuffersVS1,
			SetConstantBuffersPS1,
			SetConstantBuffersCS1,
			SetVertexBuffer,
			SetIndexBuffer,
			Draw,
			DrawIndexedInstanced,
			Dispatch,
			ClearState,
		};

		Command(Type type) : type(type) {}

		Type GetType() const { return type;  }

	private:
		Type type;
	};


	class CommandUpdateSubresource : public Command
	{
	public:
		CommandUpdateSubresource(ID3D11Resource* resource, UINT level, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
			: Command(Type::UpdateSubresource), resource(resource), level(level), pSrcData(pSrcData), SrcRowPitch(SrcRowPitch), SrcDepthPitch(SrcDepthPitch) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->UpdateSubresource(resource, level, nullptr, pSrcData, SrcRowPitch, SrcDepthPitch);
		}

	private:
		ID3D11Resource* resource;
		UINT level;
		const void* pSrcData;
		UINT SrcRowPitch;
		UINT SrcDepthPitch;
	};


	class CommandSetRenderTargets : public Command
	{
	public:
		CommandSetRenderTargets(unsigned rtv_count, ID3D11RenderTargetView** _rtvs, ID3D11DepthStencilView* dsv, bool clear_color, bool clear_depth_stencil, DirectX::PackedVector::XMCOLOR color, UINT depth_stencil_flags, float Z)
			: Command(Type::SetRenderTargets), rtv_count(rtv_count), dsv(dsv), clear_color(clear_color), clear_depth_stencil(clear_depth_stencil), color(color), depth_stencil_flags(depth_stencil_flags), Z(Z)
		{
			assert(rtv_count <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);

			for (unsigned i = 0; i < rtv_count; ++i)
			{
				rtvs[i] = _rtvs[i];
			}
		}

		void Execute(ID3D11DeviceContext* context)
		{
			context->OMSetRenderTargets(rtv_count, rtvs, dsv);

			if (clear_color)
			{
				DirectX::XMVECTORF32 c;
				c.v = DirectX::PackedVector::XMLoadColor(&color);
				for (unsigned i = 0; i < rtv_count; ++i)
				{
					if (rtvs[i])
					{
						context->ClearRenderTargetView(rtvs[i], c);
					}
				}
			}

			if (clear_depth_stencil && dsv)
			{
				context->ClearDepthStencilView(dsv, depth_stencil_flags, Z, 0);
			}
		}

	private:
		unsigned rtv_count;
		ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D11DepthStencilView* dsv;
		bool clear_color;
		bool clear_depth_stencil;
		DirectX::PackedVector::XMCOLOR color;
		UINT depth_stencil_flags;
		float Z;
	};


	class CommandSetViewports : public Command
	{
	public:
		CommandSetViewports(unsigned int count, D3D11_VIEWPORT* viewports)
			: Command(Type::SetViewports), count(count)
		{
			assert(count <= D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX);

			for (unsigned i = 0; i < count; ++i)
			{
				this->viewports[i] = viewports[i];
			}
		}

		void Execute(ID3D11DeviceContext* context)
		{
			context->RSSetViewports(count, viewports);
		}

	private:
		unsigned count;
		D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
	};


    class CommandSetScissorRects : public Command
    {
    public:
        CommandSetScissorRects(unsigned count, D3D11_RECT* rects)
            : Command(Type::SetScissorRects), count(count)
        {
            assert(count <= D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX);

            for (unsigned i = 0; i < count; ++i)
            {
                this->rects[i] = rects[i];
            }
        }

        void Execute(ID3D11DeviceContext* context)
        {
            context->RSSetScissorRects(count, rects);
        }

    private:
        unsigned count;
        D3D11_RECT rects[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    };

	
	class CommandSetPipeline : public Command
	{
	public:
		CommandSetPipeline(D3D11_PRIMITIVE_TOPOLOGY primitive_type, ID3D11InputLayout* layout, ID3D11VertexShader* vertex_shader, ID3D11PixelShader* pixel_shader, ID3D11BlendState* blend_state, ID3D11RasterizerState* rasterizer_state, ID3D11DepthStencilState* depth_stencil_state, DWORD stencil_ref)
			: Command(Type::SetPipeline), primitive_type(primitive_type), layout(layout), vertex_shader(vertex_shader), pixel_shader(pixel_shader), blend_state(blend_state), rasterizer_state(rasterizer_state), depth_stencil_state(depth_stencil_state), stencil_ref(stencil_ref) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->IASetPrimitiveTopology(primitive_type);
			context->IASetInputLayout(layout);
			context->VSSetShader(vertex_shader, 0, 0);
			context->PSSetShader(pixel_shader, 0, 0);
			context->OMSetBlendState(blend_state, NULL, (UINT)0xFFFFFFFFF);
			context->RSSetState(rasterizer_state);
			context->OMSetDepthStencilState(depth_stencil_state, stencil_ref);
		}

	private:
		D3D11_PRIMITIVE_TOPOLOGY primitive_type;
		ID3D11InputLayout* layout;
		ID3D11VertexShader* vertex_shader;
		ID3D11PixelShader* pixel_shader;
		ID3D11BlendState* blend_state;
		ID3D11RasterizerState* rasterizer_state;
		ID3D11DepthStencilState* depth_stencil_state;
		DWORD stencil_ref;
	};


	class CommandSetPipelineCS : public Command
	{
	public:
		CommandSetPipelineCS(ID3D11ComputeShader* compute_shader)
			: Command(Type::SetPipelineCS), compute_shader(compute_shader) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->CSSetShader(compute_shader, nullptr, 0);
		}

	private:
		ID3D11ComputeShader* compute_shader;
	};


	class CommandSetSamplerVS : public Command
	{
	public:
		CommandSetSamplerVS(ID3D11SamplerState* sampler_state, unsigned int index)
			: Command(Type::SetSamplerVS), sampler_state(sampler_state), index(index) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->VSSetSamplers(index, 1, &sampler_state);
		}

	private:
		ID3D11SamplerState* sampler_state;
		unsigned int index;
	};


	class CommandSetSamplerPS : public Command
	{
	public:
		CommandSetSamplerPS(ID3D11SamplerState* sampler_state, unsigned int index)
			: Command(Type::SetSamplerPS), sampler_state(sampler_state), index(index) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->PSSetSamplers(index, 1, &sampler_state);
		}

	private:
		ID3D11SamplerState* sampler_state;
		unsigned int index;
	};

	class CommandSetSamplerCS : public Command
	{
	public:
		CommandSetSamplerCS(ID3D11SamplerState* sampler_state, unsigned int index)
			: Command(Type::SetSamplerCS), sampler_state(sampler_state), index(index) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->CSSetSamplers(index, 1, &sampler_state);
		}

	private:
		ID3D11SamplerState* sampler_state;
		unsigned int index;
	};

	class CommandSetSRVsVS : public Command
	{
	public:
		CommandSetSRVsVS(unsigned count, ID3D11ShaderResourceView** srvs)
			: Command(Type::SetSRVsVS), count(count), srvs(srvs) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->VSSetShaderResources(0, count, srvs);
		}

	private:
		unsigned count;
		ID3D11ShaderResourceView** srvs;
	};


	class CommandSetSRVsPS : public Command
	{
	public:
		CommandSetSRVsPS(unsigned count, ID3D11ShaderResourceView** srvs)
			: Command(Type::SetSRVsPS), count(count), srvs(srvs) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->PSSetShaderResources(0, count, srvs);
		}

	private:
		unsigned count;
		ID3D11ShaderResourceView** srvs;
	};


	class CommandSetSRVsCS : public Command
	{
	public:
		CommandSetSRVsCS(unsigned count, ID3D11ShaderResourceView** srvs)
			: Command(Type::SetSRVsCS), count(count), srvs(srvs) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->CSSetShaderResources(0, count, srvs);
		}

	private:
		unsigned count;
		ID3D11ShaderResourceView** srvs;
	};


	class CommandSetUAVsGraphic : public Command
	{
	public:
		CommandSetUAVsGraphic(unsigned start_slot, unsigned count, ID3D11UnorderedAccessView** uavs, const UINT* initial_counts)
			: Command(Type::SetUAVsGraphic), count(count), start_slot(start_slot)
		{
			this->uavs.fill(nullptr);
			for (unsigned a = start_slot; a < count; a++)
				this->uavs[a] = uavs[a];

			this->initial_counts.fill(UINT(-1));
			for (unsigned a = 0; a < count; a++)
				this->initial_counts[a] = initial_counts[a];
		}

		void Execute(ID3D11DeviceContext* context)
		{
			if(count > start_slot)
				context->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, start_slot, count - start_slot, uavs.data() + start_slot, initial_counts.data() + start_slot);
		}

	private:
		unsigned start_slot;
		unsigned count;
		std::array<ID3D11UnorderedAccessView*, UAVSlotCount> uavs;
		std::array<UINT, UAVSlotCount> initial_counts;
	};


	class CommandSetUAVsCS : public Command
	{
	public:
		CommandSetUAVsCS(unsigned count, ID3D11UnorderedAccessView** uavs, const UINT* initial_counts)
			: Command(Type::SetUAVsCS), count(count)
		{
			this->uavs.fill(nullptr);
			for (unsigned a = 0; a < count; a++)
				this->uavs[a] = uavs[a];

			this->initial_counts.fill(UINT(-1));
			for (unsigned a = 0; a < count; a++)
				this->initial_counts[a] = initial_counts[a];
		}

		void Execute(ID3D11DeviceContext* context)
		{
			context->CSSetUnorderedAccessViews(0, count, uavs.data(), initial_counts.data());
		}

	private:
		unsigned count;
		std::array<ID3D11UnorderedAccessView*, UAVSlotCount> uavs;
		std::array<UINT, UAVSlotCount> initial_counts;
	};


	class CommandSetConstantBufferVS : public Command
	{
	public:
		CommandSetConstantBufferVS(ID3D11Buffer* buffer, unsigned index)
			: Command(Type::SetConstantBufferVS), buffer(buffer), index(index) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->VSSetConstantBuffers(index, 1, &buffer);
		}

	private:
		ID3D11Buffer* buffer;
		unsigned index;
	};


	class CommandSetConstantBufferPS : public Command
	{
	public:
		CommandSetConstantBufferPS(ID3D11Buffer* buffer, unsigned index)
			: Command(Type::SetConstantBufferPS), buffer(buffer), index(index) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->PSSetConstantBuffers(index, 1, &buffer);
		}

	private:
		ID3D11Buffer* buffer;
		unsigned index;
	};


	class CommandSetConstantBufferCS : public Command
	{
	public:
		CommandSetConstantBufferCS(ID3D11Buffer* buffer, unsigned index)
			: Command(Type::SetConstantBufferCS), buffer(buffer), index(index) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->CSSetConstantBuffers(index, 1, &buffer);
		}

	private:
		ID3D11Buffer* buffer;
		unsigned index;
	};


	class CommandSetConstantBuffersVS1 : public Command
	{
	public:
		CommandSetConstantBuffersVS1(ConstantBuffers1& constant_buffers1)
			: Command(Type::SetConstantBuffersVS1), constant_buffers1(constant_buffers1) {} // TODO: Avoid struct copy.

		void Execute(ID3D11DeviceContext* context)
		{
			auto context1 = (ID3D11DeviceContext1*)context;
			context1->VSSetConstantBuffers1(0, constant_buffers1.count, constant_buffers1.buffers, constant_buffers1.offsets, constant_buffers1.counts);
			if (use_constant_buffers_workaround)
			{
				ID3D11Buffer* null_buffer = nullptr;
				context1->VSSetConstantBuffers(0, 1, &null_buffer);
				if (constant_buffers1.count > 1)
					context1->VSSetConstantBuffers(constant_buffers1.count - 1, 1, &null_buffer);
				context1->VSSetConstantBuffers1(0, constant_buffers1.count, constant_buffers1.buffers, constant_buffers1.offsets, constant_buffers1.counts);
			}
		}

	private:
		ConstantBuffers1 constant_buffers1;
	};


	class CommandSetConstantBuffersPS1 : public Command
	{
	public:
		CommandSetConstantBuffersPS1(ConstantBuffers1& constant_buffers1)
			: Command(Type::SetConstantBuffersPS1), constant_buffers1(constant_buffers1) {} // TODO: Avoid struct copy.

		void Execute(ID3D11DeviceContext* context)
		{
			auto context1 = (ID3D11DeviceContext1*)context;
			context1->PSSetConstantBuffers1(0, constant_buffers1.count, constant_buffers1.buffers, constant_buffers1.offsets, constant_buffers1.counts);
			if (use_constant_buffers_workaround)
			{
				ID3D11Buffer* null_buffer = nullptr;
				context1->PSSetConstantBuffers(0, 1, &null_buffer);
				if (constant_buffers1.count > 1)
					context1->PSSetConstantBuffers(constant_buffers1.count - 1, 1, &null_buffer);
				context1->PSSetConstantBuffers1(0, constant_buffers1.count, constant_buffers1.buffers, constant_buffers1.offsets, constant_buffers1.counts);
			}
		}

	private:
		ConstantBuffers1 constant_buffers1;
	};


	class CommandSetConstantBuffersCS1 : public Command
	{
	public:
		CommandSetConstantBuffersCS1(ConstantBuffers1& constant_buffers1)
			: Command(Type::SetConstantBuffersCS1), constant_buffers1(constant_buffers1) {} // TODO: Avoid struct copy.

		void Execute(ID3D11DeviceContext* context)
		{
			auto context1 = (ID3D11DeviceContext1*)context;
			context1->CSSetConstantBuffers1(0, constant_buffers1.count, constant_buffers1.buffers, constant_buffers1.offsets, constant_buffers1.counts);
			if (use_constant_buffers_workaround)
			{
				ID3D11Buffer* null_buffer = nullptr;
				context1->CSSetConstantBuffers(0, 1, &null_buffer);
				if (constant_buffers1.count > 1)
					context1->CSSetConstantBuffers(constant_buffers1.count - 1, 1, &null_buffer);
				context1->CSSetConstantBuffers1(0, constant_buffers1.count, constant_buffers1.buffers, constant_buffers1.offsets, constant_buffers1.counts);
			}
		}

	private:
		ConstantBuffers1 constant_buffers1;
	};


	class CommandSetVertexBuffer : public Command
	{
	public:
		CommandSetVertexBuffer(ID3D11Buffer* vertex_buffer, unsigned int offset, unsigned int stride, ID3D11Buffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
			: Command(Type::SetVertexBuffer), vertex_buffers({vertex_buffer, instance_vertex_buffer}), offsets({offset, instance_offset}), strides({stride, instance_stride}) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->IASetVertexBuffers(0, vertex_buffers[1] ? 2 : 1, vertex_buffers.data(), strides.data(), offsets.data());
		}

	private:
		std::array<ID3D11Buffer*, 2> vertex_buffers;
		std::array<unsigned, 2> offsets;
		std::array<unsigned, 2> strides;
	};

	class CommandSetIndexBuffer : public Command
	{
	public:
		CommandSetIndexBuffer(ID3D11Buffer* index_buffer, DXGI_FORMAT index_format)
			: Command(Type::SetIndexBuffer), index_buffer(index_buffer), index_format(index_format) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->IASetIndexBuffer(index_buffer, index_format, 0);
		}

	private:
		ID3D11Buffer* index_buffer;
		DXGI_FORMAT index_format;
	};

	
	class CommandDraw : public Command
	{
	public:
		CommandDraw(UINT vertex_count, UINT instance_count, UINT start_vertex, UINT start_instance)
			: Command(Type::Draw), vertex_count(vertex_count), start_vertex(start_vertex), instance_count(instance_count), start_instance(start_instance) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
		}

	private:
		UINT vertex_count;
		UINT start_vertex;
		UINT instance_count;
		UINT start_instance;
	};


	class CommandDrawIndexedInstanced : public Command
	{
	public:
		CommandDrawIndexedInstanced(UINT index_count, UINT instance_count, UINT start_index, INT base_vertex, UINT start_instance)
			: Command(Type::DrawIndexedInstanced), index_count(index_count), instance_count(instance_count), start_index(start_index), base_vertex(base_vertex), start_instance(start_instance) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->DrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance);
		}

	private:
		UINT index_count;
		UINT instance_count;
		UINT start_index;
		UINT base_vertex;
		UINT start_instance;
	};

	class CommandDispatch : public Command
	{
	public:
		CommandDispatch(UINT group_count_x, UINT group_count_y, UINT group_count_z)
			: Command(Type::Dispatch), group_count_x(group_count_x), group_count_y(group_count_y), group_count_z(group_count_z) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->Dispatch(group_count_x, group_count_y, group_count_z);
		}

	private:
		UINT group_count_x;
		UINT group_count_y;
		UINT group_count_z;
	};

	class CommandClearState : public Command
	{
	public:
		CommandClearState()
			: Command(Type::ClearState) {}

		void Execute(ID3D11DeviceContext* context)
		{
			context->ClearState();
		}
	};

	CommandListD3D11::CommandListD3D11(ID3D11Device* device)
	{
		// https://msdn.microsoft.com/en-us/library/windows/desktop/hh446795(v=vs.85).aspx
		D3D11_FEATURE_DATA_THREADING threadingCaps = { FALSE, FALSE };
		HRESULT hr = device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadingCaps, sizeof(threadingCaps));
		use_constant_buffers_workaround = SUCCEEDED(hr) && !threadingCaps.DriverCommandLists;
	}

	void CommandListD3D11::PushUpdateSubresource(ID3D11Resource* resource, UINT level, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
	{
		auto* command = (CommandUpdateSubresource*)commands.Allocate(sizeof(CommandUpdateSubresource));
		new (command) CommandUpdateSubresource(resource, level, pSrcData, SrcRowPitch, SrcDepthPitch);
	}

	void CommandListD3D11::PushSetRenderTargets(unsigned rtv_count, ID3D11RenderTargetView** rtvs, ID3D11DepthStencilView* dsv, bool clear_color, bool clear_depth_stencil, DirectX::PackedVector::XMCOLOR color, UINT depth_stencil_flags, float Z)
	{
		auto* command = (CommandSetRenderTargets*)commands.Allocate(sizeof(CommandSetRenderTargets));
		new (command) CommandSetRenderTargets(rtv_count, rtvs, dsv, clear_color, clear_depth_stencil, color, depth_stencil_flags, Z);
	}

	void CommandListD3D11::PushSetViewports(unsigned int count, D3D11_VIEWPORT* viewports)
	{
		auto* command = (CommandSetViewports*)commands.Allocate(sizeof(CommandSetViewports));
		new (command) CommandSetViewports(count, viewports);
	}

    void CommandListD3D11::PushSetScissorRects(unsigned int count, D3D11_RECT* rects)
    {
        auto* command = (CommandSetScissorRects*)commands.Allocate(sizeof(CommandSetScissorRects));
        new (command) CommandSetScissorRects(count, rects);
    }
	
	void CommandListD3D11::PushSetPipeline(D3D11_PRIMITIVE_TOPOLOGY primitive_type, ID3D11InputLayout* layout, ID3D11VertexShader* vertex_shader, ID3D11PixelShader* pixel_shader, ID3D11BlendState* blend_state, ID3D11RasterizerState* rasterizer_state, ID3D11DepthStencilState* depth_stencil_state, DWORD stencil_ref)
	{
		auto* command = (CommandSetPipeline*)commands.Allocate(sizeof(CommandSetPipeline));
		new (command) CommandSetPipeline(primitive_type, layout, vertex_shader, pixel_shader, blend_state, rasterizer_state, depth_stencil_state, stencil_ref);
	}

	void CommandListD3D11::PushSetPipeline(ID3D11ComputeShader* compute_shader)
	{
		auto* command = (CommandSetPipelineCS*)commands.Allocate(sizeof(CommandSetPipeline));
		new (command) CommandSetPipelineCS(compute_shader);
	}

	void CommandListD3D11::PushSetSamplerStateVS(ID3D11SamplerState* sampler_state, unsigned index)
	{
		auto* command = (CommandSetSamplerVS*)commands.Allocate(sizeof(CommandSetSamplerVS));
		new (command) CommandSetSamplerVS(sampler_state, index);
	}

	void CommandListD3D11::PushSetSamplerStatePS(ID3D11SamplerState* sampler_state, unsigned index)
	{
		auto* command = (CommandSetSamplerPS*)commands.Allocate(sizeof(CommandSetSamplerPS));
		new (command) CommandSetSamplerPS(sampler_state, index);
	}

	void CommandListD3D11::PushSetSamplerStateCS(ID3D11SamplerState* sampler_state, unsigned index)
	{
		auto* command = (CommandSetSamplerCS*)commands.Allocate(sizeof(CommandSetSamplerCS));
		new (command) CommandSetSamplerCS(sampler_state, index);
	}

	void CommandListD3D11::PushSetSRVsVS(unsigned count, ID3D11ShaderResourceView** srvs)
	{
		auto* command = (CommandSetSRVsVS*)commands.Allocate(sizeof(CommandSetSRVsVS));
		new (command) CommandSetSRVsVS(count, srvs);
	}

	void CommandListD3D11::PushSetSRVsPS(unsigned count, ID3D11ShaderResourceView** srvs)
	{
		auto* command = (CommandSetSRVsPS*)commands.Allocate(sizeof(CommandSetSRVsPS));
		new (command) CommandSetSRVsPS(count, srvs);
	}

	void CommandListD3D11::PushSetSRVsCS(unsigned count, ID3D11ShaderResourceView** srvs)
	{
		auto* command = (CommandSetSRVsCS*)commands.Allocate(sizeof(CommandSetSRVsCS));
		new (command) CommandSetSRVsCS(count, srvs);
	}

	void CommandListD3D11::PushSetUAVsGraphic(unsigned start_slot, unsigned count, ID3D11UnorderedAccessView** uavs, const UINT* initial_counts)
	{
		auto* command = (CommandSetUAVsGraphic*)commands.Allocate(sizeof(CommandSetUAVsGraphic));
		new (command) CommandSetUAVsGraphic(start_slot, count, uavs, initial_counts);
	}

	void CommandListD3D11::PushSetUAVsCS(unsigned count, ID3D11UnorderedAccessView** uavs, const UINT* initial_counts)
	{
		auto* command = (CommandSetUAVsCS*)commands.Allocate(sizeof(CommandSetUAVsCS));
		new (command) CommandSetUAVsCS(count, uavs, initial_counts);
	}

	void CommandListD3D11::PushSetConstantBufferVS(ID3D11Buffer* buffer, unsigned index)
	{
		auto* command = (CommandSetConstantBufferVS*)commands.Allocate(sizeof(CommandSetConstantBufferVS));
		new (command) CommandSetConstantBufferVS(buffer, index);
	}

	void CommandListD3D11::PushSetConstantBufferPS(ID3D11Buffer* buffer, unsigned index)
	{
		auto* command = (CommandSetConstantBufferPS*)commands.Allocate(sizeof(CommandSetConstantBufferPS));
		new (command) CommandSetConstantBufferPS(buffer, index);
	}

	void CommandListD3D11::PushSetConstantBufferCS(ID3D11Buffer* buffer, unsigned index)
	{
		auto* command = (CommandSetConstantBufferCS*)commands.Allocate(sizeof(CommandSetConstantBufferCS));
		new (command) CommandSetConstantBufferCS(buffer, index);
	}

	void CommandListD3D11::PushSetConstantBuffersVS1(ConstantBuffers1& constant_buffers1)
	{
		auto* command = (CommandSetConstantBuffersVS1*)commands.Allocate(sizeof(CommandSetConstantBuffersVS1));
		new (command) CommandSetConstantBuffersVS1(constant_buffers1);
	}

	void CommandListD3D11::PushSetConstantBuffersPS1(ConstantBuffers1& constant_buffers1)
	{
		auto* command = (CommandSetConstantBuffersPS1*)commands.Allocate(sizeof(CommandSetConstantBuffersPS1));
		new (command) CommandSetConstantBuffersPS1(constant_buffers1);
	}

	void CommandListD3D11::PushSetConstantBuffersCS1(ConstantBuffers1& constant_buffers1)
	{
		auto* command = (CommandSetConstantBuffersCS1*)commands.Allocate(sizeof(CommandSetConstantBuffersCS1));
		new (command) CommandSetConstantBuffersCS1(constant_buffers1);
	}

	void CommandListD3D11::PushSetVertexBuffer(ID3D11Buffer* vertex_buffer, unsigned offset, unsigned stride, ID3D11Buffer* instance_vertex_buffer, unsigned instance_offset, unsigned instance_stride)
	{
		auto* command = (CommandSetVertexBuffer*)commands.Allocate(sizeof(CommandSetVertexBuffer));
		new (command) CommandSetVertexBuffer(vertex_buffer, offset, stride, instance_vertex_buffer, instance_offset, instance_stride);
	}

	void CommandListD3D11::PushSetIndexBuffer(ID3D11Buffer* index_buffer, DXGI_FORMAT index_format)
	{
		auto* command = (CommandSetIndexBuffer*)commands.Allocate(sizeof(CommandSetIndexBuffer));
		new (command) CommandSetIndexBuffer(index_buffer, index_format);
	}

	void CommandListD3D11::PushDraw(UINT vertex_count, UINT instance_count, UINT start_vertex, UINT start_instance)
	{
		auto* command = (CommandDraw*)commands.Allocate(sizeof(CommandDraw));
		new (command) CommandDraw(vertex_count, instance_count, start_vertex, start_instance);
	}

	void CommandListD3D11::PushDrawIndexedInstanced(UINT index_count, UINT instance_count, UINT start_index, INT base_vertex, UINT start_instance)
	{
		auto* command = (CommandDrawIndexedInstanced*)commands.Allocate(sizeof(CommandDrawIndexedInstanced));
		new (command) CommandDrawIndexedInstanced(index_count, instance_count, start_index, base_vertex, start_instance);
	}

	void CommandListD3D11::PushDispatch(UINT group_count_x, UINT group_count_y, UINT group_count_z)
	{
		auto* command = (CommandDispatch*)commands.Allocate(sizeof(CommandDispatch));
		new(command)CommandDispatch(group_count_x, group_count_y, group_count_z);
	}

	void CommandListD3D11::PushClearState()
	{
		auto* command = (CommandClearState*)commands.Allocate(sizeof(CommandClearState));
		new(command)CommandClearState();
	}

	void CommandListD3D11::Clear()
	{
		commands.Clear();
	}

	void CommandListD3D11::Flush( ID3D11DeviceContext* context )
	{
		commands.Process([&](void* mem)
		{
			auto* command = (Command*)mem;

			switch (command->GetType())
			{
			case Command::Type::UpdateSubresource: static_cast<CommandUpdateSubresource*>(command)->Execute(context); break;
			case Command::Type::SetRenderTargets: static_cast<CommandSetRenderTargets*>(command)->Execute(context); break;
			case Command::Type::SetViewports: static_cast<CommandSetViewports*>(command)->Execute(context); break;
            case Command::Type::SetScissorRects: static_cast<CommandSetScissorRects*>(command)->Execute(context); break;
			case Command::Type::SetPipeline: static_cast<CommandSetPipeline*>(command)->Execute(context); break;
			case Command::Type::SetPipelineCS: static_cast<CommandSetPipelineCS*>(command)->Execute(context); break;
			case Command::Type::SetSamplerVS: static_cast<CommandSetSamplerVS*>(command)->Execute(context); break;
			case Command::Type::SetSamplerPS: static_cast<CommandSetSamplerPS*>(command)->Execute(context); break;
			case Command::Type::SetSamplerCS: static_cast<CommandSetSamplerCS*>(command)->Execute(context); break;
			case Command::Type::SetSRVsVS: static_cast<CommandSetSRVsVS*>(command)->Execute(context); break;
			case Command::Type::SetSRVsPS: static_cast<CommandSetSRVsPS*>(command)->Execute(context); break;
			case Command::Type::SetSRVsCS: static_cast<CommandSetSRVsCS*>(command)->Execute(context); break;
			case Command::Type::SetUAVsGraphic: static_cast<CommandSetUAVsGraphic*>(command)->Execute(context); break;
			case Command::Type::SetUAVsCS: static_cast<CommandSetUAVsCS*>(command)->Execute(context); break;
			case Command::Type::SetConstantBufferVS: static_cast<CommandSetConstantBufferVS*>(command)->Execute(context); break;
			case Command::Type::SetConstantBufferPS: static_cast<CommandSetConstantBufferPS*>(command)->Execute(context); break;
			case Command::Type::SetConstantBufferCS: static_cast<CommandSetConstantBufferCS*>(command)->Execute(context); break;
			case Command::Type::SetConstantBuffersVS1: static_cast<CommandSetConstantBuffersVS1*>(command)->Execute(context); break;
			case Command::Type::SetConstantBuffersPS1: static_cast<CommandSetConstantBuffersPS1*>(command)->Execute(context); break;
			case Command::Type::SetConstantBuffersCS1: static_cast<CommandSetConstantBuffersCS1*>(command)->Execute(context); break;
			case Command::Type::SetVertexBuffer: static_cast<CommandSetVertexBuffer*>(command)->Execute(context); break;
			case Command::Type::SetIndexBuffer: static_cast<CommandSetIndexBuffer*>(command)->Execute(context); break;
			case Command::Type::Draw: static_cast<CommandDraw*>(command)->Execute(context); break;
			case Command::Type::DrawIndexedInstanced: static_cast<CommandDrawIndexedInstanced*>(command)->Execute(context); break;
			case Command::Type::Dispatch: static_cast<CommandDispatch*>(command)->Execute(context); break;
			case Command::Type::ClearState: static_cast<CommandClearState*>(command)->Execute(context); break;
			}
		});

		commands.Reset();
	}

}
