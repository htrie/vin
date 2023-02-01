#pragma once

#include "Common/Simd/Curve.h"
#include "Common/FileReader/BlendMode.h"

#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Texture/TextureSystem.h"


namespace Device
{
	static const unsigned RenderTargetSlotCount = 4;
	static const unsigned TextureSlotCount = 64;
	static const unsigned UAVSlotCount = 8;
	static const unsigned SamplerSlotCount = 16;
	static const unsigned UniformBufferSlotCount = 10;

	class IDevice;
	class Shader;
	class Texture;
	class RenderTarget;
	class VertexDeclaration;
	class ByteAddressBuffer;
	class IndexBuffer;
	class StructuredBuffer;
	class TexelBuffer;
	class VertexBuffer;
	class CommandBuffer;
	struct Rect;

	enum class PrimitiveType : unsigned
	{
		POINTLIST,
		LINELIST,
		LINESTRIP,
		TRIANGLELIST,
		TRIANGLESTRIP,
		TRIANGLEFAN,
	};

	enum class ClearTarget : unsigned
	{
		NONE = 0,
		STENCIL = 1 << 0,
		DEPTH = 1 << 1,
		COLOR = 1 << 2,
	};

	enum class CompareMode : unsigned
	{
		NEVER,
		LESS,
		EQUAL,
		LESSEQUAL,
		GREATER,
		NOTEQUAL,
		GREATEREQUAL,
		ALWAYS,
		NumCompareMode
	};

	enum class FillMode : unsigned
	{
		POINT,
		WIREFRAME,
		SOLID,
		NumFillMode
	};

	enum class CullMode : unsigned
	{
		NONE,
		CW,
		CCW,
		NumCullMode
	};

	enum class BlendMode : unsigned
	{
		ZERO,
		ONE,
		SRCCOLOR,
		INVSRCCOLOR,
		SRCALPHA,
		INVSRCALPHA,
		DESTALPHA,
		INVDESTALPHA,
		DESTCOLOR,
		INVDESTCOLOR,
		SRCALPHASAT,
		BOTHSRCALPHA,
		BOTHINVSRCALPHA,
		BLENDFACTOR,
		INVBLENDFACTOR,
		NumBlendMode
	};

	enum class BlendOp : unsigned
	{
		ADD,
		SUBTRACT,
		REVSUBTRACT,
		MIN,
		MAX,
	};

	enum class StencilOp : unsigned
	{
		KEEP,
		REPLACE,
		NumStencilOp
	};

#pragma pack(push)	
#pragma pack(1)
	struct BlendChannelState
	{
		BlendMode src = BlendMode::ONE;
		BlendOp func = BlendOp::ADD;
		BlendMode dst = BlendMode::ZERO;

		BlendChannelState() = default;
		BlendChannelState(BlendMode src, BlendOp func, BlendMode dst) : src(src), func(func), dst(dst) {}
	};
#pragma pack(pop)
	struct DisableBlendChannelState : public BlendChannelState { DisableBlendChannelState() = default; };

#pragma pack(push)	
#pragma pack(1)
	struct BlendTargetState
	{
		bool enabled = false;
		BlendChannelState color = DisableBlendChannelState();
		BlendChannelState alpha = DisableBlendChannelState();

		BlendTargetState() = default;
		BlendTargetState(const BlendChannelState& color, const BlendChannelState& alpha, bool enabled = true) : color(color), alpha(alpha), enabled(enabled) {}
		BlendTargetState(const BlendChannelState& blend, bool enabled = true) : color(blend), alpha(blend), enabled(enabled) {}

		inline BlendTargetState& SetEnabled(bool enabled) { this->enabled = enabled; return *this; }
		inline BlendTargetState& SetColorBlend(const BlendChannelState& color) { this->color = color; return *this; }
		inline BlendTargetState& SetAlphaBlend(const BlendChannelState& alpha) { this->alpha = alpha; return *this; }

