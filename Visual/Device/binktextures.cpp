
#ifdef ENABLE_BINK_VIDEO

namespace
{
	const Device::VertexElements vertex_elements =
	{
		{ 0, 0, Device::DeclType::FLOAT2, Device::DeclUsage::POSITION, 0 },
	};
	
	const float vertex_data[4][2] =
	{
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f }, 
		{ 0.0f, 1.0f }, 
		{ 1.0f, 1.0f }
	};

	const std::array < Device::BlendState, static_cast<std::size_t>( BinkBlendModes::NumBlendModes ) > bink_video_blendmode_states
	{
		Device::DisableBlendState{},

		Device::BlendState(
			Device::BlendChannelState( Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA ),
			Device::BlendChannelState( Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE ) ),

		Device::BlendState(
			Device::BlendChannelState( Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ONE ),
			Device::BlendChannelState( Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE ) ),

		Device::BlendState(
			Device::BlendChannelState( Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ZERO ),
			Device::BlendChannelState( Device::BlendMode::ONE, Device::BlendOp::ADD, Device::BlendMode::ZERO ) ),

		Device::DisableBlendState{},
	};
}

struct BinkShaders
{
	BINKSHADERS pub;

	Device::VertexDeclaration vertex_decl;

	Device::Handle<Device::Shader> pixel_shader_alpha;
	Device::Handle<Device::Shader> pixel_shader;
	Device::Handle<Device::Shader> vertex_shader;

	Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer_alpha;
	Device::Handle<Device::DynamicUniformBuffer> pixel_uniform_buffer;
	Device::Handle<Device::DynamicUniformBuffer> vertex_uniform_buffer;

	Device::Handle<Device::VertexBuffer> quad_vb;

#pragma pack(push)
#pragma pack(1)
	struct PipelineKey
	{
		Device::PointerID<Device::Pass> pass;
		bool is_alpha = false;
		bool is_pm = false;
		BinkBlendModes blend_mode{ BinkBlendModes::Unspecified };

		PipelineKey() {}
		PipelineKey(Device::Pass* pass, bool is_alpha, bool is_pm, BinkBlendModes blend_mode )
			: pass(pass), is_alpha(is_alpha), is_pm(is_pm), blend_mode(blend_mode) {}
	};
#pragma pack(pop)
	std::unique_ptr<Device::Cache<PipelineKey, Device::Pipeline>> pipelines;

#pragma pack(push)
#pragma pack(1)
	struct DescriptorSetKey
	{
		Device::PointerID<Device::Pipeline> pipeline;
		Device::PointerID<Device::DynamicBindingSet> pixel_binding_set;
		uint32_t samplers_hash = 0;

		DescriptorSetKey() {}
		DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
			: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash) {}
	};
#pragma pack(pop)
	std::unique_ptr<Device::Cache<DescriptorSetKey, Device::DescriptorSet>> descriptor_sets;

	Device::IDevice* device;
};

struct BinkTextures
{
	BINKTEXTURES pub;

#define BINKPLANEY      0
#define BINKPLANECR     1
#define BINKPLANECB     2
#define BINKPLANEA      3
#define BINKPLANECOUNT  4

	std::array<Device::Handle<Device::Texture>, BINKPLANECOUNT> tex;

	Device::Handle<Device::DynamicBindingSet> pixel_binding_set;
	Device::Handle<Device::DynamicBindingSet> pixel_binding_set_alpha;

	Device::IDevice* device;

	BinkShaders * shaders;
	HBINK bink;
	BINKFRAMEBUFFERS bink_buffers;

	int video_width, video_height;

	float x0, y0, x1, y1;
	float alpha;
	int is_pm;
	float u0, v0, u1, v1;
	std::array<float, 3> colour_multiply; // r, g, b
	BinkBlendModes blend_mode;
};


