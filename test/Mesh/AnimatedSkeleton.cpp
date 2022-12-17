#include <iostream>
#include <sstream>

#include "Common/Resource/ResourceCache.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Simd/Utils.h"

#include "Visual/Animation/AnimationSet.h"
#include "Visual/Mesh/AnimatedSkeletonDataStructures.h"

#include "AnimatedSkeleton.h"
#include "AnimatedLightState.h"
#include "BoneConstants.h"

#define ANIMATION_LAZY_LOADING

namespace Mesh
{

	namespace
	{
		const char* AuxBoneSuffix = "_aux";
	}

	struct AnimatedSkeleton::TrackData
	{
		unsigned index;
		float duration;
		FileBoneAnimDesc bone_anim_desc;
		Animation::AnimationSet::VectorTrack scale;
		Animation::AnimationSet::QuaternionTrack rotation;
		Animation::AnimationSet::VectorTrack translation;
		Animation::AnimationSet::VectorTrack aux_scale;
		Animation::AnimationSet::QuaternionTrack aux_rotation;
		Animation::AnimationSet::VectorTrack aux_translation;
	};

	class AnimatedSkeleton::Reader : public FileReader::ASTReader
	{
	private:
		struct TrackData : public Track
		{
			unsigned int animation_index;
			unsigned int reference_index;

			TrackData(Track&& t, unsigned int anim_index, unsigned int ref_index) : Track(std::move(t)), animation_index(anim_index), reference_index(ref_index) {}
		};

		AnimatedSkeleton* parent;
		Memory::Vector<TrackData, Memory::Tag::AnimationTrack> light_tracks;
		Memory::Vector<TrackData, Memory::Tag::AnimationTrack> bone_tracks;
		Memory::Vector<size_t, Memory::Tag::AnimationTrack> animation_memory_sizes;
#if !defined(ANIMATION_LAZY_LOADING)
		Memory::Vector<Animation, Memory::Tag::AnimationTrack> animation_list;
#endif
		const std::wstring& filename;

		struct ConvertedTracks
		{
			::Animation::AnimationSet::VectorTrack scale;
			::Animation::AnimationSet::QuaternionTrack rotation;
			::Animation::AnimationSet::VectorTrack translation;
			::Animation::AnimationSet::VectorTrack aux_scale;
			::Animation::AnimationSet::QuaternionTrack aux_rotation;
			::Animation::AnimationSet::VectorTrack aux_translation;
		};

		static void ConvertTracks(const Track& src, ConvertedTracks* dst, void*& mem)
		{
			::Animation::AnimationSet::Vector3WithKey* v3_ptr = reinterpret_cast<::Animation::AnimationSet::Vector3WithKey*>(mem);

			for (size_t a = 0; a < src.scales.size(); a++)
			{
				v3_ptr[a].first = src.scales[a].first;
				v3_ptr[a].second = src.scales[a].second;
			}
			dst->scale = std::make_pair(v3_ptr, static_cast< unsigned >( src.scales.size() ) );
			v3_ptr += src.scales.size();

			for (size_t a = 0; a < src.translations.size(); a++)
			{
				v3_ptr[a].first = src.translations[a].first;
				v3_ptr[a].second = src.translations[a].second;
			}
			dst->translation = std::make_pair(v3_ptr, static_cast< unsigned >( src.translations.size() ) );
			v3_ptr += src.translations.size();

			for (size_t a = 0; a < src.aux_scales.size(); a++)
			{
				v3_ptr[a].first = src.aux_scales[a].first;
				v3_ptr[a].second = src.aux_scales[a].second;
			}
			dst->aux_scale = std::make_pair(v3_ptr, static_cast< unsigned >( src.aux_scales.size() ) );
			v3_ptr += src.aux_scales.size();

			for (size_t a = 0; a < src.aux_translations.size(); a++)
			{
				v3_ptr[a].first = src.aux_translations[a].first;
				v3_ptr[a].second = src.aux_translations[a].second;
			}
			dst->aux_translation = std::make_pair(v3_ptr, static_cast< unsigned >( src.aux_translations.size() ) );
			v3_ptr += src.aux_translations.size();

			mem = v3_ptr;

			::Animation::AnimationSet::QuaternionWithKey* q_ptr = reinterpret_cast<::Animation::AnimationSet::QuaternionWithKey*>(mem);

			for (size_t a = 0; a < src.rotations.size(); a++)
			{
				q_ptr[a].first = src.rotations[a].first;
				q_ptr[a].second = src.rotations[a].second;
			}
			dst->rotation = std::make_pair(q_ptr, static_cast< unsigned >( src.rotations.size() ) );
			q_ptr += src.rotations.size();

			for (size_t a = 0; a < src.aux_rotations.size(); a++)
			{
				q_ptr[a].first = src.aux_rotations[a].first;
				q_ptr[a].second = src.aux_rotations[a].second;
			}
			dst->aux_rotation = std::make_pair(q_ptr, static_cast< unsigned >( src.aux_rotations.size() ) );
			q_ptr += src.aux_rotations.size();

			mem = q_ptr;
		}

