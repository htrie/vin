
namespace Device
{
	Memory::FlatSet<Line*, Memory::Tag::Device> Line::lines;

	namespace Internal
	{

		static const unsigned QuadMaxCount = 1 * 1024;
		static const unsigned LineMaxCount = 2 * 1024;

		static const unsigned QuadVertexCount = 4;
		static const unsigned QuadIndexCount = 6;
		static const unsigned LineVertexCount = 2;
		static const unsigned LineIndexCount = 2;


		struct Vertex
		{
			simd::vector4 position;
			simd::vector4 color;
		};

		template <unsigned VertexCount, unsigned IndexCount, unsigned MaxCount>
		class Buffer
		{
			Device::Handle<Device::VertexBuffer> vertex_buffer;
			Device::Handle<Device::IndexBuffer> index_buffer;
			Vertex* vertices = nullptr;
			uint16_t* indices = nullptr;
			unsigned used_count = 0;
			bool locked = false;

		public:
			static const size_t VertexTotalSize = MaxCount * sizeof(Vertex) * VertexCount;
			static const size_t IndexTotalSize = MaxCount * sizeof(uint16_t) * IndexCount;

			Buffer(Device::IDevice* device)
			{
				vertex_buffer = Device::VertexBuffer::Create("DebugVertexBuffer", device, (unsigned)VertexTotalSize, (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::Pool::DEFAULT, nullptr);
				index_buffer = Device::IndexBuffer::Create("DebugIndexBuffer", device, (unsigned)IndexTotalSize, (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY), Device::IndexFormat::_16, Device::Pool::DEFAULT, nullptr);
			}

			void Reset()
			{
				used_count = 0;
			}

			void Lock()
			{
				assert(!locked);
				vertex_buffer->Lock(0, (unsigned)VertexTotalSize, (void**)&vertices, Device::Lock::DISCARD);
				index_buffer->Lock(0, (unsigned)IndexTotalSize, (void**)&indices, Device::Lock::DISCARD);
				locked = true;
			}

			void Unlock()
			{
				assert(locked);
				vertex_buffer->Unlock();
				index_buffer->Unlock();
				locked = false;
			}

			std::pair<Vertex*, uint16_t*> Allocate(unsigned count)
			{
				assert(used_count + count <= MaxCount);
				const auto res = std::make_pair<Vertex*, uint16_t*>(
					&vertices[used_count * VertexCount],
					&indices[used_count * IndexCount]);
				used_count += count;
				return res;
			}

			unsigned GetUsedCount() const { return used_count; }
			Device::VertexBuffer& GetVertexBuffer() { return *vertex_buffer; }
			Device::IndexBuffer& GetIndexBuffer() { return *index_buffer; }

			bool IsFull() const { return used_count == MaxCount; }
		};

		template <typename B>
		class BufferList
		{
			std::deque<std::shared_ptr<B>> available_buffers;
			std::deque<std::shared_ptr<B>> used_buffers;
			std::deque<std::shared_ptr<B>> flushed_buffers;
			Device::IDevice* device = nullptr;

		public:
			BufferList(Device::IDevice* device)
				: device(device)
			{}

			void Swap()
			{
				while (used_buffers.size() > 0)
				{
					available_buffers.push_back(std::move(used_buffers.front()));
					used_buffers.pop_front();
				}

				while (flushed_buffers.size() > 0)
				{
					available_buffers.push_back(std::move(flushed_buffers.front()));
					flushed_buffers.pop_front();
				}

			}

			std::shared_ptr<B> Grab()
			{
				if (available_buffers.size() > 0)
				{
					auto buffer = std::move(available_buffers.front());
					available_buffers.pop_front();
					return buffer;
				}

				return std::make_shared<B>(device);
			}

			void Push(std::shared_ptr<B> buffer)
			{
				used_buffers.push_back(buffer);
			}

			std::deque<std::shared_ptr<B>>& GetUsedBuffers() { return used_buffers; }
			std::deque<std::shared_ptr<B>>& GetFlushedBuffers() { return flushed_buffers; }
		};

		class LineImpl
		{
			IDevice* device = nullptr;

			Memory::DebugStringA<> name;

			simd::vector2 dimensions;

			Handle<Shader> vertex_shader;
			Handle<Shader> pixel_shader;

			Handle<DynamicUniformBuffer> vertex_uniform_buffer;

			VertexDeclaration vertex_declaration;

		#pragma pack(push)	
		#pragma pack(1)
			struct PipelineKey
			{
				Device::PointerID<Device::Pass> pass;
				bool quad = false;