static BINKTEXTURES* Create_textures(BINKSHADERS* pshaders, HBINK bink, void* user_ptr);
static void Free_shaders(BINKSHADERS* pshaders);
static void Free_textures(BINKTEXTURES* ptextures);
static void Start_texture_update(BINKTEXTURES* ptextures);
static void Finish_texture_update(BINKTEXTURES* ptextures);
static void Draw_textures(BINKTEXTURES* ptextures, BINKSHADERS* pshaders);
static void Set_draw_position(BINKTEXTURES* ptextures, float x0, float y0, float x1, float y1);
static void Set_source_rect(BINKTEXTURES* ptextures, float u0, float v0, float u1, float v1);
static void Set_alpha_settings(BINKTEXTURES* ptextures, float alpha_value, int is_premultipied);
static void Set_blend_mode(BINKTEXTURES* ptextures, BinkBlendModes blend_mode);
static void Set_colour_multiply(BINKTEXTURES* ptextures, float r, float g, float b);


static void Free_shaders(BINKSHADERS* pshaders)
{
	auto* shaders = (BinkShaders*)pshaders;

	shaders->pixel_shader_alpha.Reset();
	shaders->pixel_shader.Reset();
	shaders->vertex_shader.Reset();
	shaders->pixel_uniform_buffer_alpha.Reset();
	shaders->pixel_uniform_buffer.Reset();
	shaders->pixel_uniform_buffer.Reset();
	if (shaders->pipelines)
		shaders->pipelines->Clear();
	if (shaders->descriptor_sets)
		shaders->descriptor_sets->Clear();
	shaders->quad_vb.Reset();
	shaders->device = nullptr;

	Memory::Free(shaders);
}

BINKSHADERS* Create_Bink_shaders(Device::IDevice* device)
{
	PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Bink);)

	if (device == 0)
		return nullptr;

	auto* shaders = (BinkShaders*)Memory::Allocate(Memory::Tag::Bink, sizeof(BinkShaders));
	memset((void*)shaders, 0, sizeof(*shaders));

	shaders->device = device;

	shaders->vertex_shader = Renderer::CreateCachedHLSLAndLoad(device, "Bink VS", Renderer::LoadShaderSource(L"Shaders/BinkVertexShader.hlsl"), nullptr, "main", Device::VERTEX_SHADER);
	shaders->pixel_shader = Renderer::CreateCachedHLSLAndLoad(device, "Bink PS", Renderer::LoadShaderSource(L"Shaders/BinkPixelShader.hlsl"), nullptr, "main", Device::PIXEL_SHADER);
	shaders->pixel_shader_alpha = Renderer::CreateCachedHLSLAndLoad(device, "Bink PS Alpha", Renderer::LoadShaderSource(L"Shaders/BinkPixelShaderAlpha.hlsl"), nullptr, "main", Device::PIXEL_SHADER);

	shaders->vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("Bink", shaders->device, shaders->vertex_shader.Get());
	shaders->pixel_uniform_buffer = Device::DynamicUniformBuffer::Create("Bink", shaders->device, shaders->pixel_shader.Get());
	shaders->pixel_uniform_buffer_alpha = Device::DynamicUniformBuffer::Create("Bink Alpha", shaders->device, shaders->pixel_shader_alpha.Get());

	shaders->vertex_decl = Device::VertexDeclaration(vertex_elements);

	Device::MemoryDesc init_mem = { vertex_data, 0, 0 };
	shaders->quad_vb = Device::VertexBuffer::Create("VB Bink", shaders->device, (UINT)sizeof(vertex_data), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &init_mem);

	shaders->pipelines = std::make_unique<Device::Cache<BinkShaders::PipelineKey, Device::Pipeline>>();
	shaders->descriptor_sets = std::make_unique<Device::Cache<BinkShaders::DescriptorSetKey, Device::DescriptorSet>>();

	shaders->pub.Create_textures = Create_textures;
	shaders->pub.Free_shaders = Free_shaders;

	return &shaders->pub;
}