		uint32_t Hash() const;
	};
#pragma pack(pop)
	struct DisableBlendTargetState : public BlendTargetState { DisableBlendTargetState() : BlendTargetState(DisableBlendChannelState(), false) {} };

#pragma pack(push)	
#pragma pack(1)
	struct BlendState
	{
		BlendTargetState target[RenderTargetSlotCount];

		BlendState() = default;
		BlendState(const BlendTargetState& target0, const BlendTargetState& target1, const BlendTargetState& target2 = DisableBlendTargetState(), const BlendTargetState& target3 = DisableBlendTargetState());
		BlendState(const std::initializer_list<BlendTargetState>& list);
		BlendState(const Memory::Span<const BlendTargetState>& list);

		BlendState(const BlendTargetState& target0) : BlendState(target0, target0, target0, target0) {}
		BlendState(const BlendChannelState& color, const BlendChannelState& alpha, bool enabled = true) : BlendState(BlendTargetState(color, alpha, enabled)) {}
		BlendState(const BlendChannelState& blend, bool enabled = true) : BlendState(BlendTargetState(blend, enabled)) {}

		BlendTargetState& operator[](size_t s) { assert(s < std::size(target)); return target[s]; }
		const BlendTargetState& operator[](size_t s) const { assert(s < std::size(target)); return target[s]; }

		uint32_t Hash() const;
	};
#pragma pack(pop)
	struct DisableBlendState : public BlendState { DisableBlendState() : BlendState(DisableBlendTargetState()) {} };
	struct UIBlendState : public BlendState
	{
		// Set blending for pre-multiplied alpha: Srgb * ( 1 - Da ) + Drgb
		explicit UIBlendState(bool additive_blend = false) : BlendState(
			BlendChannelState(BlendMode::ONE, BlendOp::ADD, additive_blend ? BlendMode::ONE : BlendMode::INVSRCALPHA),
			BlendChannelState(BlendMode::INVDESTALPHA, BlendOp::ADD, BlendMode::ONE)
		) {}
	};

#pragma pack(push)	
#pragma pack(1)
	struct RasterizerState
	{
		CullMode cull_mode = CullMode::CCW;
		FillMode fill_mode = FillMode::SOLID;
		float depth_bias = 0;
		float slope_scale = 0;

		RasterizerState() = default;

		RasterizerState(CullMode cull_mode, FillMode fill_mode, float depth_bias, float slope_scale)
			: cull_mode(cull_mode)
			, fill_mode(fill_mode)
			, depth_bias(depth_bias)
			, slope_scale(slope_scale)
		{}

		inline RasterizerState& SetDepthBias(float depth_bias, float slope_scale = 0.0f) { this->depth_bias = depth_bias; this->slope_scale = slope_scale; return *this; }
		inline RasterizerState& SetRenderMode(CullMode cull_mode, FillMode fill_mode = FillMode::SOLID) { this->cull_mode = cull_mode; this->fill_mode = fill_mode; return *this; }

		uint32_t Hash() const;
	};
#pragma pack(pop)
	struct DefaultRasterizerState : public RasterizerState { DefaultRasterizerState() : RasterizerState(CullMode::CCW, FillMode::SOLID, 0.0f, 0.0f) {} };
	struct UIRasterizerState : public RasterizerState { UIRasterizerState() : RasterizerState(CullMode::NONE, FillMode::SOLID, 0.0f, 0.0f) {} };

#pragma pack(push)	
#pragma pack(1)
	struct DepthState
	{
		bool test_enabled = false;
		bool write_enabled = false;
		CompareMode compare_mode = CompareMode::LESSEQUAL;