	public:
		Reader(AnimatedSkeleton* parent, const std::wstring& filename) : parent(parent), filename(filename) {}

		void SetNumberBones(unsigned int num_bones, unsigned char num_noinfluence_bones) override
		{
			parent->number_of_noninfluence_bones = num_noinfluence_bones;
			parent->bones.resize(num_bones);
			parent->bone_offsets.resize(num_bones);
		}
		void SetNumberLights(unsigned int num_lights) override
		{
			parent->lights.resize(num_lights);
		}
		void SetNumberAnimations(unsigned int num_animations) override
		{
			parent->animations.resize(num_animations);
			animation_memory_sizes.resize(num_animations, 0);
#if !defined(ANIMATION_LAZY_LOADING)
			animation_list.resize(num_animations);
#endif
		}
		void SetBone(unsigned int index, const Bone& bone) override
		{
			auto& pbone = parent->bones[index];
			pbone.id = index;
			pbone.child = (bone.child_index != INVALID_JOINT_INDEX) ? &parent->bones[bone.child_index] : nullptr;
			pbone.sibling = (bone.sibling_index != INVALID_JOINT_INDEX) ? &parent->bones[bone.sibling_index] : nullptr;
			pbone.name = bone.name;
			pbone.local = bone.local_transform;
			SetBoneHasAuxTrack(index, bone.has_aux_track);
		}
		void SetBoneHasAuxTrack(unsigned int index, bool hasAuxTrack) override
		{
			parent->bones[index].has_aux_keys = hasAuxTrack;
			if(hasAuxTrack)
				parent->has_uv_and_alpha_joints = true;
		}
		void SetLight(unsigned int index, const Light& light) override
		{
			auto& plight = parent->lights[index];
			plight.is_spotlight = light.is_spotlight;
			plight.colour = light.colour;
			plight.position = light.position;
			plight.direction = light.direction;
			plight.radius = light.radius;
			plight.penumbra = light.penumbra;
			plight.umbra = light.umbra;
			plight.casts_shadow = light.casts_shadow;
			plight.usage_frequency = light.usage_frequency;
			plight.dist_threshold = light.dist_threshold;
			plight.profile = light.profile;
			plight.name = light.name;
		}

		::Animation::AnimationSet::PlaybackMode GetPlaybackMode(FileReader::ASTReader::PlaybackType playback_type)
		{
			switch (playback_type)
			{
				case PlaybackType::Loop:
				case PlaybackType::LoopNoInterpolation:
					return::Animation::AnimationSet::PLAY_LOOPING;

				case PlaybackType::Once:
					return ::Animation::AnimationSet::PLAY_ONCE;

				default:
					return::Animation::AnimationSet::PLAY_ONCE;
			}
		}

		::Animation::AnimationSet::AnimationType GetAnimationType(FileReader::ASTReader::AnimationType animation_type)
		{
			switch (animation_type)
			{
				case AnimationType::Additive:	return ::Animation::AnimationSet::AnimationType::Additive;
				default:						return ::Animation::AnimationSet::AnimationType::Default;
			}
		}