static void Free_textures(BINKTEXTURES* ptextures)
{
	auto* textures = (BinkTextures*)ptextures;

	for (auto& texture : textures->tex)
		texture.Reset();

	textures->pixel_binding_set.Reset();
	textures->pixel_binding_set_alpha.Reset();

	Memory::Free(textures);
}

static BINKTEXTURES* Create_textures(BINKSHADERS* shaders, HBINK bink, void* user_ptr)
{
	PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Bink);)

	auto* textures = (BinkTextures*)Memory::Allocate(Memory::Tag::Bink, sizeof(BinkTextures));
	memset(textures, 0, sizeof(*textures));

	textures->pub.user_ptr = user_ptr;
	textures->pub.Free_textures = Free_textures;
	textures->pub.Start_texture_update = Start_texture_update;
	textures->pub.Finish_texture_update = Finish_texture_update;
	textures->pub.Draw_textures = Draw_textures;
	textures->pub.Set_draw_position = Set_draw_position;
	textures->pub.Set_source_rect = Set_source_rect;
	textures->pub.Set_alpha_settings = Set_alpha_settings;
	textures->pub.Set_blend_mode = Set_blend_mode;
	textures->pub.Set_colour_multiply = Set_colour_multiply;
	textures->shaders = (BinkShaders*)shaders;
	textures->bink = bink;
	textures->video_width = bink->Width;
	textures->video_height = bink->Height;
	textures->device = textures->shaders->device;

	BINKFRAMEBUFFERS& fb = textures->bink_buffers;
	BinkGetFrameBuffersInfo(bink, &fb);
	BinkAllocateFrameBuffers(bink, &fb, 0);

	BINKFRAMEPLANESET& ps = fb.Frames[0];
	const auto pixel_format = Device::PixelFormat::L8;
#if defined(PS4)
	const auto usage = Device::UsageHint::DYNAMIC;
#else
	const auto usage = Device::UsageHint::DEFAULT;
#endif
	const auto pool = Device::Pool::MANAGED_WITH_SYSTEMMEM;

	if (ps.YPlane.Allocate) textures->tex[BINKPLANEY] = Device::Texture::CreateTexture("Bink PlaneY", textures->device, fb.YABufferWidth, fb.YABufferHeight, 1, usage, pixel_format, pool, false, false, false);
	if (ps.cRPlane.Allocate) textures->tex[BINKPLANECR] = Device::Texture::CreateTexture("Bink PlaneCr", textures->device, fb.cRcBBufferWidth, fb.cRcBBufferHeight, 1, usage, pixel_format, pool, false, false, false);
	if (ps.cBPlane.Allocate) textures->tex[BINKPLANECB] = Device::Texture::CreateTexture("Bink PlaneCb", textures->device, fb.cRcBBufferWidth, fb.cRcBBufferHeight, 1, usage, pixel_format, pool, false, false, false);
	if (ps.APlane.Allocate) textures->tex[BINKPLANEA] = Device::Texture::CreateTexture("Bink PlaneA", textures->device, fb.YABufferWidth, fb.YABufferHeight, 1, usage, pixel_format, pool, false, false, false);

	Device::DynamicBindingSet::Inputs inputs;
	inputs.push_back({ "tex0", textures->tex[BINKPLANEY].Get() });
	inputs.push_back({ "tex1", textures->tex[BINKPLANECR].Get() });
	inputs.push_back({ "tex2", textures->tex[BINKPLANECB].Get() });
	inputs.push_back({ "tex3", textures->tex[BINKPLANEA].Get() });
	textures->pixel_binding_set = Device::DynamicBindingSet::Create("Bink", textures->device, textures->shaders->pixel_shader.Get(), inputs);
	textures->pixel_binding_set_alpha = Device::DynamicBindingSet::Create("Bink Alpha", textures->device, textures->shaders->pixel_shader_alpha.Get(), inputs);

	BinkRegisterFrameBuffers(bink, &fb);

	Set_draw_position(&textures->pub, 0, 0, 1, 1);
	Set_source_rect(&textures->pub, 0, 0, 1, 1);
	Set_alpha_settings(&textures->pub, 1.0f, 0);
	Set_blend_mode(&textures->pub, BinkBlendModes::Unspecified);
	Set_colour_multiply(&textures->pub, 1.f, 1.f, 1.f);

	return &textures->pub;
}

