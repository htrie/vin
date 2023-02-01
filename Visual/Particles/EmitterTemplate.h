#pragma once

#include "Common/Resource/ChildHandle.h"
#include "Common/Utility/OsAbstraction.h"
#include "Common/Utility/DataTrack.h"
#include "Common/FileReader/PETReader.h"

#include "Visual/Material/Material.h"
#include "Visual/Mesh/FixedMesh.h"
#include "Visual/Device/DynamicCulling.h"
#include "Visual/Device/State.h"

namespace Particles
{
	typedef FileReader::PETReader::EmitterType EmitterType;
	typedef FileReader::PETReader::EmitterBlendType EmitterBlendType;
	typedef FileReader::PETReader::FaceType FaceType;
	typedef FileReader::PETReader::LockMode LockMode;
	typedef FileReader::PETReader::FlipMode FlipMode;
	typedef FileReader::PETReader::RotationLockType RotationLockType;
	typedef Device::DynamicCulling::Priority Priority;

	extern const Device::VertexElements vertex_elements;
	extern const Device::VertexElements rotation_lock_vertex_elements;
	extern const Device::VertexElements no_lock_vertex_elements;
	extern const Device::VertexElements mesh_vertex_elements;

	///Parameters of a particle emitter
	class EmitterTemplate
	{
	private:
		class Reader;
	public:
		static const unsigned NumPointsInLifetime = 7;

		Resource::UniquePtr<FileReader::PETReader::Emitter> GetReader(Resource::Allocator& allocator);
		void Write( std::wostream& stream ) const;

		unsigned MaxParticles() const;
		unsigned MinSimultaneousparticles() const; // only used for the asset viewer

		EmitterType emitter_type;
		Renderer::DrawCalls::BlendMode blend_mode;
		std::wstring texture_filename;
		MaterialHandle material;
#ifdef ENABLE_PARTICLE_FMT_SEQUENCE
		Memory::Vector<Mesh::FixedMeshHandle, Memory::Tag::Particle> mesh_handles;
#else
		Mesh::FixedMeshHandle mesh_handle;
#endif
		
		float particle_duration;
		float particle_duration_variance;
		float velocity_along_variance;
		float velocity_up_variance;
		float up_acceleration_variance;
		float scale_variance;
		float rotation_speed_variance;
		float stretch_variance;
		float min_emitter_duration;
		float max_emitter_duration;
		float particle_offset[ 2 ];
		float rotation_direction;
		float emit_speed_threshold;
		float emit_speed_variance;
		float update_speed_threshold;
		float anim_speed_scale_on_death;
		float scale_to_bone_length;
		float distribute_distance;
		float max_scale; // Maximum scale of a particle, based on scale, emit_scale and stretch. Stored here to not call CalculateMinMax on the data tracks every frame
		simd::vector3 rotation_offset_min;
		simd::vector3 rotation_offset_max;
		simd::vector3 position_offset_min;
		simd::vector3 position_offset_max;
		bool is_infinite;
		bool is_locked_to_bone; // Like locked_to_parent but using the bone's transform instead 
		bool is_locked_movement_to_bone; // Movement of particles is locked along it's parent bone group path
		bool enable_lmb_cone_size; // Enable cone size for locked-movement-to-bone
		bool scale_emitter_duration; // Scales the emitter duration according to the parent's anim speed
		bool scale_particle_duration; // Scales the particle duration according to the parent's anim speed;
		int  number_of_frames_x;
		int  number_of_frames_y;
		float anim_speed;
		bool anim_play_once; // if play the animation only once from start to end
		bool random_flip_x, random_flip_y; // if randomize flipping of each particle at their creation time
		bool reverse_bone_group;
		FlipMode flip_mode;
		int random_seed;
		bool enable_random_seed;
		bool enabled; //Only used in the asset viewer. Not saved.
		FaceType face_type;
		RotationLockType rotation_lock_type;
		std::bitset<LockMode::NumLockMode> lock_mode;
		Priority priority;
		size_t culling_threshold;

		// Mesh Particles Only:
		simd::vector3 instance_rotation_offset_min;
		simd::vector3 instance_rotation_offset_max;
		simd::vector3 instance_position_offset_min;
		simd::vector3 instance_position_offset_max;

		// Emit interval data:
		float emit_interval_min = 0;
		float emit_interval_max = 0;
		float pause_interval_min = 0;
		float pause_interval_max = 0;
		float start_interval_min = 0;
		float start_interval_max = 0;
		float emit_min_speed = -1.0f;
		float emit_max_speed = -1.0f;
		bool scale_interval_durations;
		bool use_world_speed = false;

		Memory::DebugStringA<128> ui_name;

		simd::vector4 colour_variance;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> scatter_cone_size;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> particles_per_second;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> velocity_along;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> velocity_up;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> up_acceleration;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> scale;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> emit_scale;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> rotation_speed;
		Utility::DataTrack::OptionalDataTrack<float, NumPointsInLifetime, Memory::Tag::Particle> stretch;
		Utility::DataTrack::OptionalDataTrack<simd::vector4, NumPointsInLifetime, Memory::Tag::Particle> colour;
		Utility::DataTrack::OptionalDataTrack<simd::vector4, NumPointsInLifetime, Memory::Tag::Particle> emit_colour;
		
		EmitterTemplate()
		{
			particle_duration_variance=0;
			number_of_frames_x=1;
			number_of_frames_y=1;
			anim_speed=1.0f;
			anim_play_once=false;
			random_flip_x=random_flip_y=false;
			flip_mode = FlipMode::FlipRotate;
			random_seed=0;
			enable_random_seed=false;
			reverse_bone_group = false;

			particles_per_second.SetDefault(10.0f);
			velocity_along.SetDefault(0.0f);
			velocity_up.SetDefault(0.0f);
			scale.SetDefault(10.0f);
			rotation_speed.SetDefault(0.0f);
			colour.SetDefault(simd::vector4(1.0f, 1.0f, 1.0f, 1.0f));
			stretch.SetDefault(1.0f);
			emit_scale.SetDefault(1.0f);
			up_acceleration.SetDefault(0.0f);
			scatter_cone_size.SetDefault(1.0f);
			emit_colour.SetDefault(simd::vector4(1.0f, 1.0f, 1.0f, 1.0f));
		}

		void Warmup() const;

		float particleLife() const
		{
			float scale_value = RandomFloat(-particle_duration_variance, particle_duration_variance);
			float scale_factor = ScaleFactor(scale_value);
			float life = particle_duration * scale_factor;
			return life;
		}

		bool IsLockedToBone() const
		{
			return is_locked_to_bone && !is_locked_movement_to_bone;
		}

		bool IsLockedMovementToBone() const
		{
			return is_locked_movement_to_bone;
		}

		bool UseAnimFromLife() const // sets 4th vtx normal channel to life/max_life or variance
		{
			return (number_of_frames_x > 1 || number_of_frames_y > 1) ? anim_play_once : false;
		}

		bool NeedsSorting() const
		{
			return Renderer::DrawCalls::blend_modes[(unsigned)blend_mode].needs_sorting;
		}

	};
}
