#include "Common/File/InputFileStream.h"

#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"

#include "EmitterTemplate.h"

namespace Particles
{
	const Device::VertexElements vertex_elements =
	{
		{ 0, 0,					Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 },
		{ 0, 3 * sizeof(float),	Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0 }, // tex coords
		{ 1, 0,					Device::DeclType::FLOAT3, Device::DeclUsage::TEXCOORD, 1 }, // instance position
		{ 1, 3 * sizeof(float), Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 5 }, //random seed
		{ 1, 4 * sizeof(float),	Device::DeclType::FLOAT16_4, Device::DeclUsage::NORMAL, 0 }, // scalexy_rotation_index
		{ 1, 6 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::COLOR, 0 }, // color
	};

	const Device::VertexElements rotation_lock_vertex_elements =
	{
		{ 0, 0,					Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 },
		{ 0, 3 * sizeof(float),	Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0 }, // tex coords
		{ 1, 0,					Device::DeclType::FLOAT3, Device::DeclUsage::TEXCOORD, 1 }, // instance position
		{ 1, 3 * sizeof(float), Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 5 }, //random seed
		{ 1, 4 * sizeof(float),	Device::DeclType::FLOAT16_4, Device::DeclUsage::NORMAL, 0 }, // scalexy_rotation_index
		{ 1, 6 * sizeof(float),	Device::DeclType::FLOAT16_4, Device::DeclUsage::TEXCOORD, 2 }, // velocity (4th channel unused)
		{ 1, 8 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::COLOR, 0 }, // color
	};

	const Device::VertexElements no_lock_vertex_elements =
	{
		{ 0, 0,					Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 },
		{ 0, 3 * sizeof(float),	Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0 }, // tex coords
		{ 1, 0,					Device::DeclType::FLOAT3, Device::DeclUsage::TEXCOORD, 1 }, // instance position
		{ 1, 3 * sizeof(float), Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 5 }, //random seed
		{ 1, 4 * sizeof(float),	Device::DeclType::FLOAT16_4, Device::DeclUsage::NORMAL, 0 }, // scalexy_rotation_index
		{ 1, 6 * sizeof(float),	Device::DeclType::FLOAT16_4, Device::DeclUsage::TEXCOORD, 2 }, // normal (4th channel unused)
		{ 1, 8 * sizeof(float),	Device::DeclType::FLOAT16_4, Device::DeclUsage::TEXCOORD, 3 }, // tangent (4th channel unused)
		{ 1,10 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::COLOR, 0 }, // color
	};

	const Device::VertexElements mesh_vertex_elements =
	{
		{1, 0,					Device::DeclType::FLOAT4, Device::DeclUsage::TEXCOORD, 1 }, // transform row 0
		{1, 4 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::TEXCOORD, 2 }, // transform row 1
		{1, 8 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::TEXCOORD, 3 }, // transform row 2
		{1, 12 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::TEXCOORD, 4 }, // transform row 3
		{1, 16 * sizeof(float),	Device::DeclType::FLOAT4, Device::DeclUsage::COLOR, 0 }, // color
		{1, 20 * sizeof(float), Device::DeclType::FLOAT1, Device::DeclUsage::TEXCOORD, 5 }, //random seed
	};


	class EmitterTemplate::Reader : public FileReader::PETReader::Emitter
	{
	private:
		EmitterTemplate* parent;

		template<typename A, typename TrackB> static inline void CopyPoints(A& dst, const TrackB& src) { dst = src.ToConstant(); }

		template<typename A, typename TrackB, size_t N> static inline void CopyPoints(A(&dst)[N], const TrackB& src) { src.ToArray(dst, N); }

		template<typename A, typename B, size_t NA, size_t NB> static inline void CopyPoints(A(&dst)[NA], const B(&src)[NB])
		{
			for (size_t a = 0; a < NA; a++)
			{
				if (a < NB)
					dst[a] = (A)src[a];
				else if (a > 0)
					dst[a] = dst[a - 1];
				else
					dst[a] = A();
			}
		}