		DepthState(bool test_enabled, CompareMode compare_mode) : DepthState(test_enabled, false, compare_mode) {}
		DepthState(bool test_enabled = false, bool write_enabled = false, CompareMode compare_mode = CompareMode::LESSEQUAL)
			: test_enabled(test_enabled || write_enabled) // Depth test needs to be enabled to allow depth writes
			, write_enabled(write_enabled)
			, compare_mode(compare_mode)
		{}
	};
#pragma pack(pop)
	struct DisableDepthState : public DepthState { DisableDepthState() = default; };
	struct WriteOnlyDepthState : public DepthState { WriteOnlyDepthState(bool write_enabled = true, CompareMode compare_mode = CompareMode::ALWAYS) : DepthState(false, write_enabled, compare_mode) {} };

#pragma pack(push)	
#pragma pack(1)
	struct StencilState
	{
		bool enabled = false;
		DWORD ref = 0;
		CompareMode compare_mode = CompareMode::ALWAYS;
		StencilOp pass_op = StencilOp::KEEP;
		StencilOp z_fail_op = StencilOp::KEEP;

		StencilState() = default;
		StencilState(bool enabled, DWORD ref, CompareMode compare_mode, StencilOp pass_op, StencilOp z_fail_op)
			: enabled(enabled)
			, ref(ref)
			, compare_mode(compare_mode)
			, pass_op(pass_op)
			, z_fail_op(z_fail_op)
		{}
	};
#pragma pack(pop)
	struct DisableStencilState : public StencilState { DisableStencilState() : StencilState(false, 0, CompareMode::ALWAYS, StencilOp::KEEP, StencilOp::KEEP) {} };

#pragma pack(push)	
#pragma pack(1)
	struct DepthStencilState
	{
		DepthState depth = DisableDepthState();
		StencilState stencil = DisableStencilState();

		DepthStencilState() = default;
		DepthStencilState(const DepthState& depth, const StencilState& stencil) : depth(depth), stencil(stencil) {}

		inline DepthStencilState& SetDepthState(const DepthState& depth) { this->depth = depth; return *this; }
		inline DepthStencilState& SetStencilState(const StencilState& stencil) { this->stencil = stencil; return *this; }

		uint32_t Hash() const;
	};
#pragma pack(pop)
	struct DisableDepthStencilState : public DepthStencilState { DisableDepthStencilState() : DepthStencilState(DisableDepthState(), DisableStencilState()) {} };
	struct UIDepthStencilState : public DepthStencilState { UIDepthStencilState(bool rendering_mask = false) : DepthStencilState(DepthState(!rendering_mask, !rendering_mask, CompareMode::LESSEQUAL), DisableStencilState()) {} };

#pragma pack(push)
#pragma pack(1)
	struct Input
	{
		uint64_t id_hash = 0;

		Input() {}
		Input(const uint64_t id_hash) : id_hash(id_hash) {}

		uint64_t GetIdHash() const { return id_hash; }
	};
#pragma pack(pop)


#pragma pack(push)
#pragma pack(1)
	struct UniformInput : public Input
	{
		enum class Type : uint8_t
		{
			None,
			Bool,
			Int,
			UInt,
			Float,
			Vector2,
			Vector3,
			Vector4,
			Matrix,
			Spline5,
		};

		const uint8_t* value = nullptr;
		Type type = Type::None;
		uint32_t hash = 0;

		UniformInput() {}
		UniformInput(const uint64_t id_hash)
			: Input(id_hash)
		{}

		bool operator==(const UniformInput& other);

		UniformInput& SetHash(const uint32_t hash);
		UniformInput& SetBool(const bool* value);
		UniformInput& SetInt(const int* value);
		UniformInput& SetUInt(const unsigned* value);
		UniformInput& SetFloat(const float* value);
		UniformInput& SetVector(const simd::vector2* value);
		UniformInput& SetVector(const simd::vector3* value);
		UniformInput& SetVector(const simd::vector4* value);
		UniformInput& SetMatrix(const simd::matrix* value);
		UniformInput& SetSpline5(const simd::Spline5* value);

		size_t GetSize() const;
	};
#pragma pack(pop)
	struct UniformInputs : public Memory::SmallVector<UniformInput, 4, Memory::Tag::Device>
	{
		uint32_t Hash() const;
		uint32_t HashLayout() const;
	};

	typedef Memory::SmallVector<const Device::UniformInputs*, 256, Memory::Tag::Render> BatchUniformInputs;


	struct BindingInput : public Input
	{
		enum class Type : uint8_t
		{
			None,
			Texture,
			TexelBuffer,
			ByteAddressBuffer,
			StructuredBuffer,
		};

