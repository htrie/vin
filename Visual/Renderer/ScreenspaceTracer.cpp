#include <vector>
#include <sstream>

#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/Numeric.h"
#include "Common/Utility/Geometric.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Renderer/TextureMapping.h"
#include "Visual/UI/UISystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Device/Buffer.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/Compiler.h"
#include "Visual/Profiler/JobProfile.h"

#include "GlobalShaderDeclarations.h"
#include "ScreenspaceTracer.h"
#include "PrecomputedTextures.h"
#include "RendererSettings.h"
#include "Scene/Camera.h"
#include "CachedHLSLShader.h"
#include "Utility.h"

#include "Visual/Physics/QuadTree.h"
#include "Visual/Physics/FluidDynamics/FluidSystem.h"
#include "Visual/Renderer/Scene/SceneDataSource.h"
#include "Common/Utility/HighResTimer.h"

namespace
{
	const Device::VertexElements vertex_elements =
	{
		{ 0, 0, Device::DeclType::FLOAT4, Device::DeclUsage::POSITION, 0 },
		{ 0, sizeof(float) * 4, Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0 },
	};

#pragma pack( push, 1 )
	struct Vertex
	{
		simd::vector4 viewport_pos;
		simd::vector2 tex_pos;
	};
#pragma pack( pop )

	const Vertex vertex_data[4] =
	{
		{ simd::vector4(-1.f, -1.f, 0.0f, 1.0f), simd::vector2(0.0f, 1.0f) },
		{ simd::vector4(-1.f, 1.f, 0.0f, 1.0f), simd::vector2(0.0f, 0.0f) },
		{ simd::vector4(1.f, -1.f, 0.0f, 1.0f), simd::vector2(1.0f, 1.0f) },
		{ simd::vector4(1.f, 1.f, 0.0f, 1.0f), simd::vector2(1.0f, 0.0f) }
	};
}

namespace Renderer
{
	ScreenspaceTracer::PassKey::PassKey(Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, bool clear, simd::color clear_color)
		: render_targets(render_targets), depth_stencil(depth_stencil), clear(clear), clear_color(clear_color)
	{}

	ScreenspaceTracer::PipelineKey::PipelineKey(Device::Pass* pass, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state)
		: pass(pass), pixel_shader(pixel_shader), blend_state(blend_state), rasterizer_state(rasterizer_state), depth_stencil_state(depth_stencil_state)
	{}

	ScreenspaceTracer::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
		: shader(shader), inputs_hash(inputs_hash)
	{}

	ScreenspaceTracer::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
		: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
	{}

	ScreenspaceTracer::UniformBufferKey::UniformBufferKey(Device::Shader * pixel_shader, Device::RenderTarget* dst)
		: pixel_shader(pixel_shader), dst(dst)
	{}

	struct ScreenspaceTracer::RiverFlowmapData
	{
		std::vector<Physics::FluidSystem::NodeDynamicData> node_dynamic_data;
		std::vector<Physics::FluidSystem::NodeStaticData> node_static_data;
		std::vector<float> shore_dist_data;
	};

	ScreenspaceTracer::ScreenspaceTracer()
		: vertex_declaration(vertex_elements)
	{
		camera = nullptr;
		ocean_data_id = -1;
		river_data_id = -1;
	}

	ScreenspaceTracer::~ScreenspaceTracer()
	{
		StopAsyncThreads();
	}

	Device::Pass* ScreenspaceTracer::FindPass(const Memory::DebugStringA<>& name, Device::ColorRenderTargets render_targets, Device::RenderTarget* depth_stencil, bool clear, simd::color clear_color)
	{
		Memory::Lock lock(passes_mutex);
		return passes.FindOrCreate(PassKey(render_targets, depth_stencil, clear, clear_color), [&]()
		{
			return Device::Pass::Create(name, device, render_targets, depth_stencil, clear ? Device::ClearTarget::COLOR : Device::ClearTarget::NONE, clear_color, 0.0f);
		}).Get();
	}

	Device::Pipeline* ScreenspaceTracer::FindPipeline(const Memory::DebugStringA<>& name, Device::Pass* pass, Device::Shader* pixel_shader, const Device::BlendState& blend_state, const Device::RasterizerState& rasterizer_state, const Device::DepthStencilState& depth_stencil_state)
	{
		Memory::Lock lock(pipelines_mutex);
		return pipelines.FindOrCreate(PipelineKey(pass, pixel_shader, blend_state, rasterizer_state, depth_stencil_state), [&]()
		{
			return Device::Pipeline::Create(name, device, pass, Device::PrimitiveType::TRIANGLESTRIP, &vertex_declaration, vertex_shader.Get(), pixel_shader, blend_state, rasterizer_state, depth_stencil_state);
		}).Get();
	}

	Device::DynamicBindingSet* ScreenspaceTracer::FindBindingSet(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, const Device::DynamicBindingSet::Inputs& inputs)
	{
		Memory::Lock lock(binding_sets_mutex);
		return binding_sets.FindOrCreate(BindingSetKey(pixel_shader, inputs.Hash()), [&]()
		{
			return Device::DynamicBindingSet::Create(name, device, pixel_shader, inputs);
		}).Get();
	}