		template<typename D1, size_t M1, Memory::Tag T1, typename DT1, typename D2, size_t M2, Memory::Tag T2, typename DT2>
		static inline void CopyPoints(Utility::DataTrack::DataTrack<D1, M1, T1, DT1>& dst, const Utility::DataTrack::DataTrack<D2, M2, T2, DT2>& src)
		{
			dst.clear();
			for (const auto& keyframe : src)
				dst.Add(keyframe.Time, keyframe.Value, keyframe.Mode);

			dst.RescaleDuration(1.0f);
		}

		template<typename D1, size_t M1, Memory::Tag T1, typename DT1, typename D2, size_t M2, Memory::Tag T2, typename DT2>
		static inline void CopyPoints(Utility::DataTrack::OptionalDataTrack<D1, M1, T1, DT1>& dst, const Utility::DataTrack::DataTrack<D2, M2, T2, DT2>& src)
		{
			dst.clear();
			for (const auto& keyframe : src)
				dst.Add(keyframe.Time, keyframe.Value, keyframe.Mode);

			dst.RescaleDuration(1.0f);
		}

	public:
		Reader(EmitterTemplate* parent) : parent(parent) {}
		~Reader()
		{
			const auto scale_limit = parent->scale.CalculateMinMax();
			const auto stretch_limit = parent->stretch.CalculateMinMax();
			const auto emit_scale_limit = parent->emit_scale.CalculateMinMax();

			parent->max_scale = (std::max(std::abs(scale_limit.first), std::abs(scale_limit.second)) + parent->scale_variance) * 
				std::max(1.0f, std::max(std::abs(stretch_limit.first), std::abs(stretch_limit.second)) + parent->stretch_variance) *
				std::max(std::abs(emit_scale_limit.first), std::abs(emit_scale_limit.second));
		}

		void SetEmitterType(EmitterType type) override
		{
			parent->emitter_type = type;
		}

		void SetMaterial(const std::wstring& filename) override
		{
			parent->texture_filename = filename;
			if(!filename.empty())
				parent->material = MaterialHandle(filename);
		}

#ifdef ENABLE_PARTICLE_FMT_SEQUENCE
		void SetMesh(const Memory::Vector<std::wstring, Memory::Tag::Particle>& filenames) override
		{
			parent->mesh_handles.clear();
			parent->mesh_handles.reserve(filenames.size());
			for (size_t a = 0; a < filenames.size(); a++)
			{
				if (!filenames[a].empty())
					parent->mesh_handles.push_back(Mesh::FixedMeshHandle(filenames[a]));
			}
		}
#else
		void SetMesh(const std::wstring& filename) override
		{
			if(!filename.empty())
				parent->mesh_handle = Mesh::FixedMeshHandle(filename);
		}
#endif

		void SetParticleOffset(float offset_x, float offset_y) override
		{
			parent->particle_offset[0] = offset_x;
			parent->particle_offset[1] = offset_y;
		}

		void SetAnimationData(const FileReader::PETReader::AnimationData& animation) override
		{
			parent->scale_emitter_duration = animation.scale_emitter_duration;
			parent->scale_particle_duration = animation.scale_particle_duration;
			parent->anim_speed_scale_on_death = animation.anim_speed_scale_on_death;
			parent->number_of_frames_x = animation.num_frames_x;
			parent->number_of_frames_y = animation.num_frames_y;
			parent->anim_speed = animation.anim_speed;
			parent->anim_play_once = animation.anim_play_once;
			parent->particle_duration_variance = animation.duration_variance;
			parent->random_flip_x = animation.random_flip_x;
			parent->random_flip_y = animation.random_flip_y;
			parent->flip_mode = animation.flip_mode;
			parent->random_seed = animation.random_seed;
			parent->enable_random_seed = animation.enable_random_seed;
			parent->scale_to_bone_length = animation.scale_to_bone_length;
			parent->particle_duration = animation.duration;
			parent->velocity_along_variance = animation.velocity_along_variance;
			parent->velocity_up_variance = animation.velocity_up_variance;
			parent->scale_variance = animation.scale_variance;
			parent->rotation_speed_variance = animation.rotation_speed_variance;
			parent->reverse_bone_group = animation.reverse_bone_group;
			CopyPoints(parent->scale, animation.scale);
			CopyPoints(parent->velocity_along, animation.velocity_along);
			CopyPoints(parent->velocity_up, animation.velocity_up);
			CopyPoints(parent->rotation_speed, animation.rotation_speed);
		}