		Type type;
		::Texture::Handle texture_handle;
		union
		{
			PointerID<Texture> texture = nullptr;
			PointerID<TexelBuffer> texel_buffer;
			PointerID<ByteAddressBuffer> byte_address_buffer;
			PointerID<StructuredBuffer> structured_buffer;
		};

		BindingInput() : type(Type::None) {}
		BindingInput(const uint64_t id_hash)
			: Input(id_hash), type(Type::None)
		{}

		BindingInput& SetTextureResource(::Texture::Handle texture_resource);
		BindingInput& SetTexture(Handle<Texture> texture);
		BindingInput& SetTexelBuffer(Handle<TexelBuffer> texel_buffer);
		BindingInput& SetByteAddressBuffer(Handle<ByteAddressBuffer> byte_address_buffer);
		BindingInput& SetStructuredBuffer(Handle<StructuredBuffer> structured_buffer);

		uint32_t Hash() const;
	};

	struct BindingInputs : public Memory::SmallVector<BindingInput, 4, Memory::Tag::DrawCalls>
	{
		typedef Memory::SmallVector<BindingInput, 4, Memory::Tag::DrawCalls> Parent;
		using Parent::Parent; // Use constructors of SmallVector, so we can keep using initialiser-list based constructor/etc

		uint32_t Hash() const;
	};

	typedef Memory::Array<PointerID<RenderTarget>, RenderTargetSlotCount> ColorRenderTargets;

	class Pass : public Resource
	{
		ColorRenderTargets color_render_targets;
		RenderTarget* depth_stencil = nullptr;

		static inline std::atomic_uint count = { 0 };

	protected:
		Pass(const Memory::DebugStringA<>& name, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil);

	public:
		virtual ~Pass();

		static Handle<Pass> Create(const Memory::DebugStringA<>& name, IDevice* device, const ColorRenderTargets& color_render_targets, RenderTarget* depth_stencil, ClearTarget clear_flags, simd::color clear_color, float clear_z);

		static unsigned GetCount() { return count.load(); }

		const ColorRenderTargets& GetColorRenderTargets() const { return color_render_targets; }
		const RenderTarget* GetDepthStencil() const { return depth_stencil; }
	};


	class Pipeline : public Resource
	{
	public:
		struct CPassMask
		{
			uint64_t vs_hash = 0;
			uint64_t ps_hash = 0;
			uint64_t cs_hash = 0;

			bool operator==(const CPassMask& o) const { return vs_hash == o.vs_hash && ps_hash == o.ps_hash && cs_hash == o.cs_hash; }
			bool operator!=(const CPassMask& o) const { return !operator==(o); }
		};

	private:
		VertexDeclaration vertex_declaration;

		Shader* vertex_shader = nullptr;
		Shader* pixel_shader = nullptr;
		Shader* compute_shader = nullptr;
		CPassMask cpass_mask;
		const bool is_graphics_pipeline;

		static inline std::atomic_uint count = { 0 };

	protected:
		Pipeline(const Memory::DebugStringA<>& name, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader);
		Pipeline(const Memory::DebugStringA<>& name, Shader* compute_shader);

	public:
		virtual ~Pipeline();

		virtual bool IsValid() const = 0;

		static Handle<Pipeline> Create(const Memory::DebugStringA<>& name, IDevice* device, const Pass* pass, PrimitiveType primitive_type, const VertexDeclaration* vertex_declaration, Shader* vertex_shader, Shader* pixel_shader, const BlendState& blend_state, const RasterizerState& rasterizer_state, const DepthStencilState& depth_stencil_state);
		static Handle<Pipeline> Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader);

		static unsigned GetCount() { return count.load(); }

		const VertexDeclaration& GetVertexDeclaration() const { return vertex_declaration; }
		Shader* GetVertexShader() const { return vertex_shader; }
		Shader* GetPixelShader() const { return pixel_shader; };
		Shader* GetComputeShader() const { return compute_shader; }
		CPassMask GetCPassMask() const { return cpass_mask; }

