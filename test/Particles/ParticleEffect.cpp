#include "Visual/Material/Material.h"
#include "Visual/Mesh/FixedMesh.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"
#include "ParticleEffect.h"

#include "Common/FileReader/PETReader.h"

namespace Particles
{
	class ParticleEffect::Reader : public FileReader::PETReader
	{
	private:
		ParticleEffect* parent;
	public:
		Reader(ParticleEffect* parent, Resource::Allocator& allocator) : parent(parent), PETReader(allocator) {}

		void SetNumEmitters(size_t num) override 
		{
			parent->emitter_templates.resize(num);
		}
		Resource::UniquePtr<Emitter> GetEmitter(size_t index, Resource::Allocator& allocator) override 
		{ 
			return parent->emitter_templates[index].GetReader(allocator);
		}

		void SetNumGpuEmitters(size_t num) override
		{
			parent->gpu_emitter_templates.resize(num);
		}
		void SetGpuEmitterData(size_t index, const GpuParticles::EmitterTemplate &template_data) override
		{
			parent->gpu_emitter_templates[index] = template_data;

			parent->gpu_emitter_templates[index].render_material.Fetch();
			parent->gpu_emitter_templates[index].update_material.Fetch();
			parent->gpu_emitter_templates[index].particle_mesh.Fetch();
		}
	};

	ParticleEffect::ParticleEffect( const std::wstring &filename, Resource::Allocator& allocator)
	{
		FileReader::PETReader::Read(filename, Reader(this, allocator));

		for (auto& emitter_template : emitter_templates)
			emitter_template.Warmup();

		for (const auto& emitter_template : gpu_emitter_templates)
			GpuParticles::System::Get().Warmup(emitter_template);
	}

	void ParticleEffect::Reload(const std::wstring& filename)
	{
		FileReader::PETReader::Read(filename, Reader(this, Resource::resource_allocator));
	}

	void ParticleEffect::AddEmitterTemplate()
	{
		unsigned index = static_cast< unsigned >( emitter_templates.size() );
		emitter_templates.resize( index + 1 );

		simd::vector4 default_colour[] =
		{
			simd::vector4(1.0f, 1.0f, 1.0f, 0.0f),simd::vector4(1.0f, 1.0f, 1.0f, 1.0f),simd::vector4(1.0f, 1.0f, 1.0f, 1.0f),
			simd::vector4(1.0f, 1.0f, 1.0f, 1.0f),simd::vector4(1.0f, 1.0f, 1.0f, 1.0f),simd::vector4(1.0f, 1.0f, 1.0f, 1.0f),
			simd::vector4(1.0f, 1.0f, 1.0f, 0.0f)
		};

		emitter_templates[ index ].enabled = true;
		emitter_templates[ index ].emitter_type = FileReader::PETReader::PointEmitter;
		emitter_templates[ index ].face_type = FileReader::PETReader::FaceCamera;
		emitter_templates[ index ].blend_mode = Renderer::DrawCalls::BlendMode::ForegroundAlphaBlend;
		emitter_templates[ index ].particle_duration = 1.0f;
		emitter_templates[ index ].is_infinite = true;
		emitter_templates[ index ].lock_mode.reset();
		emitter_templates[ index ].is_locked_to_bone = false;
		emitter_templates[ index ].is_locked_movement_to_bone = false;
		emitter_templates[ index ].enable_lmb_cone_size = false;
		emitter_templates[ index ].scale_emitter_duration = false;
		emitter_templates[ index ].scale_particle_duration = false;
		emitter_templates[ index ].rotation_lock_type = FileReader::PETReader::RotationLockType::Random;
		emitter_templates[ index ].velocity_along_variance = 0.0f;
		emitter_templates[ index ].velocity_up_variance = 0.0f;
		emitter_templates[ index ].scale_variance = 0.0f;
		emitter_templates[ index ].rotation_speed_variance = 0.0f;
		emitter_templates[ index ].min_emitter_duration = 1.0f;
		emitter_templates[ index ].max_emitter_duration = 1.0f;
		emitter_templates[ index ].stretch_variance = 0.0f;
		emitter_templates[ index ].particle_offset[ 0 ] = emitter_templates[ index ].particle_offset[ 1 ] = 0.0f;
		emitter_templates[ index ].rotation_direction = 0.0f;
		emitter_templates[ index ].rotation_offset_min = 0.0f;
		emitter_templates[ index ].rotation_offset_max = 0.0f;
		emitter_templates[ index ].position_offset_min = 0.0f;
		emitter_templates[ index ].position_offset_max = 0.0f;
		emitter_templates[ index ].instance_rotation_offset_min = 0.0f;
		emitter_templates[ index ].instance_rotation_offset_max = 0.0f;
		emitter_templates[ index ].instance_position_offset_min = 0.0f;
		emitter_templates[ index ].instance_position_offset_max = 0.0f;
		emitter_templates[ index ].emit_speed_threshold = 0.0f;
		emitter_templates[ index ].emit_speed_variance = 0.0f;
		emitter_templates[ index ].update_speed_threshold = 0.0f;
		emitter_templates[ index ].anim_speed_scale_on_death = 1.0f;
		emitter_templates[ index ].up_acceleration_variance = 0.0f;
		emitter_templates[ index ].random_seed = 0;
		emitter_templates[ index ].enable_random_seed = false;
		emitter_templates[ index ].scale_to_bone_length = 0.f;
		emitter_templates[ index ].distribute_distance = 0.f;
		emitter_templates[ index ].priority = Priority::Essential;
		emitter_templates[ index ].culling_threshold = 3;
		emitter_templates[ index ].max_scale = 100.0f; // guessed default value

		emitter_templates[ index ].emit_interval_min = 1.0f;
		emitter_templates[ index ].emit_interval_max = 1.0f;
		emitter_templates[ index ].pause_interval_min = 0.0f;
		emitter_templates[ index ].pause_interval_max = 0.0f;
		emitter_templates[ index ].start_interval_min = 0.0f;
		emitter_templates[ index ].start_interval_max = 0.0f;
		emitter_templates[ index ].scale_interval_durations = false;

		emitter_templates[index].colour_variance = simd::vector4(0.0f);
		emitter_templates[index].colour.FromArray(default_colour);
	}

	void ParticleEffect::RemoveEmitterTemplate( unsigned index )
	{
		emitter_templates.erase( emitter_templates.begin() + index );
	}

	size_t ParticleEffect::AddGpuEmitterTemplate()
	{
		size_t index = gpu_emitter_templates.size();
		gpu_emitter_templates.push_back(GpuParticles::EmitterTemplate());
		return index;
	}

	void ParticleEffect::RemoveGpuEmitterTemplate(size_t index)
	{
		gpu_emitter_templates.erase(gpu_emitter_templates.begin() + index);
	}

	bool ParticleEffect::HasInfiniteEmitter( ) const
	{
		for (const auto& a : emitter_templates)
			if (a.is_infinite)
				return true;

		for (const auto& a : gpu_emitter_templates)
			if (a.is_continuous)
				return true;

		return false;
	}

	bool ParticleEffect::HasFiniteEmitter() const
	{
		for (const auto& a : emitter_templates)
			if (!a.is_infinite)
				return true;

		for (const auto& a : gpu_emitter_templates)
			if (!a.is_continuous)
				return true;

		return false;
	}
}