		void SetEmitData(const FileReader::PETReader::EmitData& emit) override
		{
			parent->emit_speed_threshold = emit.emit_speed_threshold;
			parent->update_speed_threshold = emit.update_speed_threshold;
			parent->rotation_direction = emit.rotation_direction;
			parent->rotation_offset_min = emit.rotation_offset_min;
			parent->rotation_offset_max = emit.rotation_offset_max;
			parent->instance_rotation_offset_min = emit.instance_rotation_offset_min;
			parent->instance_rotation_offset_max = emit.instance_rotation_offset_max;
			parent->position_offset_min = emit.position_offset_min;
			parent->position_offset_max = emit.position_offset_max;
			parent->instance_position_offset_min = emit.instance_position_offset_min;
			parent->instance_position_offset_max = emit.instance_position_offset_max;
			parent->up_acceleration_variance = emit.up_acceleration_variance;
			parent->distribute_distance = emit.distribute_distance;
			parent->is_infinite = emit.is_infinite;
			parent->min_emitter_duration = emit.min_emitter_duration;
			parent->max_emitter_duration = emit.max_emitter_duration;
			parent->emit_speed_variance = emit.emit_speed_variance;

			parent->emit_interval_min = emit.emit_interval_min;
			parent->emit_interval_max = emit.emit_interval_max;
			parent->pause_interval_min = emit.pause_interval_min;
			parent->pause_interval_max = emit.pause_interval_max;
			parent->start_interval_min = emit.start_interval_min;
			parent->start_interval_max = emit.start_interval_max;
			parent->emit_min_speed = emit.emit_min_speed;
			parent->emit_max_speed = emit.emit_max_speed;
			parent->scale_interval_durations = emit.scale_interval_durations;
			parent->use_world_speed = emit.use_world_speed;
			parent->ui_name = emit.ui_name;

			CopyPoints(parent->scatter_cone_size, emit.scatter_cone_size);
			CopyPoints(parent->particles_per_second, emit.particles_per_second);
			CopyPoints(parent->emit_scale, emit.emit_scale);
			CopyPoints(parent->up_acceleration, emit.up_acceleration);
			CopyPoints(parent->emit_colour, emit.color);

			if (parent->max_emitter_duration <= 0.0)
			{
				parent->is_infinite = true;
				parent->min_emitter_duration = 1.0f;
				parent->max_emitter_duration = 1.0f;
			}
		}

		void SetDisplayData(const FileReader::PETReader::DisplayData& display) override
		{
			parent->blend_mode = display.blend_mode;
			parent->rotation_lock_type = display.rotation_lock_type;
			parent->stretch_variance = display.stretch_varaince;
			parent->face_type = display.face_type;
			parent->lock_mode = display.lock_mode;
			parent->is_locked_to_bone = display.locked_to_bone;
			parent->is_locked_movement_to_bone = display.locked_movement_to_bone;
			parent->enable_lmb_cone_size = display.locked_movement_to_bone_conesize;
			CopyPoints(parent->stretch, display.stretch);
			CopyPoints(parent->colour, display.color);
			parent->colour_variance = display.color_variance;
			parent->priority = Device::DynamicCulling::FromParticlePriority(display.priority);
			parent->culling_threshold = size_t(display.culling_threshold);
		}
	};