static void Start_texture_update(BINKTEXTURES* ptextures)
{
}

static void Finish_texture_update(BINKTEXTURES* ptextures)
{
	auto* textures = (BinkTextures*)ptextures;
	auto& ps = textures->bink_buffers.Frames[textures->bink_buffers.FrameNum];

	if (textures->tex[BINKPLANEY]) textures->tex[BINKPLANEY]->Fill(ps.YPlane.Buffer, ps.YPlane.BufferPitch);
	if (textures->tex[BINKPLANECR]) textures->tex[BINKPLANECR]->Fill(ps.cRPlane.Buffer, ps.cRPlane.BufferPitch);
	if (textures->tex[BINKPLANECB]) textures->tex[BINKPLANECB]->Fill(ps.cBPlane.Buffer, ps.cBPlane.BufferPitch);
	if (textures->tex[BINKPLANEA]) textures->tex[BINKPLANEA]->Fill(ps.APlane.Buffer, ps.APlane.BufferPitch);
}

static void Draw_textures(BINKTEXTURES* ptextures, BINKSHADERS* pshaders)
{
	auto* textures = (BinkTextures*)ptextures;
	auto* shaders = (BinkShaders*)pshaders;

	auto& command_buffer = *textures->device->GetCurrentUICommandBuffer();
	auto* pass = textures->device->GetCurrentUIPass();

	const bool is_alpha = textures->tex[BINKPLANEA] || textures->alpha < 0.999f;
	const bool is_pm = textures->is_pm;

	const auto blend_mode = (textures->blend_mode != BinkBlendModes::Unspecified)
		? textures->blend_mode
		: is_alpha ? (is_pm ? BinkBlendModes::AdditiveBlend : BinkBlendModes::AlphaBlend) : BinkBlendModes::Default;

	auto* pipeline = shaders->pipelines->FindOrCreate(BinkShaders::PipelineKey(pass, is_alpha, is_pm, blend_mode), [&]()
	{
		return Device::Pipeline::Create("Bink", textures->device, pass, Device::PrimitiveType::TRIANGLESTRIP,
			&shaders->vertex_decl, 
			shaders->vertex_shader.Get(), 
			is_alpha ? shaders->pixel_shader_alpha.Get() : shaders->pixel_shader.Get(),
			bink_video_blendmode_states[ static_cast<size_t>( blend_mode ) ],
			Device::UIRasterizerState(),
			Device::UIDepthStencilState());
	}).Get();
	if (command_buffer.BindPipeline(pipeline))
	{
		auto* vertex_uniform_buffer = shaders->vertex_uniform_buffer.Get();
		auto* pixel_uniform_buffer = textures->tex[BINKPLANEA] ? shaders->pixel_uniform_buffer_alpha.Get() : shaders->pixel_uniform_buffer.Get();

		pixel_uniform_buffer->SetVector("crc", (simd::vector4*) & textures->bink->ColorSpace[0]);
		pixel_uniform_buffer->SetVector("cbc", (simd::vector4*) & textures->bink->ColorSpace[4]);
		pixel_uniform_buffer->SetVector("adj", (simd::vector4*) & textures->bink->ColorSpace[8]);
		pixel_uniform_buffer->SetVector("yscale", (simd::vector4*) & textures->bink->ColorSpace[12]);

		const auto consta = simd::vector4(
			(textures->is_pm) ? textures->alpha : 1.0f,
			(textures->is_pm) ? textures->alpha : 1.0f,
			(textures->is_pm) ? textures->alpha : 1.0f,
			textures->alpha);
		pixel_uniform_buffer->SetVector("consta", &consta);

		const simd::vector4 colour_multiply{ textures->colour_multiply[0], textures->colour_multiply[1], textures->colour_multiply[2], 1.f };
		pixel_uniform_buffer->SetVector( "colour_multiply", &colour_multiply );

		const auto coord_xy = simd::vector4(
			(textures->x1 - textures->x0) * 2.0f,
			(textures->y1 - textures->y0) * -2.0f, // view space has +y = up, our coords have +y = down
			textures->x0 * 2.0f - 1.0f,
			 1.0f - textures->y0 * 2.0f);
		vertex_uniform_buffer->SetVector("coord_xy", &coord_xy);

		const float luma_u_scale = (float)textures->video_width / (float)textures->bink_buffers.YABufferWidth;
		const float luma_v_scale = (float)textures->video_height / (float)textures->bink_buffers.YABufferHeight;
		const float chroma_u_scale = (float)(textures->video_width / 2) / (float)textures->bink_buffers.cRcBBufferWidth;
		const float chroma_v_scale = (float)(textures->video_height / 2) / (float)textures->bink_buffers.cRcBBufferHeight;
		const simd::matrix uvmatrix = {
			 simd::vector4(
				(textures->u1 - textures->u0) * luma_u_scale,
				0.0f,
				(textures->u1 - textures->u0) * chroma_u_scale,
				0.0f),
			 simd::vector4(
				0.0f,
				(textures->v1 - textures->v0) * luma_v_scale,
				0.0f,
				(textures->v1 - textures->v0) * chroma_v_scale),
			 simd::vector4(
				textures->u0 * luma_u_scale,
				textures->v0 * luma_v_scale,
				textures->u0 * chroma_u_scale,
				textures->v0 * chroma_v_scale),
			0
		};
		vertex_uniform_buffer->SetMatrix("constuv", &uvmatrix);

		auto* pixel_binding_set = is_alpha ? textures->pixel_binding_set_alpha.Get() : textures->pixel_binding_set.Get();

		auto* descriptor_set = shaders->descriptor_sets->FindOrCreate(BinkShaders::DescriptorSetKey(pipeline, pixel_binding_set, textures->device->GetSamplersHash()), [&]()
		{
			return Device::DescriptorSet::Create("Bink", textures->device, pipeline, {}, { pixel_binding_set });
		}).Get();

		command_buffer.BindDescriptorSet(descriptor_set, {}, { vertex_uniform_buffer, pixel_uniform_buffer });

		command_buffer.BindBuffers(nullptr, shaders->quad_vb.Get(), 0, 2 * sizeof(float));
		command_buffer.Draw(4, 0);
	}
}