		void SetAnimation(unsigned int index, const Animation& animation) override
		{
			const auto playback_mode = GetPlaybackMode(animation.playback);
			const auto animation_type = GetAnimationType(animation.animation_type);
			
			parent->animations[index].animation_set = std::make_unique<::Animation::AnimationSet>(animation.name, index);
			parent->animations[index].data_offset = animation.data_offset;
			parent->animations[index].data_size = animation.data_size;
			parent->animations[index].number_of_bones = animation.number_of_bones;
			parent->animations[index].number_of_aux_bones = animation.number_of_aux_bones;
#if !defined(ANIMATION_LAZY_LOADING)
			animation_list[index] = animation;
#endif
			auto& panim = parent->animations[index].animation_set;
			panim->SetTicksPerSecond(animation.frames_per_second);
			panim->SetPlaybackMode(playback_mode);
			panim->SetAnimationType(animation_type);
		}
		void AddLightTrack(unsigned int animation_index, unsigned int light_index, Track&& track) override
		{
			auto& panim = parent->animations[animation_index].animation_set;
			panim->SetDuration(track.duration);

			const size_t num_v3 = track.aux_scales.size() + track.aux_translations.size() + track.scales.size() + track.translations.size();
			const size_t num_q = track.rotations.size() + track.aux_rotations.size();
			const size_t size = (num_v3 * sizeof(::Animation::AnimationSet::Vector3WithKey)) + (num_q * sizeof(::Animation::AnimationSet::QuaternionWithKey));
			animation_memory_sizes[animation_index] += size;

			light_tracks.emplace_back(std::move(track), animation_index, light_index);
		}

		void AddBoneTrack(unsigned int animation_index, unsigned int bone_index, Track&& track) override
		{
			auto& panim = parent->animations[animation_index].animation_set;
			panim->SetDuration(track.duration);

			const size_t num_v3 = track.aux_scales.size() + track.aux_translations.size() + track.scales.size() + track.translations.size();
			const size_t num_q = track.rotations.size() + track.aux_rotations.size();
			const size_t size = (num_v3 * sizeof(::Animation::AnimationSet::Vector3WithKey)) + (num_q * sizeof(::Animation::AnimationSet::QuaternionWithKey));
			animation_memory_sizes[animation_index] += size;

			bone_tracks.emplace_back(std::move(track), animation_index, bone_index);
		}

		void CopyTracks()
		{
			ConvertedTracks ntrack;

			Memory::Vector<void*, Memory::Tag::AnimationTrack> mems;
			mems.resize(animation_memory_sizes.size(), nullptr);
			for (size_t a = 0; a < animation_memory_sizes.size(); a++)
			{
				if (animation_memory_sizes[a] == 0 || !parent->animations[a].data.empty())
					continue;

				parent->animations[a].data.resize(animation_memory_sizes[a]);
				mems[a] = parent->animations[a].data.data();
			}

			for (const auto& track : light_tracks)
			{
				if (mems[track.animation_index] == nullptr)
					continue;

				ConvertTracks(track, &ntrack, mems[track.animation_index]);
				auto& panim = parent->animations[track.animation_index].animation_set;
				panim->AddAuxiliaryTrack(track.reference_index, ntrack.scale);
				panim->AddAuxiliaryTrack(track.reference_index, ntrack.translation);
				panim->AddAuxiliaryTrack(track.reference_index, ntrack.aux_scale);
				panim->AddAuxiliaryTrack(track.reference_index, ntrack.aux_translation);
			}
			for (const auto& track : bone_tracks)
			{
				if (mems[track.animation_index] == nullptr)
					continue;

				ConvertTracks(track, &ntrack, mems[track.animation_index]);
				auto& panim = parent->animations[track.animation_index].animation_set;

				panim->AddTransformTrack(ntrack.scale, ntrack.rotation, ntrack.translation);

				if (track.aux_scales.size() > 0)
				{
					panim->AddAuxiliaryTrack(track.reference_index, ntrack.aux_scale);
				}
				if (track.aux_translations.size() > 0)
				{
					panim->AddAuxiliaryTrack(track.reference_index, ntrack.aux_translation);
				}
			}

			for (size_t a = 0; a < animation_memory_sizes.size(); a++)
			{
				if (animation_memory_sizes[a] == 0)
					continue;

				parent->animations[a].animation_set->SetLoaded();
			}
		}

		virtual void SetCompressionInfo(CompressionInfo compression_info) override
		{
#if defined(ANIMATION_LAZY_LOADING)
			parent->compression_info = Memory::Pointer<FileReader::ASTReader::CompressionInfo, Memory::Tag::AnimationData>::make(std::move(compression_info));
#else
			ReadAllCompressedAnimations(filename, compression_info, animation_list, *this);
#endif
		}
	};