		bool IsGraphicPipeline() const { return is_graphics_pipeline; }
		Shader::WorkgroupSize GetWorkgroupSize() const
		{
			Shader* shader = is_graphics_pipeline ? vertex_shader : compute_shader;
			if (shader)
				return shader->GetWorkgroupSize();

			return Shader::WorkgroupSize();
		}
	};


	struct UniformAlloc
	{
		simd::vector4* mem = nullptr;
		uint32_t offset = 0;
		uint32_t count = 0;

		UniformAlloc() {}
		UniformAlloc(simd::vector4* mem, uint32_t offset, uint32_t count)
			: mem(mem), offset(offset), count(count) {}
	};

	struct UniformAllocs
	{
		UniformAlloc vs_alloc;
		UniformAlloc ps_alloc;
		UniformAlloc cs_alloc;

		operator bool() const
		{
			return 
				(vs_alloc.mem != nullptr && ps_alloc.mem != nullptr ) || 
				(cs_alloc.mem != nullptr);
		}
	};

	class UniformLayout
	{
		struct Entry
		{
			uint16_t offset = 0;
			uint16_t size = 0;
			UniformInput::Type type = UniformInput::Type::None;

			Entry() {}
			Entry(uint16_t offset, uint16_t size, UniformInput::Type type);
		};

		Memory::FlatMap<uint64_t, Entry, Memory::Tag::DrawCalls> entries;
		size_t stride = 0;

		static inline void GatherUniform(UniformInput::Type type, const uint8_t* src, size_t offset, uint8_t* dst);

		const Entry& Find(uint64_t id_hash) const;

	public:
		void Initialize(size_t count, size_t size);
		void Add(uint64_t id_hash, size_t offset, size_t size, UniformInput::Type type);

		void BatchGather(const BatchUniformInputs& batch_uniform_inputs, uint8_t* batch_mem) const;

		size_t GetStride() const { return stride; }
	};

	typedef std::array<UniformLayout, 3> UniformLayouts;

	struct Shaders
	{
		Handle<Shader> vs_shader;
		Handle<Shader> ps_shader;
		Handle<Shader> cs_shader;

		UniformLayouts vs_layouts;
		UniformLayouts ps_layouts;
		UniformLayouts cs_layouts;

		size_t GetMaxStride(unsigned rate, bool graphics) const { return graphics ? std::max(vs_layouts[rate].GetStride(), ps_layouts[rate].GetStride()) : cs_layouts[rate].GetStride(); }
	};


	class UniformOffsets
	{
	protected:
		struct Offset
		{
			uint8_t* mem = nullptr;
			void* buffer = nullptr;
			uint32_t offset = 0;
			uint32_t count = 0;
		};

		struct Buffer
		{
			uint64_t id_hash = 0;
			Offset vs_offset;
			Offset ps_offset;
			Offset cs_offset;
		};

		Buffer cpass;
		Buffer cpipeline;
		Buffer cobject;

	private:
		static void Validate(const UniformInputs& uniform_inputs, const Shader* shader, unsigned buffer_index, uint64_t buffer_id_hash);
		static void GatherUniform(UniformInput::Type type, const uint8_t* src, size_t offset, size_t size, uint8_t* dst);
		static Offset Allocate(IDevice* device, size_t size);
		static Offset Gather(IDevice* device, Shader* shader, const UniformInputs& uniform_inputs, uint64_t buffer_id_hash);

	public:
		void Flush(IDevice* device, Pipeline* pipeline, const UniformInputs& uniform_inputs, uint64_t buffer_id_hash);

		static const uint64_t cpass_id_hash = IdHash("cpass");
		static const uint64_t cpipeline_id_hash = IdHash("cpipeline");
		static const uint64_t cobject_id_hash = IdHash("cobject");

		const Buffer& GetCPassBuffer() const { return cpass; }
		const Buffer& GetCPipelineBuffer() const { return cpipeline; }
		const Buffer& GetCObjectBuffer() const { return cobject; }
	};


	class DynamicUniformBuffer : public Resource
	{
		static inline std::atomic_uint count = { 0 };

		Shader* shader = nullptr;

		struct Buffer
		{
			unsigned index = 0;
			size_t size = 0;
			std::unique_ptr<uint8_t[]> data;

			Buffer() {}
			Buffer(unsigned index, size_t size);
		};
		typedef Memory::SmallVector<Buffer, 8, Memory::Tag::Device> Buffers;
		Buffers buffers;

		void Patch(const char* name, const uint8_t* data, size_t size);

	protected:
		DynamicUniformBuffer(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader);

	public:
		virtual ~DynamicUniformBuffer();

		static Handle<DynamicUniformBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader);

		static unsigned GetCount() { return count.load(); }

		void SetBool(const char* name, bool b);
		void SetInt(const char* name, int i);
		void SetUInt(const char* name, unsigned int i);
		void SetFloat(const char* name, float f);
		void SetVector(const char* name, const simd::vector4* v);
		void SetVectorArray(const char* name, const simd::vector4* v, unsigned count);
		void SetMatrix(const char* name, const simd::matrix* m);

		const Buffers& GetBuffers() const { return buffers; }
		const ShaderType GetShaderType() const { return shader ? shader->GetType() : ShaderType::NULL_SHADER; }
	};

	class BindingSet : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	public:
	#pragma pack(push)
	#pragma pack(1)
		struct Input
		{
			uint64_t id_hash = 0;
			BindingInput::Type type;
			union
			{
				PointerID<Texture> texture = nullptr;
				PointerID<TexelBuffer> texel_buffer;
				PointerID<ByteAddressBuffer> byte_address_buffer;
				PointerID<StructuredBuffer> structured_buffer;
			};

			Input() : type(BindingInput::Type::None) {}
			Input(const BindingInput& texture_input);
		};
	#pragma pack(pop)

		struct Inputs : public Memory::Array<Input, TextureSlotCount>
		{
			Inputs() {}
			Inputs(const BindingInputs& inputs);

			uint32_t Hash() const;
		};

	protected:
		struct Slot
		{
			BindingInput::Type type;
			union
			{
				Texture* texture = nullptr;
				TexelBuffer* texel_buffer;
				ByteAddressBuffer* byte_address_buffer;
				StructuredBuffer* structured_buffer;
			};

			Slot() : type(BindingInput::Type::None) {}

			void Set(Texture* texture) { if (texture) { type = BindingInput::Type::Texture; this->texture = texture; } }
			void Set(TexelBuffer* texel_buffer) { if (texel_buffer) { type = BindingInput::Type::TexelBuffer; this->texel_buffer = texel_buffer; } }
			void Set(ByteAddressBuffer* byte_address_buffer) { if (byte_address_buffer) { type = BindingInput::Type::ByteAddressBuffer; this->byte_address_buffer = byte_address_buffer; } }
			void Set(StructuredBuffer* structured_buffer) { if (structured_buffer) { type = BindingInput::Type::StructuredBuffer; this->structured_buffer = structured_buffer; } }
		};

		struct Slots {
			typedef Memory::Array<Slot, TextureSlotCount> RVSlots;
			RVSlots srv_slots;
			RVSlots uav_slots;

			RVSlots& operator[](Shader::BindingType type) { return type == Shader::BindingType::UAV ? uav_slots : srv_slots; }
			const RVSlots& operator[](Shader::BindingType type) const { return type == Shader::BindingType::UAV ? uav_slots : srv_slots; }
		};

	private:
		Slots vs_slots;
		Slots ps_slots;
		Slots cs_slots;

		void InitSlots(Shader* shader, Slots& slots, const Inputs& inputs);

	protected:
		BindingSet(const Memory::DebugStringA<>& name, IDevice* device, Shader* vertex_shader, Shader* pixel_shader, const Inputs& inputs);
		BindingSet(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader, const Inputs& inputs);

	public:
		virtual ~BindingSet();

		static Handle<BindingSet> Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* vertex_shader, Shader* pixel_shader, const Inputs& inputs);
		static Handle<BindingSet> Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* compute_shader, const Inputs& inputs);

		static unsigned GetCount() { return count.load(); }

		const Slots& GetVSSlots() const { return vs_slots; } // TODO: Remove.
		const Slots& GetPSSlots() const { return ps_slots; } // TODO: Remove.
		const Slots& GetCSSlots() const { return cs_slots; } // TODO: Remove.
	};


	class DynamicBindingSet : public Resource
	{
		static inline std::atomic_uint count = { 0 };

		Shader* shader = nullptr;

		struct Slot
		{
			Texture* texture = nullptr;
			unsigned mip_level = (unsigned)-1;

			Slot() {}
			Slot(Texture* texture, unsigned mip_level)
				: texture(texture), mip_level(mip_level) {}
		};
		typedef std::array<Slot, TextureSlotCount> Slots;
		Slots slots;

		void Set(const uint64_t id_hash, Texture* texture, unsigned mip_level = (unsigned)-1);

	public:
	#pragma pack(push)
	#pragma pack(1)
		struct Input
		{
			PointerID<Texture> texture = nullptr;
			unsigned mip_level = (unsigned)-1;
			uint64_t id_hash = 0;

			Input() {}
			Input(const std::string& id, Texture* texture, unsigned mip_level = (unsigned)-1);
		};
	#pragma pack(pop)

		struct Inputs : public Memory::Array<Input, TextureSlotCount>
		{
			Inputs() {}
			Inputs(const BindingInputs& inputs);

			uint32_t Hash() const;
		};


	protected:
		DynamicBindingSet(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader, const Inputs& inputs);

	public:
		virtual ~DynamicBindingSet();

		static Handle<DynamicBindingSet> Create(const Memory::DebugStringA<>& name, IDevice* device, Shader* shader, const Inputs& inputs);

		static unsigned GetCount() { return count.load(); }

		const Slots& GetSlots() const { return slots; } // TODO: Remove.

		ShaderType GetShaderType() const { return shader ? shader->GetType() : ShaderType::NULL_SHADER; }
	};


	class DescriptorSet : public Resource
	{
	public:
		typedef Memory::Array<BindingSet*, 3> BindingSets;  // Pass, pipeline and object.
		typedef Memory::Array<DynamicBindingSet*, 2> DynamicBindingSets;

	private:
		Pipeline* pipeline = nullptr;

		DynamicUniformBuffer* vertex_uniform_buffer = nullptr;

		BindingSets binding_sets;
		DynamicBindingSets dynamic_binding_sets;

		static inline std::atomic_uint count = { 0 };

	#ifdef PROFILING
		unsigned texture_count = 0;
		unsigned texture_memory = 0;
		simd::vector2_int max_texture_size;
		simd::vector2_int avg_texture_size;
	#endif

	protected:
		DescriptorSet(const Memory::DebugStringA<>& name, Pipeline* pipeline, const BindingSets& binding_sets, const DynamicBindingSets& dynamic_binding_sets);

	#ifdef PROFILING
		void ComputeStats();
	#endif

	public:
		virtual ~DescriptorSet();

		static Handle<DescriptorSet> Create(const Memory::DebugStringA<>& name, IDevice* device, Pipeline* pipeline, const BindingSets& binding_sets = BindingSets(), const DynamicBindingSets& dynamic_binding_sets = DynamicBindingSets());

		static unsigned GetCount() { return count.load(); }

		Pipeline* GetPipeline() { return pipeline; }

		const auto& GetBindingSets() { return binding_sets; }
		const auto& GetDynamicBindingSets() const { return dynamic_binding_sets; }

	#ifdef PROFILING
		unsigned GetTextureCount() const { return texture_count; }
		unsigned GetTextureMemory() const { return texture_memory; }
		const auto& GetAvgTextureSize() const { return avg_texture_size; }
		const auto& GetMaxTextureSize() const { return max_texture_size; }
	#endif
	};


	class CommandBuffer : public Resource
	{
		static inline std::atomic_uint count = { 0 };

	protected:
		CommandBuffer(const Memory::DebugStringA<>& name);

		Shader::WorkgroupSize workgroup_size;

	public:
		typedef Memory::Array<DynamicUniformBuffer*, 2> DynamicUniformBuffers;

		virtual ~CommandBuffer();

		static Handle<CommandBuffer> Create(const Memory::DebugStringA<>& name, IDevice* device);

		static unsigned GetCount() { return count.load(); }

		virtual bool Begin() = 0;
		virtual void End() = 0;

		virtual void BeginPass(Pass* pass, Rect viewport_rect, Rect scissor_rect, ClearTarget clear_flags = ClearTarget::NONE, simd::color clear_color = 0, float clear_z = 0.0f) = 0;
		virtual void EndPass() = 0;

		virtual void BeginComputePass() = 0;
		virtual void EndComputePass() = 0;

		virtual void SetScissor(Rect rect) = 0;

		virtual bool BindPipeline(Pipeline* pipeline) = 0;
		virtual void BindDescriptorSet(DescriptorSet* descriptor_set, const UniformOffsets* uniform_offsets = nullptr, const DynamicUniformBuffers& dynamic_uniform_buffers = DynamicUniformBuffers()) = 0;
		virtual void BindBuffers(IndexBuffer* index_buffer, VertexBuffer* vertex_buffer, unsigned offset, unsigned stride, VertexBuffer* instance_vertex_buffer = nullptr, unsigned instance_offset = 0, unsigned instance_stride = 0) = 0;

		void Draw(unsigned vertex_count, unsigned vertex_start) { Draw(vertex_count, 1, vertex_start, 0); }
		virtual void Draw(unsigned vertex_count, unsigned instance_count, unsigned vertex_start, unsigned instance_start) = 0;
		virtual void DrawIndexed(unsigned index_count, unsigned instance_count, unsigned index_start, unsigned vertex_offset, unsigned instance_start) = 0;
		virtual void Dispatch(unsigned group_count_x, unsigned group_count_y = 1, unsigned group_count_z = 1) = 0;

		// Sets the counter value of an Append/Consume buffer. Must not be called inside a BeginPass/EndPass pair.
		//TODO: Counter Buffer is currently not supported
		//virtual void SetCounter(StructuredBuffer* buffer, uint32_t value) = 0;

		void DispatchThreads(unsigned threads_x, unsigned threads_y = 1, unsigned threads_z = 1)
		{
			assert(workgroup_size.x > 0 && workgroup_size.y > 0 && workgroup_size.z > 0);
			Dispatch((threads_x + workgroup_size.x - 1) / workgroup_size.x, (threads_y + workgroup_size.y - 1) / workgroup_size.y, (threads_z + workgroup_size.z - 1) / workgroup_size.z);
		}
	};

}

namespace Renderer::DrawCalls
{
	struct AlphaTestState
	{
		bool enabled;
		unsigned int ref;
		simd::vector4 default_alpha_ref = 0;

		AlphaTestState() = default;
		AlphaTestState(bool enable, unsigned int refvalue = 0)
			: enabled(enable), ref(refvalue), default_alpha_ref(1.0f, (float)refvalue / 255.0f, 0.001f, 1.0f)
		{}
	};

	struct BlendModeState
	{
		const AlphaTestState alpha_test;
		const Device::DepthStencilState* const depth_stencil_state;
		const Device::BlendState* const blend_state;

		BlendModeState(const AlphaTestState& alpha_test, const Device::DepthStencilState* depth_stencil_state, const Device::BlendState* blend_state)
			: alpha_test(alpha_test)
			, depth_stencil_state(depth_stencil_state)
			, blend_state(blend_state)
		{}
	};

	struct BlendModeInfo
	{
		const std::string macro;
		const BlendModeState* blend_mode_state = nullptr;
		const bool has_tool_visibility = false;
		const bool needs_sorting = false;
	};

	extern const BlendModeInfo blend_modes[(unsigned)BlendMode::NumBlendModes];
}