	Resource::UniquePtr<FileReader::PETReader::Emitter> EmitterTemplate::GetReader(Resource::Allocator& allocator)
	{
		enabled = true;
		return Resource::UniquePtr<FileReader::PETReader::Emitter>(allocator.Construct<Reader>(this).release());
	}

	static float RoundSmallFloat(float f)
	{
		return fabs(f) < 0.0001f ? 0.0f : f;
	}

	void EmitterTemplate::Write( std::wostream& stream ) const
	{
		stream << L"\t" << FileReader::PETReader::EmitterTypeNames[ emitter_type ] << L"\r\n";
		stream << L"\t\"" << ( material.IsNull() ? L"" : std::wstring( material.GetFilename().c_str() ) ) << "\"\r\n";
		bool has_mesh = false;
#ifdef ENABLE_PARTICLE_FMT_SEQUENCE
		if (!mesh_handles.empty())
		{
			has_mesh = true;
			stream << L"\tmesh_filenames " << mesh_handles.size();
			for (const auto& mesh_handle : mesh_handles)
				stream << L" \"" << std::wstring(mesh_handle.GetFilename().c_str()) << L"\"";

			stream << L"\r\n";
		}
#else
		has_mesh = !mesh_handle.IsNull();
		stream << L"\tmesh_filename \"" << ( mesh_handle.IsNull() ? L"" : std::wstring( mesh_handle.GetFilename().c_str() ) ) << L"\"\r\n";
#endif
		stream << L"\tblend_mode " << Renderer::DrawCalls::GetBlendModeName(blend_mode) << L"\r\n";
		stream << L"\trotation_lock_type " << FileReader::PETReader::RotationLockNames[ rotation_lock_type ] << L"\r\n";
		stream << L"\tstretch " << stretch.ToString() << L"\r\n";
		stream << L"\tstretch_variance " << stretch_variance << L"\r\n";
		stream << L"\tface_type " << FileReader::PETReader::FaceTypeNames[ face_type ] << L"\r\n";
		stream << L"\tparticle_offset " << particle_offset[ 0 ] << " " << particle_offset[ 1 ] << L"\r\n";
		stream << L"\tis_locked_to_bone " << is_locked_to_bone << L"\r\n";
		stream << L"\tis_locked_movement_to_bone " << is_locked_movement_to_bone << L"\r\n";
		if (ui_name.length() > 0) stream << L"\tui_name \"" << StringToWstring(ui_name) << L"\"\r\n";
		if(enable_lmb_cone_size) stream << L"\tenable_locked_movement_to_bone_conesize " << enable_lmb_cone_size << L"\r\n";
		if (lock_mode.any()) stream << L"\tlock_mode " << lock_mode << L"\r\n";
		stream << L"\tscale_duration_to_parent " << scale_emitter_duration << L"\r\n";
		if(scale_particle_duration) stream << L"\tscale_particle_duration " << scale_particle_duration << L"\r\n";
		stream << L"\temit_speed_threshold " << emit_speed_threshold << L"\r\n";
		if(update_speed_threshold > 0.f) stream << L"\tupdate_speed_threshold " << update_speed_threshold << L"\r\n";
		if(anim_speed_scale_on_death != 1.f) stream << L"\tanim_speed_scale_on_death " << anim_speed_scale_on_death << L"\r\n";
		stream << L"\trotation_direction " << rotation_direction << L"\r\n";
		if(rotation_offset_min.sqrlen() > 0.f) stream << L"\trotation_offset " << rotation_offset_min.x * ToDegreesMultiplier << L" " << rotation_offset_min.y * ToDegreesMultiplier << L" " << rotation_offset_min.z * ToDegreesMultiplier << L"\r\n";
		if(rotation_offset_max.sqrlen() > 0.f) stream << L"\trotation_offset_max " << rotation_offset_max.x * ToDegreesMultiplier << L" " << rotation_offset_max.y * ToDegreesMultiplier << L" " << rotation_offset_max.z * ToDegreesMultiplier << L"\r\n";
		if (position_offset_min.sqrlen() > 0.f) stream << L"\tposition_offset_min " << position_offset_min.x << L" " << position_offset_min.y << L" " << position_offset_min.z << L"\r\n";
		if (position_offset_max.sqrlen() > 0.f) stream << L"\tposition_offset_max " << position_offset_max.x << L" " << position_offset_max.y << L" " << position_offset_max.z << L"\r\n";
		if (has_mesh)
		{
			if (instance_rotation_offset_min.sqrlen() > 0.f) stream << L"\tinstance_rotation_offset_min " << instance_rotation_offset_min.x * ToDegreesMultiplier << L" " << instance_rotation_offset_min.y * ToDegreesMultiplier << L" " << instance_rotation_offset_min.z * ToDegreesMultiplier << L"\r\n";
			if (instance_rotation_offset_max.sqrlen() > 0.f) stream << L"\tinstance_rotation_offset_max " << instance_rotation_offset_max.x * ToDegreesMultiplier << L" " << instance_rotation_offset_max.y * ToDegreesMultiplier << L" " << instance_rotation_offset_max.z * ToDegreesMultiplier << L"\r\n";
			if (instance_position_offset_min.sqrlen() > 0.f) stream << L"\tinstance_position_offset_min " << instance_position_offset_min.x << L" " << instance_position_offset_min.y << L" " << instance_position_offset_min.z << L"\r\n";
			if (instance_position_offset_max.sqrlen() > 0.f) stream << L"\tinstance_position_offset_max " << instance_position_offset_max.x << L" " << instance_position_offset_max.y << L" " << instance_position_offset_max.z << L"\r\n";
		}
		stream << L"\tup_acceleration_graph " << up_acceleration.ToString() << L"\r\n";
		stream << L"\tup_acceleration_variance " << up_acceleration_variance << L"\r\n";
		stream << L"\tnumber_of_frames " << number_of_frames_x << L"\r\n";
		if(number_of_frames_y > 1) stream << L"\tnumber_of_frames_y " << number_of_frames_y << L"\r\n";
		stream << L"\tanim_speed " << anim_speed << L"\r\n";
		stream << L"\tanim_play_once " << anim_play_once << L"\r\n";
		stream << L"\tparticle_duration_variance " << particle_duration_variance << L"\r\n";
		stream << L"\trandom_flip_x " << random_flip_x << L"\r\n";
		stream << L"\trandom_flip_y " << random_flip_y << L"\r\n";
		if(emit_speed_variance > 0) stream << L"\temit_speed_variance " << emit_speed_variance << L"\r\n";
		if(has_mesh) stream << L"\tflip_mode " << FileReader::PETReader::FlipModeNames[flip_mode] << L"\r\n";
		if (enable_random_seed) stream << L"\trandom_seed " << random_seed << L"\r\n";
		if (scale_to_bone_length > 0.f) stream << L"\tscale_to_bone_length " << scale_to_bone_length << L"\r\n";
		if (distribute_distance > 0.f) stream << L"\tdistribute_distance " << distribute_distance << L"\r\n";
		stream << L"\temit_interval " << emit_interval_min << L" " << emit_interval_max << L" " << pause_interval_min << L" " << pause_interval_max << L" " << start_interval_min << L" " << start_interval_max << L"\r\n";
		stream << L"\tuse_world_speed " << use_world_speed << L"\r\n";
		if (emit_min_speed >= 0.0f || emit_max_speed >= 0.0f)
			stream << L"\temit_speed_range " << (emit_min_speed >= 0.0f ? emit_min_speed : -1.0f) << L" " << (emit_max_speed >= 0.0f ? emit_max_speed : -1.0f) << L"\r\n";

		if (scale_interval_durations)
			stream << L"\tscale_interval_durations " << scale_interval_durations << L"\r\n";

		stream << L"\temit_scale " << emit_scale.ToString() << L"\r\n";
		stream << L"\tscatter_cone_size " << scatter_cone_size.ToString() << L"\r\n";
		stream << L"\tparticle_duration " << particle_duration << L"\r\n";
		stream << L"\tmax_emitter_duration " << max_emitter_duration << L"\r\n";
		stream << L"\tis_infinite " << is_infinite << L"\r\n";
		stream << L"\tparticles_per_second " << particles_per_second.ToString() << L"\r\n";
		stream << L"\tcolour_variance " << RoundSmallFloat(colour_variance.x) << L" " << RoundSmallFloat(colour_variance.y) << L" " << RoundSmallFloat(colour_variance.z) << L" " << RoundSmallFloat(colour_variance.w) << L"\r\n";
		stream << L"\temit_colour " << emit_colour.ToString() << L"\r\n";
		if (reverse_bone_group)
			stream << L"\treverse_bone_group " << reverse_bone_group << L"\r\n";

		stream << L"\tpriority " << FileReader::PETReader::PriorityNames[Device::DynamicCulling::ToParticlePriority(priority)] << L"\r\n";
		if (Device::DynamicCulling::ToParticlePriority(priority) == FileReader::PETReader::Priority::Important)
			stream << L"\tculling_threshold " << culling_threshold << L"\r\n";

		stream << L"\tvelocity_along " << velocity_along.ToString() << L"\r\n";
		stream << L"\tvelocity_along_variance " << velocity_along_variance << L"\r\n";
		stream << L"\tvelocity_up " << velocity_up.ToString() << L"\r\n";
		stream << L"\tvelocity_up_variance " << velocity_up_variance << L"\r\n";
		stream << L"\tscale " << scale.ToString() << L"\r\n";
		stream << L"\tscale_variance " << scale_variance << L"\r\n";
		stream << L"\trotation_speed " << rotation_speed.ToString() << L"\r\n";
		stream << L"\trotation_speed_variance " << rotation_speed_variance << L"\r\n";
		stream << L"\temitter_duration " << min_emitter_duration << L"\r\n";
		stream << L"\tcolour " << colour.ToString() << L"\r\n";
	}

