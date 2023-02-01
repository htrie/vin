#pragma once

#include "Common/File/InputFileStream.h"
#include "Common/Resource/Handle.h"
#include "Common/FileReader/ASTReader.h"

#include "Visual/Animation/AnimationSet.h"
#include "Common/Resource/DeviceUploader.h"
#include "AnimatedSkeletonDataStructures.h"

namespace Animation { class AnimationSet; }

namespace Mesh
{

	const size_t MaxNumBones = 255;

	struct AnimatedLightState;

	class AnimatedSkeleton
	{
	private:
		class Reader;
	public:

		simd::matrix GetBoneDefMatrix(size_t bone_index) const;

	#pragma pack(push)
	#pragma pack(1)
		struct Bone
		{
			void GenerateOffsetTransforms(simd::matrix *const bone_offsets, const simd::matrix &parent);
			void GetCurrentBoneLayout( const simd::matrix* animated_transforms, const simd::matrix& parent_transform, unsigned parent_index, std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const;

			simd::matrix local;
			simd::matrix reverse_offset;

			std::string name;
			Bone* child = nullptr;
			Bone* sibling = nullptr;
			unsigned int id = 0;
			bool has_aux_keys = false;
			unsigned char sibling_index = 0;
			unsigned char child_index = 0;
			unsigned char name_length = 0;
		};

		struct Light
		{
			std::string name;
			simd::vector3 colour;
			simd::vector3 position;
			simd::vector3 direction;
			float radius = 0.0f;
			float penumbra = 0.0f;
			float umbra = 0.0f;
			float dist_threshold = 0.0f;
			bool casts_shadow = false;
			bool is_spotlight = false;
			unsigned char name_length = 0;
			unsigned char usage_frequency = 0; ///< 0 = Low, 1 = Medium, 2 = High
			uint32_t profile = 0u;
		};
	#pragma pack(pop)

	public:
		AnimatedSkeleton( const std::wstring& filename, Resource::Allocator& );
		
		const std::wstring& GetFilename() const { return filename; }
		bool HasUvAndAlphaJoints() const { return has_uv_and_alpha_joints; }
		const Memory::Vector< Light, Memory::Tag::AnimationSkeleton >& GetLights() const { return lights; }
		unsigned GetLightIndexByName( const std::string& light_name ) const;
		::Animation::AnimationSet* GetAnimationByIndex( unsigned index ) const;

		size_t GetNumBones() const { return bones.size(); }
		const Memory::Vector< Bone, Memory::Tag::AnimationSkeleton >& GetBones() const { return bones; }
		const simd::matrix* GetBoneOffsets() const { return bone_offsets.data(); }
		size_t GetNumInfluenceBones( ) const { return bones.size() - number_of_noninfluence_bones; }
		size_t GetNumBonesWithAuxKeys() const;
		const std::string& GetBoneName( size_t i ) const { assert( i < bones.size() ); return bones[ i ].name; }
		unsigned GetBoneIndexByName( const std::string& name ) const;
		unsigned GetBoneIndexByName( const char* name ) const;
		void GetCurrentBoneLayout( const simd::matrix* animated_transforms, std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const;
		const simd::matrix& GetBindSpaceTransform( const size_t i ) const { return bones[ i ].reverse_offset; }

	private:
		struct TrackData;
		struct AnimationData
		{
			Memory::Vector<uint8_t, Memory::Tag::AnimationData> data;
			std::unique_ptr<::Animation::AnimationSet> animation_set;
			uint32_t number_of_bones;
			uint32_t number_of_aux_bones;
			uint32_t data_offset;
			uint32_t data_size;
		};

		Memory::Pointer<FileReader::ASTReader::CompressionInfo, Memory::Tag::AnimationData> compression_info;
		Memory::Vector<Bone, Memory::Tag::AnimationSkeleton> bones;
		Memory::Vector<Light, Memory::Tag::AnimationSkeleton> lights;
		Memory::Vector<simd::matrix, Memory::Tag::AnimationSkeleton> bone_offsets;
		Memory::Vector<simd::matrix, Memory::Tag::AnimationSkeleton> bone_def_matrices;
		mutable Memory::Vector<AnimationData, Memory::Tag::AnimationData> animations;
		unsigned int number_of_noninfluence_bones;
		bool has_uv_and_alpha_joints;
		std::wstring filename;

		void BuildBoneDefMatrices();
		void LoadAnimation(uint32_t index) const;

		Memory::SpinMutex animation_mutex;
	};

	typedef Resource::Handle< AnimatedSkeleton > AnimatedSkeletonHandle;

}