static void Set_draw_position(BINKTEXTURES* ptextures, float x0, float y0, float x1, float y1)
{
	auto* textures = (BinkTextures*)ptextures;
	textures->x0 = x0;
	textures->y0 = y0;
	textures->x1 = x1;
	textures->y1 = y1;
}

static void Set_source_rect(BINKTEXTURES* ptextures, float u0, float v0, float u1, float v1)
{
	auto* textures = (BinkTextures*)ptextures;
	textures->u0 = u0;
	textures->v0 = v0;
	textures->u1 = u1;
	textures->v1 = v1;
}

static void Set_alpha_settings(BINKTEXTURES* ptextures, float alpha_value, int is_premultipied)
{
	auto* textures = (BinkTextures*)ptextures;
	textures->alpha = alpha_value;
	textures->is_pm = is_premultipied;
}

static void Set_blend_mode( BINKTEXTURES* ptextures, BinkBlendModes blend_mode )
{
	auto* textures = (BinkTextures*)ptextures;
	textures->blend_mode = blend_mode;
}

static void Set_colour_multiply( BINKTEXTURES* const ptextures, const float r, const float g, const float b )
{
	auto* const textures = (BinkTextures*)ptextures;
	textures->colour_multiply[0] = r;
	textures->colour_multiply[1] = g;
	textures->colour_multiply[2] = b;
}

#endif