				PipelineKey() {}
				PipelineKey(Device::Pass* pass, bool quad)
					: pass(pass), quad(quad) {}
			};
		#pragma pack(pop)
			Device::Cache<PipelineKey, Device::Pipeline> pipelines;

		#pragma pack(push)
		#pragma pack(1)
			struct DescriptorSetKey
			{
				Device::PointerID<Device::Pipeline> pipeline;
				uint32_t samplers_hash = 0;

				DescriptorSetKey() {}
				DescriptorSetKey(Device::Pipeline* pipeline, uint32_t samplers_hash)
					: pipeline(pipeline), samplers_hash(samplers_hash) {}
			};
		#pragma pack(pop)
			Device::Cache<DescriptorSetKey, Device::DescriptorSet> descriptor_sets;

			typedef Buffer<QuadVertexCount, QuadIndexCount, QuadMaxCount> QuadBuffer;
			typedef Buffer<LineVertexCount, LineIndexCount, LineMaxCount> LineBuffer;

			BufferList<QuadBuffer> quad_buffer_list;
			BufferList<LineBuffer> line_buffer_list;

			std::shared_ptr<QuadBuffer> quad_current_buffer;
			std::shared_ptr<LineBuffer> line_current_buffer;

			float thickness = 1.0f;

			BlendState blend_state;

			void Flush();

			void FlushQuad(Device::CommandBuffer& command_buffer, Device::Pass* pass, QuadBuffer& buffer);
			void FlushLine(Device::CommandBuffer& command_buffer, Device::Pass* pass, LineBuffer& buffer);

			void CheckCurrentBufferLine();
			void CheckCurrentBufferQuad();

			void EndCurrentBufferQuad();
			void EndCurrentBufferLine();

			void DrawQuad(const simd::vector2& p0, const simd::vector2& p1, const simd::vector2& p2, const simd::vector2& p3, const simd::color& StartColor, const simd::color& EndColor);
			void DrawLine(const simd::vector2& p0, const simd::vector2& p1, const simd::color& StartColor, const simd::color& EndColor);
			void DrawLine(const simd::vector4& p0, const simd::vector4& p1, const simd::color& StartColor, const simd::color& EndColor);

		public:
			LineImpl(IDevice* device, const Memory::DebugStringA<>& name, const Device::BlendState& blend_state );

			void Swap();
			void OnResetDevice();
			void OnLostDevice();

			void Begin();
			void End();

			void Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& Color);
			void Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& StartColor, const simd::color& EndColor);
			void DrawTransform(const simd::vector3* pVertexList, DWORD dwVertexListCount, const simd::matrix* pTransform, const simd::color& Color);

			void SetPattern(DWORD dwPattern);
			DWORD GetPattern();
			void SetPatternScale(float fPatternScale);
			float GetPatternScale();
			void SetWidth(float width);
			float GetWidth();
			void SetAntialias(bool antialias);
			bool GetAntialias();
		};

		static Device::VertexElements vertex_def =
		{
			{ 0, offsetof(Vertex, position), Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
			{ 0, offsetof(Vertex, color), Device::DeclType::FLOAT4, Device::DeclUsage::COLOR, 0 },
		};

		LineImpl::LineImpl( IDevice* device, const Memory::DebugStringA<>& name, const BlendState& blend_state )
			: device(device)
			, name(name)
			, quad_buffer_list(device)
			, line_buffer_list(device)
			, blend_state(blend_state)
			, vertex_declaration(vertex_def)
		{
			vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "DebugLine_Vertex", Renderer::LoadShaderSource(L"Shaders/DebugLine_Vertex.hlsl"), nullptr, "main", ShaderType::VERTEX_SHADER);
			pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "DebugLine_Pixel", Renderer::LoadShaderSource(L"Shaders/DebugLine_Pixel.hlsl"), nullptr, "main", ShaderType::PIXEL_SHADER);

			vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("DebugLine VUB", device, vertex_shader.Get());
		}

		void LineImpl::Swap()
		{
			quad_buffer_list.Swap();
			line_buffer_list.Swap();
		}

		void LineImpl::OnResetDevice()
		{
		}

		void LineImpl::OnLostDevice()
		{
			descriptor_sets.Clear();
			pipelines.Clear();
		}

		void LineImpl::Flush()
		{
			auto& command_buffer = *device->GetCurrentUICommandBuffer();
			auto* pass = device->GetCurrentUIPass();

			auto& used_quad_buffers = quad_buffer_list.GetUsedBuffers();
			auto& flushed_quad_buffers = quad_buffer_list.GetFlushedBuffers();
			while (used_quad_buffers.size() > 0)
			{
				FlushQuad(command_buffer, pass, *used_quad_buffers.front().get());
				flushed_quad_buffers.push_back(std::move(used_quad_buffers.front()));
				used_quad_buffers.pop_front();
			}

			auto& used_line_buffers = line_buffer_list.GetUsedBuffers();
			auto& flushed_line_buffers = line_buffer_list.GetFlushedBuffers();
			while (used_line_buffers.size() > 0)
			{
				FlushLine(command_buffer, pass, *used_line_buffers.front().get());
				flushed_line_buffers.push_back(std::move(used_line_buffers.front()));
				used_line_buffers.pop_front();
			}
		}

		void LineImpl::CheckCurrentBufferQuad()
		{
			if (!quad_current_buffer || quad_current_buffer->IsFull())
			{
				EndCurrentBufferQuad();

				quad_current_buffer = quad_buffer_list.Grab();
				quad_current_buffer->Reset();
				quad_current_buffer->Lock();
			}
		}

		void LineImpl::CheckCurrentBufferLine()
		{
			if (!line_current_buffer || line_current_buffer->IsFull())
			{
				EndCurrentBufferLine();

				line_current_buffer = line_buffer_list.Grab();
				line_current_buffer->Reset();
				line_current_buffer->Lock();
			}
		}

		void LineImpl::EndCurrentBufferQuad()
		{
			if (quad_current_buffer)
			{
				quad_current_buffer->Unlock();
				quad_buffer_list.Push(std::move(quad_current_buffer));
			}
		}

		void LineImpl::EndCurrentBufferLine()
		{
			if (line_current_buffer)
			{
				line_current_buffer->Unlock();
				line_buffer_list.Push(std::move(line_current_buffer));
			}
		}

		void LineImpl::FlushQuad(Device::CommandBuffer& command_buffer, Device::Pass* pass, QuadBuffer& buffer)
		{
			if (const auto quad_draw_count = buffer.GetUsedCount())
			{
				const auto transform = simd::matrix::identity();
				vertex_uniform_buffer->SetMatrix("m_WorldViewProj", &transform);

				auto* pipeline = pipelines.FindOrCreate(PipelineKey(pass, true), [&]()
				{
					return Device::Pipeline::Create("DebugQuad", device, pass, PrimitiveType::TRIANGLELIST, &vertex_declaration, vertex_shader.Get(), pixel_shader.Get(), blend_state, 
						Device::UIRasterizerState().SetRenderMode(Device::CullMode::NONE, Device::FillMode::SOLID), Device::UIDepthStencilState());
				}).Get();
				if (command_buffer.BindPipeline(pipeline))
				{
					auto* descriptor_set = descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, device->GetSamplersHash()), [&]()
					{
						return Device::DescriptorSet::Create("DebugQuad", device, pipeline);
					}).Get();

					command_buffer.BindDescriptorSet(descriptor_set, {}, { vertex_uniform_buffer.Get() });
					command_buffer.BindBuffers(&buffer.GetIndexBuffer(), &buffer.GetVertexBuffer(), 0, sizeof(Vertex));
					command_buffer.DrawIndexed(quad_draw_count * 6, 1, 0, 0, 0);
				}
			}
		}

		void LineImpl::FlushLine(Device::CommandBuffer& command_buffer, Device::Pass* pass, LineBuffer& buffer)
		{
			if (const auto line_draw_count = buffer.GetUsedCount())
			{
				const auto transform = simd::matrix::identity();
				vertex_uniform_buffer->SetMatrix("m_WorldViewProj", &transform);

				auto* pipeline = pipelines.FindOrCreate(PipelineKey(pass, false), [&]()
				{
					return Device::Pipeline::Create("DebugLine", device, pass, PrimitiveType::LINELIST, &vertex_declaration, vertex_shader.Get(), pixel_shader.Get(), blend_state, 
						Device::UIRasterizerState().SetRenderMode(Device::CullMode::NONE, Device::FillMode::SOLID), Device::UIDepthStencilState());
				}).Get();
				if (command_buffer.BindPipeline(pipeline))
				{
					auto* descriptor_set = descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, device->GetSamplersHash()), [&]()
					{
						return Device::DescriptorSet::Create("DebugLine", device, pipeline);
					}).Get();

					command_buffer.BindDescriptorSet(descriptor_set, {}, { vertex_uniform_buffer.Get() });
					command_buffer.BindBuffers(&buffer.GetIndexBuffer(), &buffer.GetVertexBuffer(), 0, sizeof(Vertex));
					command_buffer.DrawIndexed(line_draw_count * 2, 1, 0, 0, 0);
				}
			}
		}

		void LineImpl::DrawQuad(const simd::vector2& p0, const simd::vector2& p1, const simd::vector2& p2, const simd::vector2& p3, const simd::color& StartColor, const simd::color& EndColor)
		{
			CheckCurrentBufferQuad();

			const auto start_rgba = StartColor.rgba();
			const auto end_rgba = EndColor.rgba();
			const auto start_index = quad_current_buffer->GetUsedCount() * QuadVertexCount;

			Vertex* vertices;
			uint16_t* indices;
			std::tie(vertices, indices) = quad_current_buffer->Allocate(1);

			vertices[0].position = simd::vector4(p0, 0.0f, 1.0f);
			vertices[1].position = simd::vector4(p1, 0.0f, 1.0f);
			vertices[2].position = simd::vector4(p2, 0.0f, 1.0f);
			vertices[3].position = simd::vector4(p3, 0.0f, 1.0f);
			vertices[0].color = start_rgba;
			vertices[1].color = end_rgba;
			vertices[2].color = start_rgba;
			vertices[3].color = end_rgba;

			indices[0] = start_index + 0;
			indices[1] = start_index + 1;
			indices[2] = start_index + 2;
			indices[3] = start_index + 1;
			indices[4] = start_index + 3;
			indices[5] = start_index + 2;
		}

		void LineImpl::DrawLine(const simd::vector2& p0, const simd::vector2& p1, const simd::color& StartColor, const simd::color& EndColor)
		{
			DrawLine(simd::vector4(p0, 0.0f, 1.0f), simd::vector4(p1, 0.0f, 1.0f), StartColor, EndColor);
		}

		void LineImpl::DrawLine(const simd::vector4& p0, const simd::vector4& p1, const simd::color& StartColor, const simd::color& EndColor)
		{
			CheckCurrentBufferLine();

			const auto start_rgba = StartColor.rgba();
			const auto end_rgba = EndColor.rgba();
			const auto start_index = line_current_buffer->GetUsedCount() * LineIndexCount;

			Vertex* vertices;
			uint16_t* indices;
			std::tie(vertices, indices) = line_current_buffer->Allocate(1);

			vertices[0].position = p0;
			vertices[1].position = p1;
			vertices[0].color = start_rgba;
			vertices[1].color = end_rgba;

			indices[0] = start_index + 0;
			indices[1] = start_index + 1;
		}

		void LineImpl::Begin()
		{
			Device::SurfaceDesc desc;
			device->GetBackBufferDesc(0, 0, &desc);
			dimensions = simd::vector2((float)desc.Width, (float)desc.Height);
		}

		void LineImpl::End()
		{
			EndCurrentBufferQuad();
			EndCurrentBufferLine();

			Flush();
		}

		void LineImpl::Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& Color)
		{
			Draw(pVertexList, dwVertexListCount, Color, Color);
		}

		void LineImpl::Draw(const simd::vector2 *pVertexList, DWORD dwVertexListCount, const simd::color& StartColor, const simd::color& EndColor)
		{
			if (dwVertexListCount < 2)
				return;

			if (thickness <= 1.0f)
			{
				for (unsigned i = 0; i < dwVertexListCount - 1; ++i)
				{
					const auto _p = pVertexList[i];
					const auto _q = pVertexList[i+1];

					const auto p = simd::vector2(_p.x, 1.0f - _p.y);
					const auto q = simd::vector2(_q.x, 1.0f - _q.y);

					const auto p0 = (p / dimensions) * 2.0f - simd::vector2(1.0f, -1.0f);
					const auto p1 = (q / dimensions) * 2.0f - simd::vector2(1.0f, -1.0f);

					DrawLine(p0, p1, StartColor, EndColor);
				}
			}
			else
			{
				for (unsigned i = 0; i < dwVertexListCount - 1; ++i)
				{
					const auto _p = pVertexList[i];
					const auto _q = pVertexList[i+1];

					const auto p = simd::vector2(_p.x, 1.0f - _p.y);
					const auto q = simd::vector2(_q.x, 1.0f - _q.y);

					simd::vector2 d = p - q;
					simd::vector2 n(d.y, -d.x);
					n = n.normalize() * thickness * 0.5f;

					const auto p0 = ((p + n) / dimensions) * 2.0f - simd::vector2(1.0f, -1.0f);
					const auto p1 = ((p - n) / dimensions) * 2.0f - simd::vector2(1.0f, -1.0f);
					const auto p2 = ((q + n) / dimensions) * 2.0f - simd::vector2(1.0f, -1.0f);
					const auto p3 = ((q - n) / dimensions) * 2.0f - simd::vector2(1.0f, -1.0f);

					DrawQuad(p0, p1, p2, p3, StartColor, EndColor);
				}
			}
		}

		void LineImpl::DrawTransform(const simd::vector3 *pVertexList, DWORD dwVertexListCount, const simd::matrix* pTransform, const simd::color& color)
		{
			if (dwVertexListCount < 2)
				return;

			if (thickness <= 1.0f)
			{
				for (unsigned i = 0; i < dwVertexListCount - 1; ++i)
				{
					const auto _p = *pTransform * simd::vector4(pVertexList[i], 1.0f);
					const auto _q = *pTransform * simd::vector4(pVertexList[i+1], 1.0f);

					DrawLine(_p, _q, color, color);
				}
			}
			else
			{
				for (unsigned i = 0; i < dwVertexListCount - 1; ++i)
				{
					const auto _p = *pTransform * simd::vector4(pVertexList[i], 1.0f);
					const auto _q = *pTransform * simd::vector4(pVertexList[i+1], 1.0f);

					const auto p = dimensions * simd::vector2(_p.x, _p.y) / _p.w;
					const auto q = dimensions * simd::vector2(_q.x, _q.y) / _q.w;

					simd::vector2 d = p - q;
					simd::vector2 n(-d.y, d.x);
					n = n.normalize() * thickness * 0.5f;

					const auto p0 = (p + n) / dimensions;
					const auto p1 = (p - n) / dimensions;
					const auto p2 = (q + n) / dimensions;
					const auto p3 = (q - n) / dimensions;

					DrawQuad(p0, p1, p2, p3, color, color);
				}
			}
		}

		void LineImpl::SetPattern( DWORD dwPattern )
		{
		}

		DWORD LineImpl::GetPattern()
		{
			return 0;
		}

		void LineImpl::SetPatternScale( float fPatternScale )
		{
		}

		float LineImpl::GetPatternScale()
		{
			return 0.0f;
		}

		void LineImpl::SetWidth( float width )
		{
			thickness = width;
		}

		float LineImpl::GetWidth()
		{
			return thickness;
		}

		void LineImpl::SetAntialias( bool antialias )
		{
		}

		bool LineImpl::GetAntialias()
		{
			return false;
		}

	}

	std::unique_ptr<Line> Line::Create(IDevice* device, const Memory::DebugStringA<>& name, const Device::BlendState& blend_state )
	{
		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Device);)
		return std::make_unique<Line>(device, name, blend_state );
	}

	void Line::SwapAll()
	{
		for (auto* line : lines)
			line->Swap();
	}

	Line::Line(IDevice* device, const Memory::DebugStringA<>& name, const Device::BlendState& blend_state)
		: impl(std::make_unique<Internal::LineImpl>(device, name, blend_state))
	{
		lines.insert(this);
	}

	Line::~Line()
	{ 
		lines.erase(this);
	}

	void Line::Swap()
	{
		impl->Swap();
	}

	void Line::OnResetDevice()
	{
		impl->OnResetDevice();
	}

	void Line::OnLostDevice()
	{
		impl->OnLostDevice();
	}

	void Line::Begin()
	{
		impl->Begin();
	}

	void Line::End()
	{
		impl->End();
	}

	void Line::Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& Color)
	{
		impl->Draw(pVertexList, dwVertexListCount, Color);
	}

	void Line::Draw(const simd::vector2* pVertexList, DWORD dwVertexListCount, const simd::color& StartColor, const simd::color& EndColor)
	{
		impl->Draw(pVertexList, dwVertexListCount, StartColor, EndColor);
	}

	void Line::DrawTransform(const simd::vector3* pVertexList, DWORD dwVertexListCount, const simd::matrix* pTransform, const simd::color& Color)
	{
		impl->DrawTransform(pVertexList, dwVertexListCount, pTransform, Color);
	}

	void Line::SetPattern(DWORD dwPattern)
	{
		impl->SetPattern(dwPattern);
	}

	DWORD Line::GetPattern()
	{
		return impl->GetPattern();
	}

	void Line::SetPatternScale(float fPatternScale)
	{
		impl->SetPatternScale(fPatternScale);
	}

	float Line::GetPatternScale()
	{
		return impl->GetPatternScale();
	}

	void Line::SetWidth(float width)
	{
		impl->SetWidth(width);
	}

	float Line::GetWidth()
	{
		return impl->GetWidth();
	}

	void Line::SetAntialias(bool antialias)
	{
		impl->SetAntialias(antialias);
	}

	bool Line::GetAntialias()
	{
		return impl->GetAntialias();
	}
}