	AnimatedSkeleton::AnimatedSkeleton( const std::wstring& filename, Resource::Allocator& )
		: filename( filename )
		, number_of_noninfluence_bones( 0 )
		, has_uv_and_alpha_joints( false )
	{
		PROFILE;

		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Animation));

		Reader reader = Reader(this, this->filename);
		FileReader::ASTReader::Read(filename, reader);
		reader.CopyTracks();

		auto identity = simd::matrix::identity();
		bones[0].GenerateOffsetTransforms(bone_offsets.data(), identity);

		BuildBoneDefMatrices();
	}

	size_t AnimatedSkeleton::GetNumBonesWithAuxKeys() const
	{
		std::size_t num_aux_bones = 0;

		for (const Mesh::AnimatedSkeleton::Bone& bone : bones)
		{
			if (bone.has_aux_keys)
			{
				num_aux_bones++;
			}
		}

		return num_aux_bones;
	}

	unsigned AnimatedSkeleton::GetBoneIndexByName(const std::string& name) const
	{
		for (unsigned i = 0; i < bones.size(); ++i)
		{
			if (bones[i].name == name)
			{
				return i;
			}
		}
		return Bones::Invalid;
	}

	unsigned AnimatedSkeleton::GetBoneIndexByName(const char* name) const
	{
		for (unsigned i = 0; i < bones.size(); ++i)
		{
			if (bones[i].name == name)
			{
				return i;
			}
		}
		return Bones::Invalid;
	}

	unsigned AnimatedSkeleton::GetLightIndexByName(const std::string& light_name) const
	{
		for (unsigned i = 0; i < lights.size(); ++i)
		{
			if (lights[i].name == light_name)
			{
				return i;
			}
		}

		return Bones::Invalid;
	}

	void AnimatedSkeleton::Bone::GenerateOffsetTransforms(simd::matrix *const bone_offsets, const simd::matrix &parent)
	{
		const auto combined = local * parent;
		if(sibling)
			sibling->GenerateOffsetTransforms(bone_offsets, parent);
		if(child)
			child->GenerateOffsetTransforms(bone_offsets, combined);
		bone_offsets[id] = combined.inverse();
		reverse_offset = combined;
	}

	void AnimatedSkeleton::GetCurrentBoneLayout( const simd::matrix* animated_transforms, std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const
	{
		positions_out.resize( bones.size() );
		auto identity = simd::matrix::identity();
		bones[ 0 ].GetCurrentBoneLayout(animated_transforms, identity, 0, positions_out, indices_out);
	}

	void AnimatedSkeleton::BuildBoneDefMatrices()
	{
		bone_def_matrices.resize(GetNumBones());
		for (size_t i = 0; i < bones.size(); i++)
		{
			bone_def_matrices[bones[i].id] = bones[i].reverse_offset;
		}
	}

	simd::matrix AnimatedSkeleton::GetBoneDefMatrix(size_t bone_index) const
	{
		return bone_def_matrices[bone_index];
	}

	void AnimatedSkeleton::Bone::GetCurrentBoneLayout( const simd::matrix* animated_transforms, const simd::matrix& parent_transform, unsigned parent_index, std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const
	{
		const auto combined = (animated_transforms != nullptr) ? animated_transforms[id] * parent_transform : parent_transform;
		if(child)
			child->GetCurrentBoneLayout(animated_transforms, combined, id, positions_out, indices_out );
		if(sibling)
			sibling->GetCurrentBoneLayout(animated_transforms, parent_transform, parent_index, positions_out, indices_out );

		static const simd::vector3 origin( 0.0f, 0.0f, 0.0f );
		
		positions_out[ id ] = combined.mulpos(origin);
		indices_out.push_back( parent_index );
		indices_out.push_back( id );
	}

	void AnimatedSkeleton::LoadAnimation(uint32_t index) const
	{
		PROFILE;

		assert(compression_info);

		Reader reader = Reader(const_cast<AnimatedSkeleton*>(this), filename);
		reader.SetNumberAnimations( static_cast< unsigned >( animations.size() ) );
		Memory::Vector<uint8_t, Memory::Tag::AnimationData> animation_data;
		FileReader::ASTReader::ReadCompressedAnimationData(filename, animation_data, index, animations[index].number_of_bones, animations[index].data_offset, animations[index].data_size, *compression_info, reader);
		reader.CopyTracks();
		animations[index].animation_set->SetLoaded();
	}

	Animation::AnimationSet* AnimatedSkeleton::GetAnimationByIndex(unsigned index) const
	{
#if defined(ANIMATION_LAZY_LOADING)
		{
			Memory::SpinLock lock(animation_mutex);
			if (!animations[index].animation_set->Loaded())
				LoadAnimation(index);
		}
#endif
		return animations[index].animation_set.get();
	}
}