	unsigned EmitterTemplate::MaxParticles() const
	{
		const float max_particles_per_second = particles_per_second.CalculateMinMax().second + emit_speed_variance;
		const float max_particle_duration = particle_duration * (1 + particle_duration_variance);

		const float ring_buffer_time = 
			( is_infinite ? 
				max_particle_duration :
				std::min(max_particle_duration, max_emitter_duration )
			);

		return unsigned( ring_buffer_time * max_particles_per_second) + 2;
	}

	unsigned EmitterTemplate::MinSimultaneousparticles() const
	{
		const float min_particles_per_second = std::max(0.0f, particles_per_second.CalculateMinMax().first - emit_speed_variance);
		const float min_particle_duration = particle_duration / (1 + particle_duration_variance);

		const float ring_buffer_time =
			(is_infinite ?
				min_particle_duration :
				std::min(min_particle_duration, min_emitter_duration)
				);

		return unsigned(ring_buffer_time * min_particles_per_second) + 2;
	}

	void EmitterTemplate::Warmup() const // IMPORTANT: Keep in sync with ParticleEmitter::GenerateCreateInfo. // [TODO] Share.
	{
		if (material.IsNull())
			return;

	#ifdef ENABLE_PARTICLE_FMT_SEQUENCE
		const bool is_mesh_particle = !mesh_handles.empty();
	#else
		const bool is_mesh_particle = !mesh_handle.IsNull();
	#endif

		auto cull_mode = Device::CullMode::CCW;

		if (!is_mesh_particle || (flip_mode == FlipMode::FlipMirrorNoCulling && (random_flip_x || random_flip_y)))
		{
			cull_mode = Device::CullMode::NONE;
		}
		else if (face_type == FaceType::XYLock)
		{
			if (cull_mode == Device::CullMode::CCW)
				cull_mode = Device::CullMode::CW;
			else if (cull_mode == Device::CullMode::CW)
				cull_mode = Device::CullMode::CCW;
		}

		auto shader_base = Shader::Base()
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0);