	Device::DescriptorSet* ScreenspaceTracer::FindDescriptorSet(const Memory::DebugStringA<>& name, Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set)
	{
		Memory::Lock lock(descriptor_sets_mutex);
		return descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, device->GetSamplersHash()), [&]()
		{
			return Device::DescriptorSet::Create(name, device, pipeline, {}, { pixel_binding_set });
		}).Get();
	}

	Device::DynamicUniformBuffer* ScreenspaceTracer::FindUniformBuffer(const Memory::DebugStringA<>& name, Device::IDevice* device, Device::Shader* pixel_shader, Device::RenderTarget* dst)
	{
		Memory::Lock lock(uniform_buffers_mutex);
		return uniform_buffers.FindOrCreate(UniformBufferKey(pixel_shader, dst), [&]()
		{
			return Device::DynamicUniformBuffer::Create(name, device, pixel_shader);
		}).Get();
	}

	void ScreenspaceTracer::OnCreateDevice(Device::IDevice *device)
	{
		this->device = device;

		Device::MemoryDesc init_mem = { vertex_data, 0, 0 };
		vertex_buffer = Device::VertexBuffer::Create("VB ScreenSpaceTracer", device, 4 * sizeof(Vertex), Device::UsageHint::IMMUTABLE, Device::Pool::DEFAULT, &init_mem);

		vertex_shader = CreateCachedHLSLAndLoad(device, "ScreenTracerVertex", LoadShaderSource(L"Shaders/FullScreenQuad_VertexShader.hlsl"), nullptr, "VShad", Device::VERTEX_SHADER);

		{
			const Device::Macro* opaque_builder_macros = nullptr;

			depth_overrider.pixel_shader = CreateCachedHLSLAndLoad(device, "DepthOverrider", LoadShaderSource(L"Shaders/DepthOverrider.hlsl"), opaque_builder_macros, "OverrideDepth", Device::PIXEL_SHADER);

			global_illumination.pixel_shader = CreateCachedHLSLAndLoad(device, "GlobalIllumination", LoadShaderSource(L"Shaders/GlobalIllumination.hlsl"), opaque_builder_macros, "ApplyGlobalIllumination", Device::PIXEL_SHADER);

			size_t max_prebuilt_mip = 5;
			for (size_t mip_level = 0; mip_level <= max_prebuilt_mip; mip_level++)
			{
				screenspace_shadows[ScreenpsaceShadowsKey(false, mip_level)] = ScreenspaceShadows();
			}
			screenspace_shadows[ScreenpsaceShadowsKey(false, size_t(-1))] = ScreenspaceShadows();
			screenspace_shadows[ScreenpsaceShadowsKey(true, size_t(-1))] = ScreenspaceShadows();

			for (auto &it : screenspace_shadows)
			{
				auto &shadows = it.second;
				std::vector<Device::Macro> shadows_macros;
				if (it.first.use_offset_tex)
					shadows_macros.push_back({ "USE_OFFSET_TEX", "1" });

				std::string mip_str;
				if(it.first.mip_level != size_t(-1))
					mip_str = std::to_string(it.first.mip_level);
				else
					mip_str = "mip_level";

				shadows_macros.push_back({"MIP_LEVEL", mip_str.c_str()});
				shadows_macros.push_back({nullptr, nullptr});

				shadows.pixel_shader = CreateCachedHLSLAndLoad(device, "ScreenspaceShadows", LoadShaderSource(L"Shaders/ScreenspaceShadows.hlsl"), shadows_macros.data(), "ComputeScreenspaceShadows", Device::PIXEL_SHADER);
			}

			for (size_t mip_level = 0; mip_level <= max_prebuilt_mip; mip_level++)
			{
				screenspace_rays[ScreenspaceRaysKey(mip_level)] = ScreenspaceRays();
			}
			screenspace_rays[ScreenspaceRaysKey(size_t(-1))] = ScreenspaceRays();

			for (auto &it : screenspace_rays)
			{
				auto &rays_shader = it.second;
				std::vector<Device::Macro> rays_macros;

				std::string mip_str;
				if (it.first.mip_level != size_t(-1))
					mip_str = std::to_string(it.first.mip_level);
				else
					mip_str = "mip_level";

				rays_macros.push_back({ "MIP_LEVEL", mip_str.c_str() });
				rays_macros.push_back({ nullptr, nullptr });

				rays_shader.pixel_shader = CreateCachedHLSLAndLoad(device, "ScreenspaceRays", LoadShaderSource(L"Shaders/ScreenspaceRays.hlsl"), rays_macros.data(), "ComputeScreenspaceRays", Device::PIXEL_SHADER);
			}

			Device::Macro square_blur_macros[] =
			{
				{"SQUARE_BLUR", "1"},
				{ NULL, NULL }
			};
			Device::Macro linear_blur_macros[] =
			{
				{ "LINEAR_BLUR", "1" },
				{ NULL, NULL }
			};

			size_t depth_aware_blur_index = 0;
			for (auto &depth_aware_blur : depth_aware_blurs)
			{
				depth_aware_blur.pixel_shader = CreateCachedHLSLAndLoad(device, "DepthAwareBlur", LoadShaderSource(L"Shaders/DepthAwareBlur.hlsl"), depth_aware_blur_index == 0 ? square_blur_macros : linear_blur_macros, "ApplyBlur", Device::PIXEL_SHADER);

				depth_aware_blur_index++;
			}
			depth_aware_blur_square = &depth_aware_blurs[0];
			depth_aware_blur_linear = &depth_aware_blurs[1];

			Device::Macro blur_fp32_macros[] =
			{
				{"DST_FP32", "1"},
				{ NULL, NULL }
			};

			Device::Macro fp16_rgba_macros[] =
			{
				{"DST_FP16_RGBA", "1"},
				{ NULL, NULL }
			};

			size_t blur_index = 0;
			for (auto &blur : blurs)
			{
				blur.pixel_shader = CreateCachedHLSLAndLoad(device, "Blur", LoadShaderSource(L"Shaders/Blur.hlsl"), blur_index == 0 ? opaque_builder_macros : blur_fp32_macros, "ApplyBlur", Device::PIXEL_SHADER);

				blur_index++;
			}
			blur_8 = &blurs[0];
			blur_fp32 = &blurs[1];

			reprojectors[ReprojectorKey(true, false)] = Reprojector();
			reprojectors[ReprojectorKey(false, true)] = Reprojector();
			reprojectors[ReprojectorKey(true, true)] = Reprojector();

			for (auto &it : reprojectors)
			{
				auto &reprojector = it.second;
				std::string use_world_space = it.first.use_world_space ? "1" : "0";
				std::string use_view_space = it.first.use_view_space ? "1" : "0";
				Device::Macro reprojector_macros[] =
				{
					{ "USE_WORLD_SPACE", use_world_space.c_str() },
					{ "USE_VIEW_SPACE", use_view_space.c_str() },
					{ NULL, NULL }
				};

				reprojector.pixel_shader = CreateCachedHLSLAndLoad(device, "Reprojector", LoadShaderSource(L"Shaders/Reprojector.hlsl"), reprojector_macros, "Reproject", Device::PIXEL_SHADER);
			}


			depth_linearizers[DepthLinearizerKey(1)] = DepthLinearizer();
			depth_linearizers[DepthLinearizerKey(2)] = DepthLinearizer();
			depth_linearizers[DepthLinearizerKey(4)] = DepthLinearizer();
			depth_linearizers[DepthLinearizerKey(8)] = DepthLinearizer();

			for (auto &it : depth_linearizers)
			{
				auto &depth_linearizer = it.second;
				std::string downscale_str = std::to_string(it.first.downscale);
				Device::Macro downscale_macros[] =
				{
					{ "DOWNSCALE", downscale_str.c_str() },
					{ NULL, NULL }
				};

				depth_linearizer.pixel_shader = CreateCachedHLSLAndLoad(device, "DepthLinearizer", LoadShaderSource(L"Shaders/DepthLinearizer.hlsl"), downscale_macros, "LinearizeDepth", Device::PIXEL_SHADER);
			}

			{
				bloom_cutoff.pixel_shader = CreateCachedHLSLAndLoad(device, "BloomCutoff", LoadShaderSource(L"Shaders/BloomCutoff.hlsl"), nullptr, "BloomCutoff", Device::PIXEL_SHADER);
			}

			{
				bloom_gather.pixel_shader = CreateCachedHLSLAndLoad(device, "BloomGathering", LoadShaderSource(L"Shaders/BloomGather.hlsl"), nullptr, "BloomGather", Device::PIXEL_SHADER);
			}
#ifndef __APPLE__iphoneos
			{
				dof_layers.pixel_shader = CreateCachedHLSLAndLoad(device, "ComputeLayers", LoadShaderSource(L"Shaders/DepthOfField.hlsl"), fp16_rgba_macros, "ComputeLayers", Device::PIXEL_SHADER);
			}
			
			{
				dof_blur.pixel_shader = CreateCachedHLSLAndLoad(device, "DoFBlur", LoadShaderSource(L"Shaders/DepthOfField.hlsl"), fp16_rgba_macros, "DoFBlur", Device::PIXEL_SHADER);
			}
#endif
		}

		bool_true = true;
		bool_false = false;
	}

	void ScreenspaceTracer::OnResetDevice(Device::IDevice *device)
	{
		default_ambient_tex = device->GetBlackOpaqueTexture();
		distorton_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/particles/distortion/muddle_double.dds"));
		surface_foam_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/particles/environment_effects/waterfalls/twilight_strand/water_stream_at.dds"));
		caustics_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/particles/environment_effects/waterfalls/twilight_strand/water_caustics_2.dds"));
		photometric_profiles_texture = ::Texture::Handle(::Texture::LinearTextureDesc(L"Art/2DArt/Lookup/photometric_profiles.dds"));

		std::wstring ripple_texture_name = L"Art/2DArt/Lookup/surface_ripple.dds";
		std::wstring wave_profile_texture_name = L"Art/2DArt/Lookup/gerstner_wave_profile.dds";
		std::wstring wave_positive_profile_texture_name = L"Art/2DArt/Lookup/gerstner_positive_wave_profile.dds";
		std::wstring wave_profile_gradient_texture_name = L"Art/2DArt/Lookup/wave_profile_gradient.dds";
		std::wstring raycast_texture_name = L"Art/2DArt/Lookup/raycast_texture";
		std::wstring fiber_occlusion_texture_name = L"Art/2DArt/Lookup/fiber_occlusion.dds";
		std::wstring environment_ggx_texture_name = L"Art/2DArt/Lookup/environment_ggx.dds";
		std::wstring scattering_texture_name = L"Art/2DArt/Lookup/scattering.dds";
		std::wstring photometric_texture_name = L"Art/2DArt/Lookup/photometric_profiles.dds";
		std::wstring gerstner_texture_name = L"Art/2DArt/Lookup/gerstner_profile.dds";

		static bool is_created = false;
		if(!is_created)
		{
			is_created = true;
			//BuildSurfaceRippleTexture(device, ripple_texture_name.c_str());
			//BuildWaveProfileTexture(device, wave_profile_texture_name.c_str());
			//BuildWavePositiveProfileTexture(device, wave_positive_profile_texture_name.c_str());
			//BuildRaycastTexture(device, raycast_texture_name.c_str());
			//BuildFiberOcclusionTexture(device, fiber_occlusion_texture_name.c_str());
			//BuildScatteringLookupTexture(device, scattering_texture_name.c_str());
			//BuildEnvironmentGGXTexture(device, environment_ggx_texture_name.c_str());
			//BuildPhotometricProfilesTexture(device, photometric_texture_name.c_str());
			//BuildGerstnerProfileTexture(device, gerstner_texture_name.c_str());
			//BuildSdmTexture(device, L"Art/2DArt/Lookup/SDF.png", L"Art/2DArt/Lookup/sdm.dds");
		}

		ripple_texture = ::Texture::Handle(::Texture::LinearTextureDesc(ripple_texture_name));
		wave_profile_texture = ::Texture::Handle(::Texture::LinearTextureDesc(wave_profile_texture_name));
		wave_profile_gradient_texture = ::Texture::Handle(::Texture::LinearTextureDesc(wave_profile_gradient_texture_name));

		ocean_data_id = -1;
		river_data_id = -1;
	}


	void ScreenspaceTracer::OnLostDevice()
	{
		uniform_buffers.Clear();
		binding_sets.Clear();
		descriptor_sets.Clear();
		pipelines.Clear();
		passes.Clear();

		default_ambient_tex = {};
		distorton_texture = {};
		surface_foam_texture = {};
		caustics_texture = {};
		ripple_texture = {};
		wave_profile_texture = {};
		wave_profile_gradient_texture = {};
		photometric_profiles_texture = {};

		water_draw_data.shoreline_coords_tex = {};

		river_flowmap_data.texture = {};
		fog_flowmap_data.texture = {};
	}

	void ScreenspaceTracer::OnDestroyDevice()
	{
		device = nullptr;

		for (auto& blur : blurs)
		{
			blur.pixel_shader.Reset();
		}
		for (auto& depth_aware_blur : depth_aware_blurs)
		{
			depth_aware_blur.pixel_shader.Reset();
		}

		reprojectors.clear();
		vertex_buffer.Reset();
		vertex_shader.Reset();
		depth_overrider.pixel_shader.Reset();
		global_illumination.pixel_shader.Reset();
		screenspace_shadows.clear();
		screenspace_rays.clear();
		depth_linearizers.clear();
		bloom_cutoff.pixel_shader.Reset();
		bloom_gather.pixel_shader.Reset();
		dof_layers.pixel_shader.Reset();
		dof_blur.pixel_shader.Reset();
	}

	void ScreenspaceTracer::SetCamera(Scene::Camera * camera) 
	{
		this->camera = camera;
	}

	Vector2d MakeVector2d(simd::vector3 point)
	{
		return Vector2d(point.x, point.y);
	}

	float Ang(Vector2d v0, Vector2d v1)
	{
		v0 = v0.normalize();
		v1 = v1.normalize();
		float sinAng = v0.x * v1.y - v0.y - v1.x;
		float cosAng = v0.dot(v1);
		return std::atan2(sinAng, cosAng);
	}

	float Cross(Vector2d v0, Vector2d v1)
	{
		return v0.x * v1.y - v0.y * v1.x;
	}

	float Sgn(float val)
	{
		return val > 0.0f ? 1.0f : -1.0f;
	}

	void PhaseLine::LoadLineData(const PointLine & line_data, float smooth_ratio, int smooth_iterations, float min_curvature_radius)
	{
		this->base_line_data = line_data;
		base_line_data.Smooth(smooth_ratio, smooth_iterations, min_curvature_radius);

		this->longitudes.resize(this->base_line_data.points.size());
		float curr_longitude = 0.0f;
		for(size_t segment_index = 0; segment_index < this->base_line_data.segments.size(); segment_index++)
		{
			auto &curr_segment = this->base_line_data.segments[segment_index];
			for(int point_number = 0; point_number < curr_segment.count; point_number++)
			{
				size_t curr_index = curr_segment.offset + point_number;
				size_t next_index = curr_segment.offset + (point_number + curr_segment.count + 1) % curr_segment.count;

				this->longitudes[curr_index] = curr_longitude;

				if(!curr_segment.is_cycle && point_number >= curr_segment.count - 1)
					continue;

				Vector2d point0 = MakeVector2d(base_line_data.points[curr_index]);
				Vector2d point1 = MakeVector2d(base_line_data.points[next_index]);

				float edge_length = (point1 - point0).length();

				curr_longitude += edge_length;
			}
		}

		this->sum_longitude = curr_longitude;
	}

	inline void PhaseLine::LoadLineData(const PointLine & line_data)
	{
		LoadLineData(line_data, 0.0f, 0, 0.0f);
	}


	PointPhase PhaseLine::GetPointPhaseInternal(Vector2d planar_pos, bool attenuate_longitude_discontinuity, size_t line_index) const
	{
		float min_edge_length = 1e-2f;
		float max_edge_length = 1e4f;
		
		float min_dist = 1e5f;
		float closest_point_longitude = -1.0f;
		float intensity = 1.0f;

		for(size_t segment_index = 0; segment_index < this->base_line_data.segments.size(); segment_index++)
		{
			auto &curr_segment = this->base_line_data.segments[segment_index];
			for (int edge_index = 0; edge_index < curr_segment.count + (curr_segment.is_cycle ? 0 : -1); edge_index++)
			{
				size_t curr_index = curr_segment.offset + edge_index;
				size_t next_index = curr_segment.offset + (edge_index + curr_segment.count + 1) % curr_segment.count;

				Vector2d point0 = MakeVector2d(base_line_data.points[curr_index]);
				Vector2d point1 = MakeVector2d(base_line_data.points[next_index]);

				float point0_longitude = longitudes[curr_index];
				float point1_longitude = longitudes[next_index];

				float point0_intensity = 1.0f;
				float point1_intensity = 1.0f;

				if (!curr_segment.is_cycle || attenuate_longitude_discontinuity)
				{
					point0_intensity = edge_index == 0 ? 0.0f : 1.0f;
					point1_intensity = (edge_index + 1 == curr_segment.count - 1) ? 0.0f : 1.0f;
				}

				Vector2d delta = point1 - point0;
				Vector2d edge_normal = Vector2d(-delta.y, delta.x).normalize();

				float ratio = (planar_pos - point0).dot(point1 - point0) / ((point1 - point0).lengthsq() + 1e-5f);
				if(ratio > 0.0f && ratio < 1.0f)
				{
					float edge_dist = (planar_pos - point0).dot(edge_normal);
					if(std::abs(edge_dist) < std::abs(min_dist))
					{
						min_dist = edge_dist;
						closest_point_longitude = Lerp(point0_longitude, point1_longitude, ratio);
						intensity = Lerp(point0_intensity, point1_intensity, ratio);
					}
				}
			}
		}
		for(size_t segment_index = 0; segment_index < this->base_line_data.segments.size(); segment_index++)
		{
			auto &curr_segment = this->base_line_data.segments[segment_index];
			for(int point_number = 0; point_number < curr_segment.count; point_number++)
			{
				size_t point_index = curr_segment.offset + point_number;
				Vector2d point = MakeVector2d(base_line_data.points[point_index]);

				float point_longitude = longitudes[point_index];
				float point_intensity = 1.0f;
				


				size_t prev_index = curr_segment.offset + (point_number + curr_segment.count - 1) % curr_segment.count;
				size_t next_index = curr_segment.offset + (point_number + curr_segment.count + 1) % curr_segment.count;

				Vector2d prev_point = MakeVector2d(base_line_data.points[prev_index]);
				Vector2d next_point = MakeVector2d(base_line_data.points[next_index]);

				Vector2d next_delta = next_point - point;
				Vector2d next_normal = Vector2d(-next_delta.y, next_delta.x).normalize();

				Vector2d prev_delta = point - prev_point;
				Vector2d prev_normal = Vector2d(-prev_delta.y, prev_delta.x).normalize();

				Vector2d center_dist = planar_pos - point;

				bool prev_edge_fine = (prev_delta.length() > min_edge_length && prev_delta.length() < max_edge_length);
				bool next_edge_fine = (next_delta.length() > min_edge_length && next_delta.length() < max_edge_length);

				if (!curr_segment.is_cycle || attenuate_longitude_discontinuity)
				{
					prev_edge_fine = prev_edge_fine && (point_number > 0);
					next_edge_fine = next_edge_fine && (point_number < curr_segment.count - 1);
					point_intensity = (point_number > 0 && point_number < curr_segment.count - 1) ? 1.0f : 0.0f;
				}

				bool side0 = center_dist.dot(prev_normal) > 0.0f;
				bool side1 = center_dist.dot(next_normal) > 0.0f;

				float sign = 1.0f;

				if(prev_edge_fine && next_edge_fine)
				{
					if(Cross(prev_delta, next_delta) > 0.0f)
					{
						sign = !side0 || !side1 ? -1.0f : 1.0f;
					}else
					{
						sign = !side0 && !side1 ? -1.0f : 1.0f;
					}
				}

				if (prev_edge_fine && !next_edge_fine)
				{
					sign = side0 ? 1.0f : -1.0f;
				}
				if (!prev_edge_fine && next_edge_fine)
				{
					sign = side1 ? 1.0f : -1.0f;
				}

				float point_dist = center_dist.length() * sign;

				if(std::abs(point_dist) < std::abs(min_dist))
				{
					min_dist = point_dist;
					closest_point_longitude = point_longitude;
					intensity = point_intensity;
				}
			}
		}

		PointPhase res;
		res.dist = min_dist;
		res.longitude = closest_point_longitude;
		res.intensity = intensity;
		return res;
	}

	PointPhase PhaseLine::GetPointPhase(Vector2d planar_pos, bool attenuate_longitude_discontinuity) const
	{
		return GetPointPhaseInternal(planar_pos, attenuate_longitude_discontinuity, 0);
	}
	bool PhaseLine::IsLongitudeContinuous(float longitude0, float longitude1) const
	{
		if(fabs(longitude0 - longitude1) > 500.0f) return false; //50m threshold
		for(auto& segment: base_line_data.segments)
		{
			if(segment.count <= 0)
				continue;
			float start_longitude = longitudes[segment.offset];
			float end_longitude = longitudes[segment.offset + segment.count - 1];
			if(longitude0 >= start_longitude && longitude0 <= end_longitude)
			{
				return (longitude1 >= start_longitude && longitude1 <= end_longitude/* && longitude0 < fabs(end_longitude - start_longitude) * 0.2f*/);
			}
		}
		return false;
	}

	void PhaseLine::RenderWireframe(const simd::matrix & view_projection, float vertical_offset, simd::color color) const
	{
		const auto final_transform = view_projection;


		for(size_t segment_index = 0; segment_index < this->base_line_data.segments.size(); segment_index++)
		{
			auto &curr_segment = this->base_line_data.segments[segment_index];
			if (curr_segment.count <= 0)
				continue;
			for(size_t edge_index = 0; edge_index < curr_segment.count + (curr_segment.is_cycle ? 0 : -1); edge_index++)
			{
				size_t curr_index = curr_segment.offset + edge_index;
				size_t next_index = curr_segment.offset + (edge_index + curr_segment.count + 1) % curr_segment.count;

				float offset = vertical_offset;

				simd::vector3 dir = base_line_data.points[next_index] - base_line_data.points[curr_index];
				simd::vector3 tangent_offset = simd::vector3(-dir.y, dir.x, 0.0f) * 0.1f;
				float max_tangent_length = 50.0f;
				if (tangent_offset.len() > max_tangent_length)
					tangent_offset = tangent_offset.normalize() * max_tangent_length;

				simd::vector3 line_verts[2];
				line_verts[0] = base_line_data.points[curr_index] + tangent_offset;
				line_verts[0].z += offset;
				line_verts[1] = base_line_data.points[next_index];
				line_verts[1].z += offset;
				UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color.c());

				line_verts[0] = base_line_data.points[curr_index] - tangent_offset;
				line_verts[0].z += offset;
				line_verts[1] = base_line_data.points[next_index];
				line_verts[1].z += offset;
				UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color.c());
			}

			if (!curr_segment.is_cycle)
			{
				auto DrawCross = [&](simd::vector3 pos)
				{
					float offset = vertical_offset;
					float size = 50.0f;
					simd::vector3 line_verts[2];
					line_verts[0] = pos + simd::vector3(-size, -size, offset);
					line_verts[1] = pos + simd::vector3(size, size, offset);
					UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color.c());
					line_verts[0] = pos + simd::vector3(size, -size, offset);
					line_verts[1] = pos + simd::vector3(-size, size, offset);
					UI::System::Get().DrawTransform(&line_verts[0], 2, &final_transform, color.c());
				};

				DrawCross(base_line_data.points[curr_segment.offset]);
				DrawCross(base_line_data.points[curr_segment.offset + curr_segment.count - 1]);
			}
		}
	}

	float PhaseLine::GetSumLongitude() const
	{
		return this->sum_longitude;
	}

	inline size_t PhaseLine::GetPointsCount() const
	{
		return base_line_data.segments.size();
	}

	PointLine & PhaseLine::GetBaseLine()
	{
		return this->base_line_data;
	}

	using namespace TexUtil;

	struct ShoreGradients
	{
		Vector2d shore_gradient;
		Vector2d ocean_gradient;
		ShoreGradients operator *(float val)
		{
			ShoreGradients newbie;
			newbie.shore_gradient = this->shore_gradient * val;
			newbie.ocean_gradient = this->ocean_gradient * val;
			return newbie;
		}
		ShoreGradients operator +(ShoreGradients other)
		{
			ShoreGradients newbie;
			newbie.shore_gradient = this->shore_gradient + other.shore_gradient;
			newbie.ocean_gradient = this->ocean_gradient + other.ocean_gradient;
			return newbie;
		}
	};

	const float slope = 0.2f;
	const float dist_to_color = slope / 250.0f / 2.0f;

	using typename Physics::Vector2f;

	ShoreGradients GetWorldDistGradient (LockedData<Color128> coord_data, simd::vector2_int tex_pos, simd::vector2_int tex_size, simd::vector2 scene_size)
	{
		struct GradientSample
		{
			simd::vector2_int offset;
			float ocean_dist;
			float shore_dist;
		};
		GradientSample samples[4];

		GradientSample newbie;
		newbie.offset.x = -1;
		newbie.offset.y = 0;
		samples[0] = newbie;

		newbie.offset.x = 1;
		newbie.offset.y = 0;
		samples[1] = newbie;

		newbie.offset.x = 0;
		newbie.offset.y = -1;
		samples[2] = newbie;

		newbie.offset.x = 0;
		newbie.offset.y = 1;
		samples[3] = newbie;

		for (int i = 0; i < 4; i++)
		{
			simd::vector2_int curr_point = tex_pos + samples[i].offset;
			curr_point.x = (curr_point.x + tex_size.x) % tex_size.x;
			curr_point.y = (curr_point.y + tex_size.y) % tex_size.y;
			//for(int sample_index = 0; sample_index < samples.size(); sample_index++)
			samples[i].ocean_dist = (1.0f - coord_data.Get(curr_point).b) / dist_to_color;
			samples[i].shore_dist = (1.0f - coord_data.Get(curr_point).r) / dist_to_color;
		}

		float world_step = scene_size.x / float(tex_size.x);
		ShoreGradients res_gradients;
		res_gradients.shore_gradient = Vector2d(samples[1].shore_dist - samples[0].shore_dist, samples[3].shore_dist - samples[2].shore_dist) / (world_step + 1e-5f);
		res_gradients.ocean_gradient = Vector2d(samples[1].ocean_dist - samples[0].ocean_dist, samples[3].ocean_dist - samples[2].ocean_dist) / (world_step + 1e-5f);
		return res_gradients;
	};

	static void SmoothDistanceMap(LockedData<Color128> coord_data, simd::vector2_int tex_size)
	{
		std::vector<Color128> ping;
		ping.resize(tex_size.x * tex_size.y);

		struct Sample
		{
			simd::vector2_int offset;
			float weight;
		};

		std::vector<Sample> samples;

		Sample newbie;

		newbie.offset = simd::vector2_int(0, 0);
		newbie.weight = 1.0f;
		samples.push_back(newbie);

		newbie.offset = simd::vector2_int(1, 0);
		newbie.weight = 1.0f;
		samples.push_back(newbie);

		newbie.offset = simd::vector2_int(0, 1);
		newbie.weight = 1.0f;
		samples.push_back(newbie);

		newbie.offset = simd::vector2_int(-1, 0);
		newbie.weight = 1.0f;
		samples.push_back(newbie);

		newbie.offset = simd::vector2_int(0, -1);
		newbie.weight = 1.0f;
		samples.push_back(newbie);

		//smoothing distance map
		const int shore_dist_smooth_iterations = 2;
		const int ocean_dist_smooth_iterations = 0;
		const int intensity_smooth_iterations = 5;

		int max_smooth_iterations = std::max(shore_dist_smooth_iterations, ocean_dist_smooth_iterations);
		max_smooth_iterations = std::max(max_smooth_iterations, intensity_smooth_iterations);

		for (int i = 0; i < max_smooth_iterations; i++)
		{
			for (int y = 0; y < tex_size.y; y++)
			{
				for (int x = 0; x < tex_size.x; x++)
				{
					auto curr_point = simd::vector2_int(x, y);
					ping[x + y * tex_size.x] = coord_data.Get(curr_point);

					float sum_shore_dist = 0.0f;
					float sum_ocean_dist = 0.0f;
					float sum_intensity = 0.0f;
					float sum_weight = 0.0f;

					for (int sample_index = 0; sample_index < samples.size(); sample_index++)
					{
						simd::vector2_int offset_point = curr_point + samples[sample_index].offset;
						if (offset_point.x < 0 || offset_point.x >= tex_size.x ||
							offset_point.y < 0 || offset_point.y >= tex_size.y)
						{
							continue;
						}
						sum_shore_dist += coord_data.Get(offset_point).r * samples[sample_index].weight;
						sum_ocean_dist += coord_data.Get(offset_point).b * samples[sample_index].weight;
						sum_intensity += coord_data.Get(offset_point).a * samples[sample_index].weight;
						//sum_longitude += coord_data[offset_point.x + offset_point.y * coastline_texture_width].g * weights[point_index];
						sum_weight += samples[sample_index].weight;
					}
					ping[x + y * tex_size.x] = coord_data.Get(simd::vector2_int(x, y));

					float avg_shore_dist = sum_shore_dist / sum_weight;
					float avg_ocean_dist = sum_ocean_dist / sum_weight;
					float avg_intensity = sum_intensity / sum_weight;
					if (i < shore_dist_smooth_iterations)
						ping[x + y * tex_size.x].r = avg_shore_dist;
					if (i < ocean_dist_smooth_iterations)
						ping[x + y * tex_size.x].b = avg_ocean_dist;
					if (i < intensity_smooth_iterations)
						ping[x + y * tex_size.x].a = avg_intensity;
					//coord_data[x + y * coastline_texture_width].g = avg_longitude;
					//coord_data[x + y * coastline_texture_width].b = avg_dist;
				}
			}
			for (int y = 0; y < tex_size.y; y++)
			{
				for (int x = 0; x < tex_size.x; x++)
				{
					coord_data.Get(simd::vector2_int(x, y)) = ping[x + y * tex_size.x];
				}
			}
		}
	}

	static void ProcessTrajectory(LockedData<Color128> coord_data, simd::vector2_int tex_size, simd::vector2 scene_size, const PhaseLine &deep_phaseline)
	{
		std::vector<ShoreGradients> world_gradients;
		world_gradients.resize(tex_size.x * tex_size.y);
		for (int y = 0; y < tex_size.y; y++)
		{
			for (int x = 0; x < tex_size.x; x++)
			{
				if (x == 0 || y == 0 || x == tex_size.x - 1 || y == tex_size.y - 1)
				{
					world_gradients[x + y * tex_size.x].shore_gradient = Vector2d(0.0f, 0.0f);
					world_gradients[x + y * tex_size.x].ocean_gradient = Vector2d(0.0f, 0.0f);
				}
				else
				{
					world_gradients[x + y * tex_size.x] = GetWorldDistGradient(coord_data, simd::vector2_int(x, y), tex_size, scene_size);
				}
			}
		}

		auto ColorGetter = [&](simd::vector2_int tex_pos) -> Color128
		{
			return coord_data.Get(tex_pos);
		};

		auto GradientGetter = [&](simd::vector2_int tex_pos) -> ShoreGradients
		{
			return world_gradients[tex_pos.x + tex_pos.y * tex_size.x];
		};

		auto Clamper = [&](simd::vector2_int tex_pos)
		{
			return simd::vector2_int(
				std::min(tex_size.x - 1, std::max(0, tex_pos.x)),
				std::min(tex_size.y - 1, std::max(0, tex_pos.y)));
		};

		auto InterpolateColor = [&](Vector2d tex_pos) -> Color128
		{
			return InterpolateValue<Color128, decltype(ColorGetter)>(ColorGetter, Clamper, tex_pos);
		};

		auto InterpolateWorldGradient = [&](Vector2d tex_pos) -> ShoreGradients
		{
			return InterpolateValue<ShoreGradients, decltype(GradientGetter)>(GradientGetter, Clamper, tex_pos);
		};

		for (int y = 1; y < tex_size.y - 1; y++)
		{
			for (int x = 1; x < tex_size.x - 1; x++)
			{
				simd::vector2_int curr_point = simd::vector2_int(x, y);
				Color128 curr_color = coord_data.Get(curr_point);
				Vector2d curr_world_point = Vector2d(
					float(x + 0.5f) / float(tex_size.x) * scene_size.x,
					float(y + 0.5f) / float(tex_size.y) * scene_size.y
				);

				float intensity = 1.0f;

				//shadows
				{
					float world_travel_dist = 0.0f;
					Vector2d tex_point = Vector2d(float(curr_point.x), float(curr_point.y));
					ShoreGradients world_gradients = InterpolateWorldGradient(tex_point);
					//if (world_gradients.ocean_gradient.dot(world_gradients.shore_gradient) < 0.0f)
					//{
					//	intensity = 0.0f;
					//}

					float world_shore_dist = (1.0f - InterpolateColor(tex_point).r) / dist_to_color;
					float max_delta_dist = 0.0f;

					for (int i = 0; i < 100; i++)
					{
						Vector2d world_point = Vector2d(
							float(tex_point.x + 0.5f) / float(tex_size.x) * scene_size.x,
							float(tex_point.y + 0.5f) / float(tex_size.y) * scene_size.y
						);

						ShoreGradients world_gradients = InterpolateWorldGradient(tex_point);
						float eps = 1e-7f;
						/*if (world_gradients.ocean_gradient.lengthsq() < eps * eps)
						{
							intensity *= 0.0f;
							break;
						}*/
						float step = 1.0f;
						//intensity *= exp(-attenuation * 0.2f * step);
						if (InterpolateColor(tex_point).b < 0.0f ||
							tex_point.x <= 0 || tex_point.y <= 0 ||
							tex_point.x >= tex_size.x - 1 || tex_point.y >= tex_size.y - 1)
							break;

						Vector2d tex_gradient = Vector2d(
							float(world_gradients.ocean_gradient.x) * float(tex_size.x) / scene_size.x,
							float(world_gradients.ocean_gradient.y) * float(tex_size.y) / scene_size.y
						);

						if (i >= 2)
						{
							/*float world_travel_dist = float(i) * step * scene_size.x / float(coastline_texture_width);*/
							float world_shore_dist = (1.0f - InterpolateColor(tex_point).r) / dist_to_color;

							if (world_shore_dist < 0.0f)
							{
								intensity *= 0.0f;
								break;
							}
						}


						tex_point += tex_gradient.normalize() * step;

						world_travel_dist += step * scene_size.x / float(tex_size.x);
						world_shore_dist = (1.0f - InterpolateColor(tex_point).r) / dist_to_color;


						float max_dist_cos = 0.4f;
						float delta_dist = world_travel_dist * max_dist_cos - world_shore_dist;
						max_delta_dist = std::max(delta_dist, max_delta_dist);
					}
					intensity *= exp(-std::max(0.0f, max_delta_dist) * 0.01f);
				}

				float trajectory_world_len = 0.0f;
				float world_dist_offset = -1000.0f;
				{
					Vector2d tex_point = Vector2d(float(curr_point.x), float(curr_point.y));
					ShoreGradients world_gradients = InterpolateWorldGradient(tex_point);

					float step_size = 1.0f;
					for (int i = 0; i < 100; i++)
					{
						Vector2d world_point = Vector2d(
							float(tex_point.x + 0.5f) / float(tex_size.x) * scene_size.x,
							float(tex_point.y + 0.5f) / float(tex_size.y) * scene_size.y
						);

						ShoreGradients world_gradients = InterpolateWorldGradient(tex_point);
						float eps = 1e-7f;

						Vector2d tex_gradient = Vector2d(
							float(world_gradients.ocean_gradient.x) * float(tex_size.x) / scene_size.x,
							float(world_gradients.ocean_gradient.y) * float(tex_size.y) / scene_size.y
						);

						float world_offset_dist = (1.0f - InterpolateColor(tex_point).r) / dist_to_color + world_dist_offset;
						float world_step_size = world_offset_dist * 0.5f;
						float tex_step_size = world_step_size * float(tex_size.x) / scene_size.x;

						Vector2d next_tex_point = tex_point;
						if (tex_gradient.length() > 0.0f)
							next_tex_point += -tex_gradient.normalize() * tex_step_size;


						tex_point = next_tex_point;
						trajectory_world_len += world_step_size;
					}
				}
				coord_data.Get(curr_point).g = trajectory_world_len * dist_to_color;
				coord_data.Get(curr_point).a = intensity;
			}
		}
	}

	simd::vector4 Color128ToVec4(TexUtil::Color128 color)
	{
		return simd::vector4(color.r, color.g, color.b, color.a);
	}
	void ScreenspaceTracer::BuildShorelineTexture(Device::IDevice *device, simd::vector2 scene_size, simd::vector2_int scene_tiles_count, size_t texel_per_tile_count, float shoreline_offset)
	{
		bool deep_line_is_present = (deep_phaseline.GetPointsCount() > 0);

		int coastline_texture_width = scene_tiles_count.x * int(texel_per_tile_count);
		int coastline_texture_height = scene_tiles_count.y * int(texel_per_tile_count);
		water_draw_data.shoreline_coords_tex = Device::Texture::CreateTexture("Shoreline Coords", device, coastline_texture_width, coastline_texture_height, 1, Device::UsageHint::DEFAULT, Device::PixelFormat::A32B32G32R32F, Device::Pool::MANAGED_WITH_SYSTEMMEM, false, false, false);

		{
			simd::vector4 shoreline_size4;
			shoreline_size4.x = float(coastline_texture_width);
			shoreline_size4.y = float(coastline_texture_height);
			shoreline_size4.z = 0.0f;
			shoreline_size4.w = 0.0f;
			water_draw_data.shoreline_coords_tex_size = shoreline_size4;
		}

		/*std::vector<Color> tidal_data;
		tidal_data.resize(tidal_texture_width * tidal_texture_height);*/
		Device::LockedRect rect;
		water_draw_data.shoreline_coords_tex->LockRect(0, &rect, Device::Lock::DEFAULT);

		//using namespace WaveUtil;
		LockedData<Color128> coord_data (rect.pBits, rect.Pitch);

		if (rect.pBits)
		{
			this->dist_to_color = Renderer::dist_to_color;

			for(int y = 0; y < coastline_texture_height; y++)
			{
				for(int x = 0; x < coastline_texture_width; x++)
				{
					Vector2d planar_pos = Vector2d(
						float(x + 0.5f) / float(coastline_texture_width) * scene_size.x,
						float(y + 0.5f) / float(coastline_texture_height) * scene_size.y
					);

					PointPhase shallow_phase = shallow_phaseline.GetPointPhase(planar_pos, false);
					PointPhase rocky_phase = rocky_phaseline.GetPointPhase(planar_pos, false);
					PointPhase deep_phase = deep_phaseline.GetPointPhase(planar_pos, false);

					Color128 color;
					//float gray = ((x % 2) != (y % 2)) ? 1.0f : 0.0f;; 
					if (deep_line_is_present)
					{
						color.r = 1.0f - (shallow_phase.dist + shoreline_offset) * dist_to_color;
						color.g = deep_phase.longitude / (deep_phaseline.GetSumLongitude() + 1e-5f);
						color.b = 1.0f - deep_phase.dist * dist_to_color;
						color.a = std::min(deep_phase.intensity, shallow_phase.intensity);

						this->sum_longitude = deep_phaseline.GetSumLongitude();

						if (std::abs(shallow_phase.dist - rocky_phase.dist) < 10.0f)
						{
							color.a = -color.a; // rocky line
						}
					}
					else
					{
						float cave_line_offset = 100.0f + shoreline_offset; //1m offset
						color.r = 1.0f - (shallow_phase.dist + cave_line_offset) * dist_to_color;
						color.g = -1.0f;
						color.b = 0.0f;
						color.a = 1.0f;

						this->sum_longitude = 1.0f;
					}
					//color.a = deep_phase.intensity;
					coord_data.Get(simd::vector2_int(x, y)) = color;
				}
			}

			auto GetDist = [&](Color128 color)
			{
				return abs((1.0f - color.r) / this->dist_to_color);
			};



			if (deep_line_is_present)
			{
				SmoothDistanceMap(coord_data, simd::vector2_int(coastline_texture_width, coastline_texture_height));
				ProcessTrajectory(coord_data, simd::vector2_int(coastline_texture_width, coastline_texture_height), scene_size, deep_phaseline);
			}

			water_draw_data.shoreline_tex_data.resize(coastline_texture_width * coastline_texture_height);
			water_draw_data.shoreline_tex_data_size = simd::vector2_int(coastline_texture_width, coastline_texture_height);
			for (int y = 0; y < water_draw_data.shoreline_tex_data_size.y; y++)
			{
				for (int x = 0; x < water_draw_data.shoreline_tex_data_size.x; x++)
				{
					water_draw_data.shoreline_tex_data[x + y * water_draw_data.shoreline_tex_data_size.x] = Color128ToVec4(coord_data.Get(simd::vector2_int(x, y)));
				}
			}
		}

		water_draw_data.shoreline_coords_tex->UnlockRect(0);

		#ifdef PHYSICS_VISUALISATIONS
			//water_draw_data.shoreline_coords_tex->SaveToFile(L"coastline_coord_texture.dds", Device::ImageFileFormat::PNG);
		#endif
	}

	void ScreenspaceTracer::BuildRiverFlowmapTexture(Device::IDevice *device, simd::vector2 scene_size, simd::vector2_int scene_tiles_count, size_t texels_per_tile_count, float shoreline_offset, float initial_turbulence, bool wait_for_initialization)
	{
		int flowmap_texture_width = scene_tiles_count.x * int(texels_per_tile_count);
		int flowmap_texture_height = scene_tiles_count.y * int(texels_per_tile_count);

		auto usage = (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY);
		auto pool = Device::Pool::DEFAULT;

		this->river_flowmap_data.tex_size = simd::vector2_int(flowmap_texture_width, flowmap_texture_height);
		this->river_flowmap_data.texture = Device::Texture::CreateTexture("Flowmap", device, river_flowmap_data.tex_size.x, river_flowmap_data.tex_size.y, 1, usage, Device::PixelFormat::A32B32G32R32F, pool, false, false, false);
		this->river_flowmap_data.data.resize(river_flowmap_data.tex_size.x * river_flowmap_data.tex_size.y);
		std::fill(river_flowmap_data.data.begin(), river_flowmap_data.data.end(), TexUtil::Color128(0.0f, 0.0f, 0.0f, 0.0f));
		this->river_flowmap_data.version = 0;
		this->river_flowmap_data.is_running = true;
		this->river_flowmap_version =-1;

		this->flow_to_color = 10.0f;

		{
			river_flowmap_data.is_pending = true;
			if (wait_for_initialization)
			{
				river_flowmap_data.river_data = GetFlowmapNodeData(shoreline_offset, initial_turbulence);
				river_flowmap_data.fluid_system = InitializeFluidSystem(*river_flowmap_data.river_data.get());
			}

			Job2::System::Get().Add(Job2::Type::Idle, { Memory::Tag::Render, Job2::Profile::FlowMap, [&, shoreline_offset=shoreline_offset, wait_for_initialization=wait_for_initialization, initial_turbulence=initial_turbulence]()
			{
				if (!wait_for_initialization)
				{
					river_flowmap_data.river_data = GetFlowmapNodeData(shoreline_offset, initial_turbulence);
					river_flowmap_data.fluid_system = InitializeFluidSystem(*river_flowmap_data.river_data.get());
				}

				for (int i = 0; i < 10; i++)
					FluidSystemIteration(i, *river_flowmap_data.river_data.get(), river_flowmap_data.fluid_system.get());

				river_flowmap_data.river_data.reset();
				river_flowmap_data.fluid_system.reset();
				river_flowmap_data.is_pending = false;
			} });
		}
	}

	std::unique_ptr<Physics::FluidSystem> ScreenspaceTracer::InitializeFluidSystem(RiverFlowmapData& data)
	{
		auto& node_dynamic_data = data.node_dynamic_data;
		auto& node_static_data = data.node_static_data;
		auto& shore_dist_data = data.shore_dist_data;

		auto fluid_system = Physics::FluidSystem::Create();
		fluid_system->SetSolverParams(5);
		fluid_system->SetSize(Physics::Vector2i(river_flowmap_data.tex_size.x, river_flowmap_data.tex_size.y), 
							  Physics::Vector2f(scene_size.x, scene_size.y));
		fluid_system->SetDynamicData(node_dynamic_data.data());
		fluid_system->SetStaticData(node_static_data.data()); // TODO: This is taking a long time on PS4 depending on the grid size (up to 2s sometimes), and should be interruptible.
		fluid_system->GetDynamicData(node_dynamic_data.data());
		return fluid_system;
	}

	std::unique_ptr<ScreenspaceTracer::RiverFlowmapData> ScreenspaceTracer::GetFlowmapNodeData(float shoreline_offset, float initial_turbulence)
	{
		auto data = std::make_unique<RiverFlowmapData>();

		auto& node_dynamic_data = data->node_dynamic_data;
		auto& node_static_data = data->node_static_data;
		auto& shore_dist_data = data->shore_dist_data;

		node_dynamic_data.resize(river_flowmap_data.tex_size.x * river_flowmap_data.tex_size.y);
		node_static_data.resize(river_flowmap_data.tex_size.x * river_flowmap_data.tex_size.y);
		shore_dist_data.resize(river_flowmap_data.tex_size.x * river_flowmap_data.tex_size.y);

		PointLine &base_line = river_bank_phaseline.GetBaseLine();

		std::vector<Physics::Vector2f> points;
		std::vector<size_t> segment_points_count;

		Physics::AABB2f scene_aabb;
		scene_aabb.box_point1 = Physics::Vector2f(0.0f, 0.0f);
		scene_aabb.box_point2 = Physics::Vector2f(this->scene_size.x, this->scene_size.y);
		Physics::QuadTree quad_tree;
		quad_tree.LoadPointLine(base_line, scene_aabb);

		float blocker_extra_width = 0.0f;
		/*for (int y = 0; y < flowmap_tex_size.y; y++)
		{
			for (int x = 0; x < flowmap_tex_size.x; x++)
			{
				Vector2f planar_pos = Vector2f(
					float(x + 0.5f) / float(flowmap_tex_size.x) * this->scene_size.x,
					float(y + 0.5f) / float(flowmap_tex_size.y) * this->scene_size.y
				);

				Physics::QuadTree::PointPhase phase = quad_tree.GetPointPhase(planar_pos);
				TexUtil::Color128 color;
				float param = phase.dist / 1000.0f;
				color.r = param > 0.0f ? param : 0.0f;
				color.g = param < 0.0f ? -param : 0.0f;
				color.b = 0.0f;
				color.a = 1.0f;

				flowmap_data.data[x + y * flowmap_tex_size.x] = color;
			}
		}
		flowmap_data.version++;
		return;*/


		for (int y = 0; (y < river_flowmap_data.tex_size.y) && river_flowmap_data.is_running; y++)
		{
			for (int x = 0; (x < river_flowmap_data.tex_size.x) && river_flowmap_data.is_running; x++)
			{
				float min_dist = 1e6f;
				float closest_point_longitude = 0.0f;


				auto &dynamic_data = node_dynamic_data[x + y * river_flowmap_data.tex_size.x];
				dynamic_data.velocity = Physics::Vector2f(0.0f, 0.0f);
				dynamic_data.pressure = 0.0f;

				//auto &static_data = node_static_data[x + y * flowmap_tex_size.x];
				auto &dist_data = shore_dist_data[x + y * river_flowmap_data.tex_size.x];

				//static_data.nodeType = Physics::FluidSystem::NodeTypes::Free;
				dynamic_data.pressure = 0.0f;
				//node_state.velocity = Vector2f(x % 10 - 5, y % 10 - 5) * (10.0f / 10.0f);
				//dynamic_data.velocity = Vector2f(float(rand()) / float(RAND_MAX) - 0.5f, float(rand()) / float(RAND_MAX) - 0.5f) * 200.0f;
				dynamic_data.velocity += Physics::Vector2f(sin(y / 5.0f) + sin(y / 3.0f + x / 4.0f), sin(x / 2.0f) + sin(x / 5.0f + y / 6.0f) * 0.0f) * 10.0f * initial_turbulence;

				//PointPhase river_phase = river_bank_phaseline.GetPointPhase(planar_pos, false);

				Vector2d dist_node_pos = Vector2d( //dist node is in corner of the grid, not in the middle of cell
					float(x) / float(river_flowmap_data.tex_size.x) * this->scene_size.x,
					float(y) / float(river_flowmap_data.tex_size.y) * this->scene_size.y
				);

				Physics::QuadTree::PointPhase river_phase = quad_tree.GetPointPhase(Physics::Vector2f(dist_node_pos.x, dist_node_pos.y));


				dist_data = river_phase.dist + shoreline_offset;
				/*if (river_phase.dist < blocker_extra_width) //-50.0f
				{
					static_data.nodeType = Physics::FluidSystem::NodeTypes::FixedVelocity;
					dynamic_data.pressure = 0.0f;
					dynamic_data.velocity = Vector2f::zero();
				}*/

				/*if (x < 5 || y < 5 || x >= flowmap_tex_size.x - 5 || y >= flowmap_tex_size.y - 5)
				{
					dynamic_data.velocity = Vector2f(0.0f, 0.0f); //in cm/s
					static_data.nodeType = Physics::FluidSystem::NodeTypes::FixedVelocity;
				}*/


				dynamic_data.substance = (((x % 80) < 40) ^ ((y % 80) < 40)) ? 1.0f : 0.0f;
			}
		}

		if (!river_flowmap_data.is_running)
			return data;

		for (auto &constraint : water_blocking_constraints)
		{
			if (!river_flowmap_data.is_running)
				break;

			Physics::Vector2f constraint_pos = Physics::Vector2f(constraint.world_pos.x, constraint.world_pos.y);
			Physics::Vector2i int_center = Physics::Vector2i(
				int(constraint.world_pos.x / this->scene_size.x * float(river_flowmap_data.tex_size.x)),
				int(constraint.world_pos.y / this->scene_size.y * float(river_flowmap_data.tex_size.y)));
			int int_radius = int(river_flowmap_data.tex_size.x * (constraint.radius / this->scene_size.x)) + 10;
			int minx = Clamp(int_center.x - int_radius, 0, river_flowmap_data.tex_size.x - 1);
			int miny = Clamp(int_center.y - int_radius, 0, river_flowmap_data.tex_size.y - 1);
			int maxx = Clamp(int_center.x + int_radius, 0, river_flowmap_data.tex_size.x - 1);
			int maxy = Clamp(int_center.y + int_radius, 0, river_flowmap_data.tex_size.y - 1);
			for (int y = miny; (y <= maxy) && river_flowmap_data.is_running; y++)
			{
				for (int x = minx; (x <= maxx) && river_flowmap_data.is_running; x++)
				{
					Physics::Vector2f dist_node_pos = Physics::Vector2f(
						float(x) / float(river_flowmap_data.tex_size.x) * this->scene_size.x,
						float(y) / float(river_flowmap_data.tex_size.y) * this->scene_size.y
					);
					float length = (constraint_pos - dist_node_pos).Len();

					auto &dist_data = shore_dist_data[x + y * river_flowmap_data.tex_size.x];

					if (constraint.blocking > 0)
					{
						dist_data = std::min(dist_data, length - constraint.radius);
					}
					else
					{
						dist_data = std::max(dist_data, -(length - constraint.radius));
					}
					/*if (length < constraint.radius && static_data.nodeType == Physics::FluidSystem::NodeTypes::Free)
					{
						if (constraint.blocking > 0)
							static_data.nodeType = Physics::FluidSystem::NodeTypes::FixedVelocity;
						else
							static_data.nodeType = Physics::FluidSystem::NodeTypes::Free;
					}*/
				}
			}
		}

		if (!river_flowmap_data.is_running)
			return data;

		for (auto &constraint : water_source_constraints)
		{
			if (!river_flowmap_data.is_running)
				break;

			Physics::Vector2f constraint_pos = Physics::Vector2f(constraint.world_pos.x, constraint.world_pos.y);
			Physics::Vector2i int_center = Physics::Vector2i(
				int(constraint.world_pos.x / this->scene_size.x * float(river_flowmap_data.tex_size.x)),
				int(constraint.world_pos.y / this->scene_size.y * float(river_flowmap_data.tex_size.y)));
			int int_radius = int(river_flowmap_data.tex_size.x * (fabs(constraint.radius) / this->scene_size.x)) + 10;
			int minx = Clamp(int_center.x - int_radius, 0, river_flowmap_data.tex_size.x - 1);
			int miny = Clamp(int_center.y - int_radius, 0, river_flowmap_data.tex_size.y - 1);
			int maxx = Clamp(int_center.x + int_radius, 0, river_flowmap_data.tex_size.x - 1);
			int maxy = Clamp(int_center.y + int_radius, 0, river_flowmap_data.tex_size.y - 1);
			for (int y = miny; (y <= maxy) && river_flowmap_data.is_running; y++)
			{
				for (int x = minx; (x <= maxx) && river_flowmap_data.is_running; x++)
				{
					{
						Physics::Vector2f dist_node_pos = Physics::Vector2f(
							float(x) / float(river_flowmap_data.tex_size.x) * this->scene_size.x,
							float(y) / float(river_flowmap_data.tex_size.y) * this->scene_size.y
						);
						float length = (constraint_pos - dist_node_pos).Len();

						auto &dist_data = shore_dist_data[x + y * river_flowmap_data.tex_size.x];
						dist_data = std::max(dist_data, -(length - constraint.radius));
					}
					{
						Physics::Vector2f pressure_node_pos = Physics::Vector2f(
							float(x + 0.5f) / float(river_flowmap_data.tex_size.x) * this->scene_size.x,
							float(y + 0.5f) / float(river_flowmap_data.tex_size.y) * this->scene_size.y
						);
						float length = (constraint_pos - pressure_node_pos).Len();

						auto &static_data = node_static_data[x + y * river_flowmap_data.tex_size.x];

						if (length < constraint.radius)
						{
							//float intensity = 1.0f / 3.0f;
							float intensity = std::max(0.0f, 1.0f - (length / fabs(constraint.radius)));
							static_data.source += constraint.source * intensity;
						}
					}
				}
			}
		}

		if (!river_flowmap_data.is_running)
			return data;

		for (int y = 0; (y < river_flowmap_data.tex_size.y) && river_flowmap_data.is_running; y++)
		{
			for (int x = 0; (x < river_flowmap_data.tex_size.x) && river_flowmap_data.is_running; x++)
			{
				auto &dynamic_data = node_dynamic_data[x + y * river_flowmap_data.tex_size.x];
				auto &static_data = node_static_data[x + y * river_flowmap_data.tex_size.x];
				auto &dist_data = shore_dist_data[x + y * river_flowmap_data.tex_size.x];
				dist_data -= blocker_extra_width;
				if (dist_data < 0.0f || x < 3 || y < 3 || x >= river_flowmap_data.tex_size.x - 4 || y >= river_flowmap_data.tex_size.y - 4)
				{
					static_data.nodeType = Physics::FluidSystem::NodeTypes::FixedVelocity;
					static_data.source = 0.0f;
					dynamic_data.substance = 0.5f;
					for (int offset_x = 0; offset_x < 2; offset_x++)
					{
						for (int offset_y = 0; offset_y < 2; offset_y++)
						{
							int velocity_x = x + offset_x;
							int velocity_y = y + offset_y;
							if (velocity_x < river_flowmap_data.tex_size.x && velocity_y < river_flowmap_data.tex_size.y)
								node_dynamic_data[velocity_x + velocity_y * river_flowmap_data.tex_size.x].velocity = Physics::Vector2f(0.0f, 0.0f);
						}
					}
				}
				else
				{
					static_data.nodeType = Physics::FluidSystem::NodeTypes::Free;
				}
			}
		}

		return data;
	}

	void ScreenspaceTracer::FluidSystemIteration(int i, RiverFlowmapData& data, Physics::FluidSystem* fluid_system)
	{
		if (!river_flowmap_data.is_running)
			return;

		PROFILE;

		auto& node_dynamic_data = data.node_dynamic_data;
		auto& node_static_data = data.node_static_data;
		auto& shore_dist_data = data.shore_dist_data;

			fluid_system->Update(0.01f);
			if (i % 10)
			{
				fluid_system->GetDynamicData(node_dynamic_data.data());

				for (int y = 0; y < (river_flowmap_data.tex_size.y) && river_flowmap_data.is_running; y++)
				{
					for (int x = 0; x < (river_flowmap_data.tex_size.x) && river_flowmap_data.is_running; x++)
					{
						auto &static_data = node_static_data[x + y * river_flowmap_data.tex_size.x];
						auto &dynamic_data = node_dynamic_data[x + y * river_flowmap_data.tex_size.x];
						//auto &dist_data = shore_dist_data[x + y * flowmap_tex_size.x];
						TexUtil::Color128 color;
						/*color.r = char(Clamp(node_state.velocity.x * flow_to_color - 0.5f, 0.0f, 1.0f) * 255);
						color.g = char(Clamp(node_state.velocity.y * flow_to_color - 0.5f, 0.0f, 1.0f) * 255);;*/

						float curl = 0.0f;
						float avg_dist = 0.0f;
						if (x > 0 && y > 0 && x < river_flowmap_data.tex_size.x - 1 && y < river_flowmap_data.tex_size.y - 1)
						{
							float step_size = this->scene_size.x / float(river_flowmap_data.tex_size.x);
							Physics::Vector2f x_gradient = (node_dynamic_data[x + 1 + y * river_flowmap_data.tex_size.x].velocity - node_dynamic_data[x - 1 + y * river_flowmap_data.tex_size.x].velocity) / (2.0f * step_size);
							Physics::Vector2f y_gradient = (node_dynamic_data[x + (y + 1) * river_flowmap_data.tex_size.x].velocity - node_dynamic_data[x + (y - 1) * river_flowmap_data.tex_size.x].velocity) / (2.0f * step_size);
							curl = x_gradient.y - y_gradient.x;

							avg_dist = (
								shore_dist_data[x + y * river_flowmap_data.tex_size.x] +
								shore_dist_data[x + 1 + y * river_flowmap_data.tex_size.x] +
								shore_dist_data[(x + 1) + (y + 1) * river_flowmap_data.tex_size.x] +
								shore_dist_data[x + (y + 1) * river_flowmap_data.tex_size.x]) * 0.25f;
						}

						color.r = dynamic_data.velocity.x;
						color.g = dynamic_data.velocity.y;
						color.b = static_data.source;// dynamic_data.substance;
						color.a = avg_dist;
						/*if (static_data.nodeType == Physics::FluidSystem::NodeTypes::FixedPressure)
							color.a = 0.5f;
						if (static_data.nodeType == Physics::FluidSystem::NodeTypes::FixedVelocity)
							color.a = 1.0f;*/
						river_flowmap_data.data[x + y * river_flowmap_data.tex_size.x] = color;
					}
				}
				river_flowmap_data.version++;
			}
		}

	void ScreenspaceTracer::BuildFogFlowmapTexture(Device::IDevice *device, const Scene::FogBlockingData &fog_blocking_data)
	{
		typedef typename Physics::Vector2<int> Vector2i;
		using typename Physics::Vector2f;

		int nodes_per_texel_count = 1;
		int flowmap_texture_width = fog_blocking_data.size.x / nodes_per_texel_count;
		int flowmap_texture_height = fog_blocking_data.size.y / nodes_per_texel_count;

		auto usage = (Device::UsageHint)((unsigned)Device::UsageHint::DYNAMIC | (unsigned)Device::UsageHint::WRITEONLY);
		auto pool = Device::Pool::DEFAULT;

		this->fog_flowmap_data.tex_size = simd::vector2_int(flowmap_texture_width, flowmap_texture_height);
		this->fog_flowmap_data.texture = Device::Texture::CreateTexture("FogFlowmap", device, fog_flowmap_data.tex_size.x, fog_flowmap_data.tex_size.y, 1, usage, Device::PixelFormat::A32B32G32R32F, pool, false, false, false);
		this->fog_flowmap_data.data.resize(fog_flowmap_data.tex_size.x * fog_flowmap_data.tex_size.y);
		std::fill(fog_flowmap_data.data.begin(), fog_flowmap_data.data.end(), TexUtil::Color128(0.0f, 0.0f, 0.0f, 0.0f));
		this->fog_flowmap_data.version = 0;
		this->fog_flowmap_data.is_running = true;
		this->fog_flowmap_version = -1;

		this->flow_to_color = 10.0f;

		BuildFogFlowmap(fog_blocking_data);
	}

	void ScreenspaceTracer::BuildFogFlowmap(const Scene::FogBlockingData &fog_blocking_data)
	{
		float initial_turbulence = 0.01f;
		std::vector<Physics::FluidSystem::NodeDynamicData> node_dynamic_data;
		std::vector<Physics::FluidSystem::NodeStaticData> node_static_data;

		node_dynamic_data.resize(fog_flowmap_data.tex_size.x * fog_flowmap_data.tex_size.y);
		node_static_data.resize(fog_flowmap_data.tex_size.x * fog_flowmap_data.tex_size.y);
		
		int nodes_per_texel_count = fog_blocking_data.size.x / fog_flowmap_data.tex_size.x;

		for (int y = 0; y < fog_flowmap_data.tex_size.y; y++)
		{
			for (int x = 0; x < fog_flowmap_data.tex_size.x; x++)
			{
				if (!river_flowmap_data.is_running)
					return;

				auto &dynamic_data = node_dynamic_data[x + y * fog_flowmap_data.tex_size.x];
				auto &static_data = node_static_data[x + y * fog_flowmap_data.tex_size.x];

				int map_x = x * nodes_per_texel_count;
				int map_y = y * nodes_per_texel_count;
				unsigned char dist = fog_blocking_data.blocking_data[map_x + map_y * fog_blocking_data.size.x];

				dynamic_data.pressure = 0.0f;
				dynamic_data.velocity += Physics::Vector2f(sin(y / 5.0f) + sin(y / 3.0f + x / 4.0f), sin(x / 2.0f) + sin(x / 5.0f + y / 6.0f) * 0.0f) * 10.0f * initial_turbulence;

				static_data.source = 0.0f;
				if (dist > 5)
				{
					static_data.nodeType = Physics::FluidSystem::NodeTypes::FixedVelocity;
					dynamic_data.substance = 0.5f;
					dynamic_data.velocity = Physics::Vector2f(0.0f, 0.0f);
				}
				else
				{
					static_data.nodeType = Physics::FluidSystem::NodeTypes::Free;
				}
			}
		}

		for (size_t index = 0; index < fog_blocking_data.source_constraints_count; index++)
		{
			const auto constraint = fog_blocking_data.source_constraints[index];

			if (!fog_flowmap_data.is_running)
				break;

			Physics::Vector2f constraint_pos = Physics::Vector2f(constraint.world_pos.x, constraint.world_pos.y);
			Physics::Vector2i int_center = Physics::Vector2i(
				int(constraint.world_pos.x / this->scene_size.x * float(fog_flowmap_data.tex_size.x)),
				int(constraint.world_pos.y / this->scene_size.y * float(fog_flowmap_data.tex_size.y)));
			int int_radius = int(fog_flowmap_data.tex_size.x * (fabs(constraint.radius) / this->scene_size.x)) + 10;
			int minx = Clamp(int_center.x - int_radius, 0, fog_flowmap_data.tex_size.x - 1);
			int miny = Clamp(int_center.y - int_radius, 0, fog_flowmap_data.tex_size.y - 1);
			int maxx = Clamp(int_center.x + int_radius, 0, fog_flowmap_data.tex_size.x - 1);
			int maxy = Clamp(int_center.y + int_radius, 0, fog_flowmap_data.tex_size.y - 1);
			for (int y = miny; (y <= maxy) && fog_flowmap_data.is_running; y++)
			{
				for (int x = minx; (x <= maxx) && fog_flowmap_data.is_running; x++)
				{

					{
						Physics::Vector2f pressure_node_pos = Physics::Vector2f(
							float(x + 0.5f) / float(fog_flowmap_data.tex_size.x) * this->scene_size.x,
							float(y + 0.5f) / float(fog_flowmap_data.tex_size.y) * this->scene_size.y
						);
						float length = (constraint_pos - pressure_node_pos).Len();

						auto &static_data = node_static_data[x + y * fog_flowmap_data.tex_size.x];

						if (length < constraint.radius)
						{
							//float intensity = 1.0f / 3.0f;
							float intensity = std::max(0.0f, 1.0f - (length / fabs(constraint.radius)));
							static_data.source += constraint.source * intensity;
							static_data.nodeType = static_data.nodeType = Physics::FluidSystem::NodeTypes::Free;
						}
					}
				}
			}
		}

		auto fluid_system = Physics::FluidSystem::Create();

		fluid_system->SetSolverParams(5);
		fluid_system->SetSize(Physics::Vector2i(fog_flowmap_data.tex_size.x, fog_flowmap_data.tex_size.y), Physics::Vector2f(this->scene_size.x, this->scene_size.y));

		/*std::ofstream file_dump("flow_data.dat", std::ios::binary);
		file_dump.write((char*)&(flowmap_tex_size.x), sizeof(flowmap_tex_size.x));
		file_dump.write((char*)&(flowmap_tex_size.y), sizeof(flowmap_tex_size.y));
		file_dump.write((char*)&(this->scene_size.x), sizeof(scene_size.x));
		file_dump.write((char*)&(this->scene_size.y), sizeof(scene_size.y));
		file_dump.write((char*)node_dynamic_data.data(), node_dynamic_data.size() * sizeof(Physics::FluidSystem::NodeDynamicData));
		file_dump.write((char*)node_static_data.data(), node_static_data.size() * sizeof(Physics::FluidSystem::NodeStaticData));*/

		fluid_system->SetDynamicData(node_dynamic_data.data());
		fluid_system->SetStaticData(node_static_data.data()); // TODO: This is taking a long time on PS4 depending on the grid size (up to 2s sometimes), and should be interruptible.


		fluid_system->GetDynamicData(node_dynamic_data.data());

		for (int i = 0; i < 10 && fog_flowmap_data.is_running; i++)
		{
			fluid_system->Update(0.01f);
			if (i % 10)
			{
				fluid_system->GetDynamicData(node_dynamic_data.data());

				for (int y = 0; y < (fog_flowmap_data.tex_size.y) && fog_flowmap_data.is_running; y++)
				{
					for (int x = 0; x < (fog_flowmap_data.tex_size.x) && fog_flowmap_data.is_running; x++)
					{
						auto &static_data = node_static_data[x + y * fog_flowmap_data.tex_size.x];
						auto &dynamic_data = node_dynamic_data[x + y * fog_flowmap_data.tex_size.x];
						//auto &dist_data = shore_dist_data[x + y * flowmap_tex_size.x];
						TexUtil::Color128 color;
						/*color.r = char(Clamp(node_state.velocity.x * flow_to_color - 0.5f, 0.0f, 1.0f) * 255);
						color.g = char(Clamp(node_state.velocity.y * flow_to_color - 0.5f, 0.0f, 1.0f) * 255);;*/

						float curl = 0.0f;
						float avg_dist = 0.0f;
						if (x > 0 && y > 0 && x < fog_flowmap_data.tex_size.x - 1 && y < fog_flowmap_data.tex_size.y - 1)
						{
							float step_size = this->scene_size.x / float(fog_flowmap_data.tex_size.x);
							Physics::Vector2f x_gradient = (node_dynamic_data[x + 1 + y * fog_flowmap_data.tex_size.x].velocity - node_dynamic_data[x - 1 + y * fog_flowmap_data.tex_size.x].velocity) / (2.0f * step_size);
							Physics::Vector2f y_gradient = (node_dynamic_data[x + (y + 1) * fog_flowmap_data.tex_size.x].velocity - node_dynamic_data[x + (y - 1) * fog_flowmap_data.tex_size.x].velocity) / (2.0f * step_size);
							curl = x_gradient.y - y_gradient.x;

						}

						color.r = dynamic_data.velocity.x;
						color.g = dynamic_data.velocity.y;
						color.b = static_data.source;// dynamic_data.substance;
						color.a = avg_dist;
						/*if (static_data.nodeType == Physics::FluidSystem::NodeTypes::FixedPressure)
							color.a = 0.5f;
						if (static_data.nodeType == Physics::FluidSystem::NodeTypes::FixedVelocity)
							color.a = 1.0f;*/
						fog_flowmap_data.data[x + y * fog_flowmap_data.tex_size.x] = color;
					}
				}
				fog_flowmap_data.version++;
			}
		}

		{
			Device::LockedRect rect;
			fog_flowmap_data.texture->LockRect(0, &rect, Device::Lock::DEFAULT);

			if (rect.pBits)
			{
				TexUtil::LockedData<TexUtil::Color128> flow_data(rect.pBits, rect.Pitch);

				this->flow_to_color = 10.0f;

				for (int y = 0; y < fog_flowmap_data.tex_size.y; y++)
				{
					for (int x = 0; x < fog_flowmap_data.tex_size.x; x++)
					{
						auto &node_state = node_dynamic_data[x + y * fog_flowmap_data.tex_size.x];

						TexUtil::Color128 color;
						color.r = node_state.velocity.x;
						color.g = node_state.velocity.y;
						color.b = node_state.substance;
						color.a = 0.0f;
						flow_data.Get(simd::vector2_int(x, y)) = color;
					}
				}
			}

			fog_flowmap_data.texture->UnlockRect(0);
		#ifdef PHYSICS_VISUALISATIONS
			fog_flowmap_data.texture->SaveToFile(L"fog_flowmap_unsolved.dds", Device::ImageFileFormat::PNG);
		#endif
		}
	}

	void ScreenspaceTracer::LoadSceneData(
		Device::IDevice *device,
		simd::vector2 scene_size,
		simd::vector2_int scene_tiles_count,
		const Renderer::Scene::OceanData &ocean_data,
		const Renderer::Scene::RiverData &river_data,
		const Renderer::Scene::FogBlockingData &fog_blocking_data)
	{
		PROFILE;
		StopAsyncThreads();

		this->scene_size = scene_size;

		//if (ocean_data)
		{
			this->shallow_phaseline.LoadLineData(*ocean_data.shallow_shoreline);
			this->rocky_phaseline.LoadLineData(*ocean_data.rocky_shoreline);

			if (ocean_data.deep_shoreline)
				this->deep_phaseline.LoadLineData(*ocean_data.deep_shoreline, 0.5f, 3000, 5000.0f);

			BuildShorelineTexture(device, scene_size, scene_tiles_count, ocean_data.texels_per_tile_count, ocean_data.shoreline_offset);
		}

		//if (river_data)
		{
			this->river_bank_phaseline.LoadLineData(*river_data.river_bank_line);

			water_flow_constraints.clear();
			for (size_t i = 0; i < river_data.flow_constraints_count; i++)
			{
				water_flow_constraints.push_back(river_data.flow_constraints[i]);
			}
			water_source_constraints.clear();
			for (size_t i = 0; i < river_data.source_constraints_count; i++)
			{
				water_source_constraints.push_back(river_data.source_constraints[i]);
			}
			water_blocking_constraints.clear();
			for (size_t i = 0; i < river_data.blocking_constraints_count; i++)
			{
				water_blocking_constraints.push_back(river_data.blocking_constraints[i]);
			}

			BuildRiverFlowmapTexture(device, scene_size, scene_tiles_count, river_data.texels_per_tile_count, ocean_data.shoreline_offset, river_data.initial_turbulence, river_data.wait_for_initialization);
		}

		{
			//BuildFogFlowmapTexture(device, fog_blocking_data);
		}
	}

	simd::vector4 ScreenspaceTracer::GetSceneData(simd::vector2 planar_pos)
	{
		auto tex_size = water_draw_data.shoreline_tex_data_size;
		auto ColorGetter = [&](simd::vector2_int tex_pos) -> simd::vector4
		{
			if (tex_pos.x < tex_size.x && tex_pos.y < tex_size.y && tex_pos.x >= 0 && tex_pos.y >= 0)
				return water_draw_data.shoreline_tex_data[tex_pos.x + tex_size.x * tex_pos.y];
			return simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);
		};

		auto Clamper = [&](simd::vector2_int tex_pos)
		{
			return simd::vector2_int(
				std::min(tex_size.x - 1, std::max(0, tex_pos.x)),
				std::min(tex_size.y - 1, std::max(0, tex_pos.y)));
		};

		auto InterpolateColor = [&](Vector2d tex_pos) -> simd::vector4
		{
			return InterpolateValue<simd::vector4, decltype(ColorGetter)>(ColorGetter, Clamper, tex_pos);
		};

		auto scene_planar_size = simd::vector2(water_draw_data.scene_size.x, water_draw_data.scene_size.y);
		simd::vector2 tex_pos = planar_pos / scene_planar_size * simd::vector2(tex_size) - simd::vector2(0.5f, 0.5f);
		simd::vector4 col = InterpolateColor(Vector2d(tex_pos.x, tex_pos.y));
		return simd::vector4((1.0f - col.x) / dist_to_color, 0.0f, (1.0f - col.z) / dist_to_color, 0.0f);
	}


	void ScreenspaceTracer::UpdateAsyncResources()
	{
		PROFILE;

		using namespace TexUtil;

		if(river_flowmap_data.texture && this->river_flowmap_version != river_flowmap_data.version)
		{
			this->river_flowmap_version = river_flowmap_data.version;

			Device::LockedRect rect;
			river_flowmap_data.texture->LockRect(0, &rect, Device::Lock::DISCARD);

			if (rect.pBits)
			{
				LockedData<Color128> flow_data(rect.pBits, rect.Pitch);

				for (int y = 0; y < river_flowmap_data.tex_size.y; y++)
				{
					for (int x = 0; x < river_flowmap_data.tex_size.x; x++)
					{
						flow_data.Get(simd::vector2_int(x, y)) = river_flowmap_data.data[x + y * river_flowmap_data.tex_size.x];
					}
				}
			}

			river_flowmap_data.texture->UnlockRect(0);
			/*#ifdef PHYSICS_VISUALISATIONS
				river_flowmap_data.texture->SaveToFile(L"river_flowmap_solved.dds", Device::ImageFileFormat::PNG);
			#endif*/
		}

		if (fog_flowmap_data.texture && this->fog_flowmap_version != fog_flowmap_data.version)
		{
			this->fog_flowmap_version = fog_flowmap_data.version;

			Device::LockedRect rect;
			fog_flowmap_data.texture->LockRect(0, &rect, Device::Lock::DISCARD);

			if (rect.pBits)
			{
				LockedData<Color128> flow_data(rect.pBits, rect.Pitch);

				for (int y = 0; y < fog_flowmap_data.tex_size.y; y++)
				{
					for (int x = 0; x < fog_flowmap_data.tex_size.x; x++)
					{
						flow_data.Get(simd::vector2_int(x, y)) = fog_flowmap_data.data[x + y * fog_flowmap_data.tex_size.x];
					}
				}
			}

			fog_flowmap_data.texture->UnlockRect(0);
			#ifdef PHYSICS_VISUALISATIONS
			fog_flowmap_data.texture->SaveToFile(L"fog_flowmap_solved.dds", Device::ImageFileFormat::PNG);
			#endif
		}
	}

	void ScreenspaceTracer::StopAsyncThreads()
	{
		this->river_flowmap_data.is_running = false;

		while (river_flowmap_data.is_pending) {}
		}

	void RenderSource(const simd::matrix & view_projection, simd::color color, simd::vector2 pos, float radius, float intensity)
	{
		simd::vector3 world_pos = simd::vector3(pos.x, pos.y, -200.0f);
		simd::vector3 height = simd::vector3(0.0f, 0.0f, -intensity * 10.0f);
		float step = 1e-1f;
		for (float ang = 0.0f; ang < 2.0f * 3.1415f; ang += step)
		{
			simd::vector3 delta = simd::vector3(cosf(ang), sinf(ang), 0.0f) * radius;
			simd::vector3 next_delta = simd::vector3(cosf(ang + step), sinf(ang + step), 0.0f) * radius;
			
			simd::vector3 line_verts[2];
			line_verts[0] = world_pos + delta;
			line_verts[1] = world_pos + next_delta;
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, color.c());

			line_verts[0] = world_pos + delta;
			line_verts[1] = world_pos + delta + height;
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, color.c());

			line_verts[0] = world_pos + delta + height;
			line_verts[1] = world_pos + next_delta + height;
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, color.c());

			line_verts[0] = world_pos + delta + height;
			line_verts[1] = world_pos + delta + height + delta * 0.1f * Physics::sgn(intensity);
			UI::System::Get().DrawTransform(&line_verts[0], 2, &view_projection, color.c());
		}
	}

	void ScreenspaceTracer::RenderWireframe(const simd::matrix & view_projection)
	{
		this->shallow_phaseline.RenderWireframe(view_projection, -150.0f, 0xff00ff00);
		this->rocky_phaseline.RenderWireframe(view_projection, -170.0f, 0xffff0000);
		this->deep_phaseline.RenderWireframe(view_projection, -190.0f, 0xff0000ff);

		this->river_bank_phaseline.RenderWireframe(view_projection, -210.0f, 0xff00ffff);

		for (auto &source_constraint : water_source_constraints)
		{
			RenderSource(view_projection, source_constraint.source > 0.0f ? 0xffff0000 : 0xff00ff00, source_constraint.world_pos, source_constraint.radius, source_constraint.source);
		}
		for (auto &blocking_constraint : water_blocking_constraints)
		{
			RenderSource(view_projection, 0xffffffff, blocking_constraint.world_pos, blocking_constraint.radius, 30.0f);
		}
	}

	void ScreenspaceTracer::OverrideDepth(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Pass* pass, 
		float depth_bias, 
		float slope_scale)
	{
		auto* pipeline = FindPipeline("OverrideDepth", pass, depth_overrider.pixel_shader.Get(),
				Device::BlendState(
					Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
					Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)),
				Device::DefaultRasterizerState().SetDepthBias(depth_bias, slope_scale),
				Device::DepthStencilState(
					Device::WriteOnlyDepthState(),
					Device::StencilState(true, 8, Device::CompareMode::EQUAL, Device::StencilOp::KEEP, Device::StencilOp::KEEP)));
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			auto* pixel_binding_set = FindBindingSet("OverrideDepth", device, depth_overrider.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("OverrideDepth", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set);

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}
	}

	void ScreenspaceTracer::ComputeGlobalIllumination(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> color_tex,
		Device::Handle<Device::Texture> depth_mip_tex,
		Device::Handle<Device::Texture> linear_depth_tex,
		Device::Handle<Device::Texture> normal_mip_tex,
		simd::vector2_int viewport_size,
		simd::vector2 frame_to_dynamic_scale,
		size_t mip_level,
		float indirect_light_area,
		float indirect_light_rampup,
		float ambient_occlusion_power,
		float thickness_angle,
		float curr_time,
		int detail_level,
		Device::RenderTarget *dst_ambient_data)
	{
		const auto display_size = (simd::vector2(dst_ambient_data->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_ambient_data };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("GlobalIllumination", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		auto* pixel_uniform_buffer = FindUniformBuffer("GlobalIllumination", device, global_illumination.pixel_shader.Get(), dst_ambient_data);
		pixel_uniform_buffer->SetInt("viewport_width", viewport_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", viewport_size.y);
		simd::matrix inv_projection_matrix = camera->GetFinalProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_projection_matrix);
		simd::matrix projection_matrix = camera->GetFinalProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("proj_matrix", &projection_matrix);
		simd::matrix view_matrix = camera->GetViewMatrix();
		pixel_uniform_buffer->SetMatrix("view_matrix", &view_matrix);
		pixel_uniform_buffer->SetFloat("curr_time", curr_time);
		pixel_uniform_buffer->SetInt("detail_level", detail_level);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);
		pixel_uniform_buffer->SetInt("mip_level", int(mip_level)); 
		pixel_uniform_buffer->SetFloat("indirect_light_rampup", indirect_light_rampup);
		pixel_uniform_buffer->SetFloat("indirect_light_area", indirect_light_area);
		pixel_uniform_buffer->SetFloat("ambient_occlusion_power", ambient_occlusion_power);
		pixel_uniform_buffer->SetFloat("thickness_angle", thickness_angle);

		auto* pipeline = FindPipeline("GlobalIllumination", pass, global_illumination.pixel_shader.Get(),
		 	Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "normal_mip_tex", normal_mip_tex.Get() });
			inputs.push_back({ "depth_mip_tex", depth_mip_tex.Get() });
			inputs.push_back({ "linear_depth_tex", linear_depth_tex.Get() });
			inputs.push_back({ "color_tex", color_tex.Get() });
			auto* pixel_binding_set = FindBindingSet("GlobalIllumination", device, global_illumination.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("GlobalIllumination", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ComputeScreenspaceShadowsChannel(
		Device::IDevice * device,
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> depth_mip_tex,
		Device::Handle<Device::Texture> pixel_offset_tex,
		Device::Handle<Device::Texture> linear_depth_tex,
		simd::vector2 frame_to_dynamic_scale,
		simd::vector4 *position_data, simd::vector4 *direction_data, simd::vector4 *color_data, size_t lights_count,
		Device::RenderTarget *dst_shadowmap, simd::vector2_int viewport_size, size_t mip_level, size_t downscale_mip_level, float mip_offset, int target_channel, bool clear)
	{
		const auto display_size = (simd::vector2(dst_shadowmap->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_shadowmap };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("ScreenspaceShadowsChannel", render_targets, depth_stencil, clear, simd::color(1.0f, 1.0f, 1.0f, 1.0f));
		command_buffer.BeginPass(pass, display_size, display_size);

		size_t local_mip_level = mip_level - downscale_mip_level;
		bool use_offset_tex = !!pixel_offset_tex;
		ScreenpsaceShadowsKey key(use_offset_tex, mip_level);
		if(screenspace_shadows.find(key) == screenspace_shadows.end())
			key.mip_level = -1; //uses uniform mip_level version
		assert(screenspace_shadows.find(key) != screenspace_shadows.end());
		auto &shadows = screenspace_shadows[key];

		auto* pixel_uniform_buffer = FindUniformBuffer("ScreenspaceShadowsChannel", device, shadows.pixel_shader.Get(), dst_shadowmap);
		pixel_uniform_buffer->SetInt("viewport_width", viewport_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", viewport_size.y);
		pixel_uniform_buffer->SetInt("mip_level", int(mip_level));
		pixel_uniform_buffer->SetInt("downscale_mip_level", int(downscale_mip_level));
		simd::matrix inv_projection_matrix = camera->GetFinalProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_projection_matrix);
		simd::matrix projection_matrix = camera->GetFinalProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("proj_matrix", &projection_matrix);
		simd::matrix view_matrix = camera->GetViewMatrix();
		pixel_uniform_buffer->SetMatrix("view_matrix", &view_matrix);
		simd::matrix inv_view_matrix = view_matrix.inverse();
		pixel_uniform_buffer->SetMatrix("inv_view_matrix", &inv_view_matrix);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);
		pixel_uniform_buffer->SetFloat("mip_offset", mip_offset);
		pixel_uniform_buffer->SetFloat("target_channel", float(target_channel));
		pixel_uniform_buffer->SetVectorArray("position_data", position_data, UINT(lights_count));
		pixel_uniform_buffer->SetVectorArray("direction_data", direction_data, UINT(lights_count));
		pixel_uniform_buffer->SetVectorArray("color_data", color_data, UINT(lights_count));
		pixel_uniform_buffer->SetInt("lights_count", UINT(lights_count));
		simd::vector4 photometric_profiles_size = simd::vector4( static_cast< float >(photometric_profiles_texture.GetWidth() ), static_cast< float >(photometric_profiles_texture.GetHeight() ), 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("photometric_profiles_size", &photometric_profiles_size);

		auto* pipeline = FindPipeline("ScreenspaceShadows", pass, shadows.pixel_shader.Get(),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::SRCCOLOR),
				Device::BlendChannelState(Device::BlendMode::ZERO, Device::BlendOp::ADD, Device::BlendMode::SRCALPHA)),
			Device::DefaultRasterizerState(),
			Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "depth_mip_tex", depth_mip_tex.Get() });
			inputs.push_back({ "pixel_offset_tex", pixel_offset_tex.Get() });
			inputs.push_back({ "linear_depth_tex", linear_depth_tex.Get() });
			inputs.push_back({ "photometric_profiles_tex", photometric_profiles_texture.GetTexture().Get() });
			auto* pixel_binding_set = FindBindingSet("ScreenspaceShadows", device, shadows.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("ScreenspaceShadows", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ComputeRays(
		Device::IDevice * device,
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> zero_depth_tex,
		Device::Handle<Device::Texture> zero_ray_dir_tex,
		Device::Handle<Device::Texture> linear_depth_tex,
		Device::Handle<Device::Texture> color_tex,
		Device::RenderTarget *dst_hit_mip,
		simd::vector2_int viewport_size, size_t mip_level, size_t downscale_mip_level, simd::vector2 frame_to_dynamic_scale, bool clear)
	{
		const auto display_size = (simd::vector2(dst_hit_mip->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_hit_mip };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("Rays", render_targets, depth_stencil, clear, simd::color(1.0f, 0.5f, 0.0f, 0.0f));
		command_buffer.BeginPass(pass, display_size, display_size);

		size_t local_mip_level = mip_level - downscale_mip_level;
		ScreenspaceRaysKey key(mip_level);
		if (screenspace_rays.find(key) == screenspace_rays.end())
			key.mip_level = -1; //uses uniform mip_level version
		assert(screenspace_rays.find(key) != screenspace_rays.end());
		auto &rays_shader = screenspace_rays[key];

		auto* pixel_uniform_buffer = FindUniformBuffer("Rays", device, rays_shader.pixel_shader.Get(), dst_hit_mip);
		pixel_uniform_buffer->SetInt("viewport_width", viewport_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", viewport_size.y);
		pixel_uniform_buffer->SetInt("mip_level", int(mip_level));
		pixel_uniform_buffer->SetInt("downscale_mip_level", int(downscale_mip_level));
		simd::matrix inv_projection_matrix = camera->GetFinalProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_projection_matrix);
		simd::matrix projection_matrix = camera->GetFinalProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("proj_matrix", &projection_matrix);
		simd::matrix view_matrix = camera->GetViewMatrix();
		pixel_uniform_buffer->SetMatrix("view_matrix", &view_matrix);
		simd::matrix inv_view_matrix = view_matrix.inverse();
		pixel_uniform_buffer->SetMatrix("inv_view_matrix", &inv_view_matrix);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);

		auto* pipeline = FindPipeline("ScreenspaceRays", pass, rays_shader.pixel_shader.Get(),
			Device::BlendState(
				Device::BlendChannelState(Device::BlendMode::SRCALPHA, Device::BlendOp::ADD, Device::BlendMode::INVSRCALPHA),
				Device::BlendChannelState(Device::BlendMode::INVDESTALPHA, Device::BlendOp::ADD, Device::BlendMode::ONE)),
			Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "zero_depth_tex", zero_depth_tex.Get() });
			inputs.push_back({ "zero_ray_dir_tex", zero_ray_dir_tex.Get() });
			inputs.push_back({ "linear_depth_tex", linear_depth_tex.Get() });
			inputs.push_back({ "color_tex", color_tex.Get() });
			auto* pixel_binding_set = FindBindingSet("ScreenspaceRays", device, rays_shader.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("ScreenspaceRays", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ComputeBloomCutoff(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget* src_tex,
		Device::RenderTarget* dst_bloom,
		float cutoff,
		float intensity,
		simd::vector2 frame_to_dynamic_scale)
	{
		Device::ColorRenderTargets render_targets = { dst_bloom };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("BloomCutoff", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, dst_bloom->GetSize(), dst_bloom->GetSize());

		auto* pixel_uniform_buffer = FindUniformBuffer("BloomCutoff", device, bloom_cutoff.pixel_shader.Get(), dst_bloom);
		pixel_uniform_buffer->SetInt("viewport_width", dst_bloom->GetSize().x);
		pixel_uniform_buffer->SetInt("viewport_height", dst_bloom->GetSize().y);
		pixel_uniform_buffer->SetFloat("cutoff", cutoff);
		pixel_uniform_buffer->SetFloat("intensity", intensity);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);

		auto* pipeline = FindPipeline("BloomCutoff", pass, bloom_cutoff.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "src_sampler", src_tex->GetTexture().Get() });
			auto* pixel_binding_set = FindBindingSet("BloomCutoff", device, bloom_cutoff.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("BloomCutoff", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ComputeDoFLayers(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Texture* src_texture,
		Device::Texture* depth_mip_tex,
		Device::RenderTarget* foreground,
		Device::RenderTarget* background,
		float focus_range,
		float focus_distance,
		float transition_background,
		float transition_foreground,
		uint32_t mip_level,
		simd::vector2 frame_to_dynamic_scale
	)
	{
		const auto display_size = (simd::vector2(foreground->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { foreground, background };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("DOFLayers", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		auto* pixel_uniform_buffer = FindUniformBuffer("DOFLayers", device, dof_layers.pixel_shader.Get(), foreground);
		pixel_uniform_buffer->SetInt("viewport_width", foreground->GetSize().x);
		pixel_uniform_buffer->SetInt("viewport_height", foreground->GetSize().y);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);
		auto inv_proj_matrix = camera->GetProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_proj_matrix);
		pixel_uniform_buffer->SetFloat("focus_range", focus_range);
		pixel_uniform_buffer->SetFloat("focus_distance", focus_distance);
		simd::vector4 transition = simd::vector4(transition_foreground, transition_background, 0, 0);
		pixel_uniform_buffer->SetVector("transition_region", &transition);
		pixel_uniform_buffer->SetInt("dst_mip_level", mip_level);

		auto* pipeline = FindPipeline("DOFLayers", pass, dof_layers.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "depth_sampler", depth_mip_tex });
			inputs.push_back({ "src_sampler", src_texture });
			auto* pixel_binding_set = FindBindingSet("DOFLayers", device, dof_layers.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("DOFCircleOfConfusion", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ComputeDoFBlur(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Texture* src_tex,
		Device::RenderTarget* dst_target,
		simd::vector2 blur_direction_sigma,
		simd::vector2 frame_to_dynamic_scale)
	{
		Memory::SmallVector<float, 16, Memory::Tag::Render> gaussian_values(16, 0.0f);
		Memory::SmallVector<float, 16, Memory::Tag::Render> sample_offsets(16, 0.0f);

		const float min_sigma = 0.5f;
		const float max_sigma = GetMaxDoFBlurSigma();
		const float sigma = std::max(min_sigma, std::min(std::max(blur_direction_sigma.x, blur_direction_sigma.y), max_sigma));
		const int radius = (int)round(2 * sigma);
		for (int j = 0; j <= radius; j++)
		{
			assert(j < sample_offsets.size());
			float value = 1.0f / (sqrt(2 * PI) * sigma) * exp(-j * j / (2 * sigma * sigma));
			gaussian_values[j] = value;
			sample_offsets[j] = (float)j;
		}

		float summ = std::accumulate(gaussian_values.begin(), gaussian_values.end(), 0.0f);
		summ = std::accumulate(gaussian_values.begin() + 1, gaussian_values.end(), summ);
		for (auto& value : gaussian_values) // Normalize
			value /= summ;

		const auto display_size = simd::vector2(dst_target->GetSize()).round();
		Device::ColorRenderTargets render_targets = { dst_target };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("ComputeDoFBlur", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		auto* pixel_uniform_buffer = FindUniformBuffer("ComputeDoFBlur", device, dof_blur.pixel_shader.Get(), dst_target);
		pixel_uniform_buffer->SetInt("viewport_width", dst_target->GetSize().x);
		pixel_uniform_buffer->SetInt("viewport_height", dst_target->GetSize().y);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);
		
		simd::vector4 blur_direction = simd::vector4(
			blur_direction_sigma.x > 0.1f ? (float)radius : 0.0f,
			blur_direction_sigma.y > 0.1f ? (float)radius : 0.0f,
			0, 0);
		pixel_uniform_buffer->SetVector("blur_direction", &blur_direction);

		pixel_uniform_buffer->SetVectorArray("blur_offsets", reinterpret_cast<simd::vector4*>(sample_offsets.data()), (uint32_t)sample_offsets.size() / 4);
		pixel_uniform_buffer->SetVectorArray("blur_weights", reinterpret_cast<simd::vector4*>(gaussian_values.data()), (uint32_t)gaussian_values.size() / 4);

		auto inv_proj_matrix = camera->GetProjectionMatrix().inverse();

		auto* pipeline = FindPipeline("ComputeDoFBlur", pass, dof_blur.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "src_sampler", src_tex });

			auto* pixel_binding_set = FindBindingSet("ComputeDoFBlur", device, dof_blur.pixel_shader.Get(), inputs);
			auto* descriptor_set = FindDescriptorSet("ComputeDoFBlur", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ComputeBloomGathering(
		Device::IDevice* device,
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> bloom_mips_tex,
		float weight_mult,
		simd::vector2 frame_to_dynamic_scale,
		Device::RenderTarget* dst_bloom)
	{
		const auto display_size = (simd::vector2(dst_bloom->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_bloom };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("BloomGathering", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		auto* pixel_uniform_buffer = FindUniformBuffer("BloomGathering", device, bloom_gather.pixel_shader.Get(), dst_bloom);
		pixel_uniform_buffer->SetInt("viewport_width", dst_bloom->GetSize().x);
		pixel_uniform_buffer->SetInt("viewport_height", dst_bloom->GetSize().y);
		pixel_uniform_buffer->SetInt("mips_count", int(bloom_mips_tex->GetMipsCount()));
		pixel_uniform_buffer->SetFloat("weight_mult", weight_mult);
		simd::vector4 frame_to_dynamic_scale4 = simd::vector4(frame_to_dynamic_scale.x, frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &frame_to_dynamic_scale4);

		auto* pipeline = FindPipeline("BloomGathering", pass, bloom_gather.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			inputs.push_back({ "src_sampler", bloom_mips_tex.Get() });
			auto* pixel_binding_set = FindBindingSet("BloomGathering", device, bloom_gather.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("BloomGathering", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ApplyDepthAwareBlur(
		Device::IDevice * device,
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> data_mip_tex,
		Device::Handle<Device::Texture> depth_mip_tex,
		Device::Handle<Device::Texture> normal_mip_tex,
		Device::RenderTarget *dst_data_mip,
		simd::vector2 frame_to_dynamic_scale,
		simd::vector2_int radii,
		simd::vector2_int base_size,
		size_t mip_level,
		float gamma)
	{
		bool is_square_blur = radii.x > 0 && radii.y > 0;
		auto &depth_aware_blur = is_square_blur ? *depth_aware_blur_square : *depth_aware_blur_linear;

		const auto display_size = (simd::vector2(dst_data_mip->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_data_mip };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("DepthAwareBlur", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		auto* pixel_uniform_buffer = FindUniformBuffer("DepthAwareBlur", device, depth_aware_blur.pixel_shader.Get(), dst_data_mip);
		pixel_uniform_buffer->SetInt("viewport_width", base_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", base_size.y);
		simd::vector4 float4_scale = simd::vector4(frame_to_dynamic_scale, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &float4_scale);
		simd::matrix inv_projection_matrix = camera->GetFinalProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_projection_matrix);
		simd::matrix projection_matrix = camera->GetFinalProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("proj_matrix", &projection_matrix);
		simd::vector4 float4_radii = simd::vector4(float(radii.x), float(radii.y), 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("radii", &float4_radii);
		pixel_uniform_buffer->SetInt("mip_level", int(mip_level));
		pixel_uniform_buffer->SetFloat("gamma", gamma);

		auto* pipeline = FindPipeline("DepthAwareBlur", pass, depth_aware_blur.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			if (normal_mip_tex)
				inputs.push_back({ "normal_mip_tex", normal_mip_tex.Get() });
			if (depth_mip_tex)
				inputs.push_back({ "depth_mip_tex", depth_mip_tex.Get() });
			if (data_mip_tex)
				inputs.push_back({ "data_mip_tex", data_mip_tex.Get() });
			auto* pixel_binding_set = FindBindingSet("DepthAwareBlur", device, depth_aware_blur.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("DepthAwareBlur", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::ApplyBlur(
		Device::IDevice * device,
		Device::CommandBuffer& command_buffer,
		Device::RenderTarget *src_data,
		Device::RenderTarget *dst_data,
		float gamma,
		float exp_mult,
		simd::vector2 frame_to_dynamic_scale,
		simd::vector2_int radii)
	{
		bool is_square_blur = radii.x > 0 && radii.y > 0;

		const auto display_size = (simd::vector2(dst_data->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_data };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("Blur", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		Blur& blur = dst_data->GetFormat() == Device::PixelFormat::G32R32F ? *blur_fp32 : *blur_8;

		auto* pixel_uniform_buffer = FindUniformBuffer("Blur", device, blur.pixel_shader.Get(), dst_data);
		pixel_uniform_buffer->SetInt("viewport_width", dst_data->GetSize().x);
		pixel_uniform_buffer->SetInt("viewport_height", dst_data->GetSize().y);
		simd::vector4 float4_scale = simd::vector4(frame_to_dynamic_scale, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &float4_scale);
		pixel_uniform_buffer->SetInt("mip_level", int(src_data->GetTextureMipLevel()));
		pixel_uniform_buffer->SetFloat("gamma", gamma);
		pixel_uniform_buffer->SetFloat("exp_mult", exp_mult);
		simd::vector4 float4_radii = simd::vector4(float(radii.x), float(radii.y), 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("radii", &float4_radii);

		auto* pipeline = FindPipeline("Blur", pass, blur.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			if (src_data)
				inputs.push_back({ "data_sampler", src_data->GetTexture().Get() });
			auto* pixel_binding_set = FindBindingSet("Blur", device, blur.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("Blur", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}


	void ScreenspaceTracer::LinearizeDepth(
		Device::IDevice * device,
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> depth_tex,
		Device::RenderTarget *dst_linear_depth,
		simd::vector2 frame_to_dynamic_scale,
		simd::vector2_int base_size,
		int mip_level, int downscale)
	{
		const auto display_size = (simd::vector2(dst_linear_depth->GetSize()) * frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_linear_depth };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("LinearizeDepth", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		assert(depth_linearizers.find(DepthLinearizerKey(downscale)) != depth_linearizers.end());
		auto &depth_linearizer = depth_linearizers[DepthLinearizerKey(downscale)];

		auto* pixel_uniform_buffer = FindUniformBuffer("LinearizeDepth", device, depth_linearizer.pixel_shader.Get(), dst_linear_depth);
		pixel_uniform_buffer->SetInt("viewport_width", base_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", base_size.y);
		simd::vector4 float4_scale = simd::vector4(frame_to_dynamic_scale, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("frame_to_dynamic_scale", &float4_scale);
		simd::matrix inv_projection_matrix = camera->GetFinalProjectionMatrix().inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_projection_matrix);
		simd::matrix projection_matrix = camera->GetFinalProjectionMatrix();
		pixel_uniform_buffer->SetMatrix("proj_matrix", &projection_matrix);
		pixel_uniform_buffer->SetInt("mip_level", mip_level);

		auto* pipeline = FindPipeline("DepthLinearizer", pass, depth_linearizer.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			if (depth_tex)
				inputs.push_back({ "depth_sampler", depth_tex.Get() });
			auto* pixel_binding_set = FindBindingSet("DepthLinearizer", device, depth_linearizer.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("DepthLinearizer", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	void ScreenspaceTracer::Reproject(
		Device::IDevice * device, 
		Device::CommandBuffer& command_buffer,
		Device::Handle<Device::Texture> curr_depth_tex, 
		Device::Handle<Device::Texture> prev_depth_tex, 
		Device::Handle<Device::Texture> prev_data_tex, 
		Device::RenderTarget * dst_data, 
		simd::vector2 curr_frame_to_dynamic_scale, 
		simd::vector2 prev_frame_to_dynamic_scale, 
		simd::matrix proj_matrix, 
		simd::matrix curr_viewproj_matrix, 
		simd::matrix prev_viewproj_matrix, 
		bool use_high_quality,
		simd::vector2_int base_size, int mip_level, int downscale)
	{
		const auto display_size = (simd::vector2(dst_data->GetSize()) * curr_frame_to_dynamic_scale).round();
		Device::ColorRenderTargets render_targets = { dst_data };
		Device::RenderTarget* depth_stencil = nullptr;
		auto* pass = FindPass("Reproject", render_targets, depth_stencil);
		command_buffer.BeginPass(pass, display_size, display_size);

		auto key = ReprojectorKey(true, use_high_quality);
		assert(reprojectors.find(key) != reprojectors.end());
		auto &reprojector = reprojectors[key];

		auto* pixel_uniform_buffer = FindUniformBuffer("Reproject", device, reprojector.pixel_shader.Get(), dst_data);
		pixel_uniform_buffer->SetInt("viewport_width", base_size.x);
		pixel_uniform_buffer->SetInt("viewport_height", base_size.y);
		simd::vector4 curr_scale4 = simd::vector4(curr_frame_to_dynamic_scale.x, curr_frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("curr_frame_to_dynamic_scale", &curr_scale4);
		simd::vector4 prev_scale4 = simd::vector4(prev_frame_to_dynamic_scale.x, prev_frame_to_dynamic_scale.y, 0.0f, 0.0f);
		pixel_uniform_buffer->SetVector("prev_frame_to_dynamic_scale", &prev_scale4);
		pixel_uniform_buffer->SetMatrix("curr_viewproj_matrix", &curr_viewproj_matrix);
		simd::matrix curr_inv_viewproj_matrix = curr_viewproj_matrix.inverse();
		pixel_uniform_buffer->SetMatrix("curr_inv_viewproj_matrix", &curr_inv_viewproj_matrix);
		pixel_uniform_buffer->SetMatrix("prev_viewproj_matrix", &prev_viewproj_matrix);
		simd::matrix prev_inv_viewproj_matrix = prev_viewproj_matrix.inverse();
		pixel_uniform_buffer->SetMatrix("prev_inv_viewproj_matrix", &prev_inv_viewproj_matrix);
		pixel_uniform_buffer->SetMatrix("proj_matrix", &proj_matrix);
		simd::matrix inv_projection_matrix = proj_matrix.inverse();
		pixel_uniform_buffer->SetMatrix("inv_proj_matrix", &inv_projection_matrix);
		pixel_uniform_buffer->SetInt("mip_level", mip_level);
		pixel_uniform_buffer->SetInt("downscale", downscale);

		auto* pipeline = FindPipeline("Reprojector", pass, reprojector.pixel_shader.Get(),
			Device::DisableBlendState(), Device::DefaultRasterizerState(), Device::DisableDepthStencilState());
		if (command_buffer.BindPipeline(pipeline))
		{
			Device::DynamicBindingSet::Inputs inputs;
			if (curr_depth_tex)
				inputs.push_back({ "curr_depth_sampler", curr_depth_tex.Get() });
			if (prev_depth_tex)
				inputs.push_back({ "prev_depth_sampler", prev_depth_tex.Get() });
			if (prev_data_tex)
				inputs.push_back({ "prev_data_sampler", prev_data_tex.Get() });
			auto* pixel_binding_set = FindBindingSet("Reprojector", device, reprojector.pixel_shader.Get(), inputs);

			auto* descriptor_set = FindDescriptorSet("Reprojector", pipeline, pixel_binding_set);
			command_buffer.BindDescriptorSet(descriptor_set, {}, { pixel_uniform_buffer });

			command_buffer.BindBuffers(nullptr, vertex_buffer.Get(), 0, sizeof(Vertex));
			command_buffer.Draw(4, 0);
		}

		command_buffer.EndPass();
	}

	bool ScreenspaceTracer::AddDepthProjectionDrawData(
		Device::UniformInputs& pass_uniform_inputs,
		Device::BindingInputs& pass_binding_inputs,
		Device::Handle<Device::Texture> depth_tex,
		bool use_depth_driver_hack)
	{
		reprojection_draw_data.type_0 = 0.0f;
		reprojection_draw_data.type_1 = 1.0f;
		reprojection_draw_data.type_2 = 2.0f;
		if(depth_tex)
		{
			pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DepthSampler, 0)).SetTexture(depth_tex));
			pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DepthSamplerType, 0)).SetFloat(use_depth_driver_hack ? &reprojection_draw_data.type_2 : &reprojection_draw_data.type_1));
		}else
		{
			pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DepthSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
			pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DepthSamplerType, 0)).SetFloat(&reprojection_draw_data.type_0));
		}

		{
			pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::FogFlowmapSampler, 0)).SetTexture(fog_flowmap_data.texture));

			fog_flowmap_data.tex_size_draw_data = simd::vector4(float(fog_flowmap_data.tex_size.x), float(fog_flowmap_data.tex_size.y), 0.0f, 0.0f);
			fog_flowmap_data.world_size_draw_data = simd::vector4(scene_size.x, scene_size.y, 0.0f, 0.0f);
			pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FogFlowmapTexSize, 0)).SetVector(&fog_flowmap_data.tex_size_draw_data));
			pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FogFlowmapWorldSize, 0)).SetVector(&fog_flowmap_data.world_size_draw_data));
		}

		return true;
	}

	bool ScreenspaceTracer::AddNullWaterDrawData(
		Device::UniformInputs& pass_uniform_inputs,
		Device::BindingInputs& pass_binding_inputs,
		simd::vector4 ambient_light_color,
		simd::vector4 ambient_light_dir,
		float curr_time)
	{
		AddWaterDrawDataUniforms(pass_uniform_inputs, ambient_light_color, ambient_light_dir, curr_time, false);

		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::OpaqueFramebufferSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WaveDataSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WaveGradientSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DistortionSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WaterTexSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::SurfaceSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::FlowmapSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::CausticsSampler, 0)).SetTexture(Device::Handle<Device::Texture>()));
		return true;
	}

	void ScreenspaceTracer::AddWaterDrawDataUniforms(
		Device::UniformInputs& pass_uniform_inputs,
		simd::vector4 ambient_light_color,
		simd::vector4 ambient_light_dir,
		float curr_time,
		bool use_downsampling)
	{
		water_draw_data.ambient_light_color = ambient_light_color;
		water_draw_data.ambient_light_dir = ambient_light_dir;
		water_draw_data.numerical_normal = 0.0f;
		water_draw_data.current_time = curr_time;
		water_draw_data.flow_to_color = this->flow_to_color;

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::AmbientLightColor, 0)).SetVector(&water_draw_data.ambient_light_color));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::AmbientLightDir, 0)).SetVector(&water_draw_data.ambient_light_dir));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::NumericalNormal, 0)).SetFloat(&water_draw_data.numerical_normal));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::CurrentTime, 0)).SetFloat(&water_draw_data.current_time));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::FlowToColor, 0)).SetFloat(&water_draw_data.flow_to_color));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::WaterUseDownsampling, 0)).SetBool(use_downsampling ? &bool_true : &bool_false));
	}

	bool ScreenspaceTracer::AddWaterDrawData(
		Device::UniformInputs& pass_uniform_inputs,
		Device::BindingInputs& pass_binding_inputs,
		Device::Handle<Device::Texture> opaque_framebuffer_tex,
		simd::vector4 ambient_light_color,
		simd::vector4 ambient_light_dir,
		float curr_time,
		bool use_downsampling)
	{
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::OpaqueFramebufferSampler, 0)).SetTexture(opaque_framebuffer_tex));

		auto GetTexture = [](const ::Texture::Handle& handle) -> Device::Handle<Device::Texture>
		{
			if (!handle.IsNull())
				return handle.GetTexture();
			return Device::Handle<Device::Texture>();
		};

		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WaveDataSampler, 0)).SetTexture(GetTexture(wave_profile_texture)));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WaveGradientSampler, 0)).SetTexture(GetTexture(wave_profile_gradient_texture)));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::DistortionSampler, 0)).SetTexture(GetTexture(distorton_texture)));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::WaterTexSampler, 0)).SetTexture(GetTexture(surface_foam_texture)));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::SurfaceSampler, 0)).SetTexture(GetTexture(ripple_texture)));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::FlowmapSampler, 0)).SetTexture(river_flowmap_data.texture));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::CausticsSampler, 0)).SetTexture(GetTexture(caustics_texture)));
	
		AddWaterDrawDataUniforms(pass_uniform_inputs, ambient_light_color, ambient_light_dir, curr_time, use_downsampling);

		return true;
	}

	bool ScreenspaceTracer::AddShorelineDrawData(
		Device::UniformInputs& pass_uniform_inputs,
		Device::BindingInputs& pass_binding_inputs)
	{
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::ShorelineCoordSampler, 0)).SetTexture(water_draw_data.shoreline_coords_tex));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::ShorelineCoordSize, 0)).SetVector(&water_draw_data.shoreline_coords_tex_size));

		simd::vector4 scene_size_float4;
		scene_size_float4.x = this->scene_size.x;
		scene_size_float4.y = this->scene_size.y;
		scene_size_float4.z = 0.0f;
		scene_size_float4.w = 0.0f;
		water_draw_data.scene_size = scene_size_float4;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SceneSize, 0)).SetVector(&water_draw_data.scene_size));

		water_draw_data.sum_longitude = this->sum_longitude;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::SumLongitude, 0)).SetFloat(&water_draw_data.sum_longitude));

		water_draw_data.dist_to_color = this->dist_to_color;
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::DistToColor, 0)).SetFloat(&water_draw_data.dist_to_color));

		return true;
	}


	bool ScreenspaceTracer::AddScreenspaceDrawData(
		Device::UniformInputs& pass_uniform_inputs,
		Device::BindingInputs& pass_binding_inputs,
		Device::Handle<Device::Texture> gi_ambient_texture,
		Device::Handle<Device::Texture> ss_shadowmap_texture,
		Device::Handle<Device::Texture> ray_hit_texture,
		int ssgi_detail)
	{
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::AmbientLightSampler, 0)).SetTexture(gi_ambient_texture ? gi_ambient_texture : default_ambient_tex));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::RayHitTexture, 0)).SetTexture(ray_hit_texture));
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::ScreenspaceShadowmap, 0)).SetTexture(ss_shadowmap_texture));

		global_illumination_debug = 0.0f;
		#if ( !defined(CONSOLE) && !defined( __APPLE__ ) && !defined( ANDROID ) ) && (defined(TESTING_CONFIGURATION) || defined(DEVELOPMENT_CONFIGURATION))
		if (GetAsyncKeyState(VK_SPACE))
			global_illumination_debug = 1.0f;
		else
			global_illumination_debug = 0.0f;
		#endif

		if (!gi_ambient_texture)
		{
			global_illumination_intensity = 0.0f;
		}
		else
		{
			global_illumination_intensity = 1.0f;
		}

		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::GlobalIlluminationIntensity, 0)).SetFloat(&global_illumination_intensity));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::GlobalIlluminationDebug, 0)).SetFloat(&global_illumination_debug));
		pass_uniform_inputs.push_back(Device::UniformInput(Device::IdHash(DrawDataNames::GlobalIlluminationDetail, 0)).SetBool(ssgi_detail ? &bool_true : &bool_false));

		return true;
	}

	void ScreenspaceTracer::reload(Device::IDevice *device)
	{
		OnLostDevice();
		OnDestroyDevice();
		OnCreateDevice(device);
		OnResetDevice(device);
	}



}