		auto renderer_base = Renderer::Base()
			.SetCullMode(cull_mode);

		if (is_mesh_particle)
		{
			auto merged_vertex_elements = mesh_handle->GetVertexElements();
			for (const auto& a : mesh_vertex_elements)
				merged_vertex_elements.push_back(a);
			renderer_base.SetVertexElements(merged_vertex_elements);

			shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_mesh.fxgraph" }, 0);

			const Mesh::VertexLayout layout(mesh_handle->GetFlags());
			if (layout.HasUV2())
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_uv2.fxgraph" }, 0);

			if (layout.HasColor())
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/attrib_color.fxgraph" }, 0);
		}
		else
		{
			const auto rotation_locked = rotation_lock_type == RotationLockType::Velocity;
			switch (face_type)
			{
			case FaceType::FaceCamera:
			{
				if (rotation_locked)
				{
					renderer_base.SetVertexElements(rotation_lock_vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_camerafacing_rotationlocked.fxgraph" }, 0);
				}
				else
				{
					renderer_base.SetVertexElements(vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_camerafacing.fxgraph" }, 0);
				}
				break;
			}
			case FaceType::XYLock:
			{
				if (rotation_locked)
				{
					renderer_base.SetVertexElements(rotation_lock_vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xylocked_rotationlocked.fxgraph" }, 0);
				}
				else
				{
					renderer_base.SetVertexElements(vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xylocked.fxgraph" }, 0);
				}
				break;
			}
			case FaceType::ZLock:
			{
				if (rotation_locked)
				{
					renderer_base.SetVertexElements(rotation_lock_vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_zlocked_rotationlocked.fxgraph" }, 0);
				}
				else
				{
					renderer_base.SetVertexElements(vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_zlocked.fxgraph" }, 0);
				}
				break;
			}
			case FaceType::NoLock:
			{
				renderer_base.SetVertexElements(no_lock_vertex_elements);
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_nolock.fxgraph" }, 0);
				break;
			}
			case FaceType::XZLock:
			{
				if (rotation_locked)
				{
					renderer_base.SetVertexElements(rotation_lock_vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xzlocked_rotationlocked.fxgraph" }, 0);
				}
				else
				{
					renderer_base.SetVertexElements(vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_xzlocked.fxgraph" }, 0);
				}
				break;
			}
			case FaceType::YZLock:
			{
				if (rotation_locked)
				{
					renderer_base.SetVertexElements(rotation_lock_vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_yzlocked_rotationlocked.fxgraph" }, 0);
				}
				else
				{
					renderer_base.SetVertexElements(vertex_elements);
					shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/particle_yzlocked.fxgraph" }, 0);
				}
				break;
			}
			}
		}
		
		shader_base.AddEffectGraphs(material->GetEffectGraphFilenames(), 0);

		if (number_of_frames_x > 1 || number_of_frames_y > 1) // animated particles
			shader_base.AddEffectGraphs({ anim_play_once ? L"Metadata/EngineGraphs/particle_animate_once.fxgraph" : L"Metadata/EngineGraphs/particle_animate.fxgraph" }, 0);

		auto _blend_mode = blend_mode;
		if (material->GetBlendMode(true) != Renderer::DrawCalls::BlendMode::NumBlendModes)
			_blend_mode = material->GetBlendMode(true);
		shader_base.SetBlendMode(_blend_mode);
		
		Shader::System::Get().Warmup(shader_base);

		renderer_base.SetShaderBase(shader_base);
		Renderer::System::Get().Warmup(renderer_base);

	}

}

