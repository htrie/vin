#pragma once

namespace Animation
{

	class AnimationSet
	{
	public:
		enum PlaybackMode
		{
			PLAY_ONCE,
			PLAY_LOOPING,
			PLAY_PING_PONG
		};

		enum class AnimationType
		{
			Default,
			Additive
		};

		struct FullSample
		{
			simd::float4_std* scale_key = nullptr;
			simd::float4_std* rotation_key = nullptr;
			simd::float4_std* translation_key = nullptr;

			simd::float4x3_std* scale = nullptr;
			simd::float4x4_std* rotation = nullptr;
			simd::float4x3_std* translation = nullptr;

			unsigned int count4;

		public:
			static size_t GetSize(size_t count4)
			{
				size_t size = 0;
				size += sizeof(simd::float4_std);
				size += sizeof(simd::float4_std);
				size += sizeof(simd::float4_std);
				size += sizeof(simd::float4x3_std);
				size += sizeof(simd::float4x4_std);
				size += sizeof(simd::float4x3_std);
				return size * count4;
			}

			FullSample() {}
			FullSample(void* mem, unsigned int count4) : count4(count4)
			{
				auto* _mem = (uint8_t*)mem;

				memset(mem, 0, GetSize(count4));

				scale_key = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
				rotation_key = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
				translation_key = (simd::float4_std*)_mem; _mem += sizeof(simd::float4_std) * count4;
				scale = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
				rotation = (simd::float4x4_std*)_mem; _mem += sizeof(simd::float4x4_std) * count4;
				translation = (simd::float4x3_std*)_mem; _mem += sizeof(simd::float4x3_std) * count4;
			}

			float GetScaleKey(unsigned int i) const { 
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				return scale_key[i4][i1];
			}
			float GetRotationKey(unsigned int i) const { 
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				return rotation_key[i4][i1];
			}
			float GetTranslationKey(unsigned int i) const { 
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				return translation_key[i4][i1];
			}

			void SetScaleKey(float f, unsigned int i) {
				assert(i < count4*4);
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				scale_key[i4][i1] = f;
			}
			void SetRotationKey(float f, unsigned int i) {
				assert(i < count4*4);
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				rotation_key[i4][i1] = f;
			}
			void SetTranslationKey(float f, unsigned int i) {
				assert(i < count4*4);
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				translation_key[i4][i1] = f;
			}

			simd::vector3 GetScale(unsigned int i) const { 
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				return simd::vector3(
					scale[i4][0][i1], 
					scale[i4][1][i1], 
					scale[i4][2][i1]);
			}
			simd::quaternion GetRotation(unsigned int i) const {
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				return simd::quaternion(
					rotation[i4][0][i1], 
					rotation[i4][1][i1], 
					rotation[i4][2][i1], 
					rotation[i4][3][i1]);
			}
			simd::vector3 GetTranslation(unsigned int i) const {
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				return simd::vector3(
					translation[i4][0][i1], 
					translation[i4][1][i1], 
					translation[i4][2][i1]);
			}

			void SetScale(const simd::vector3& s, unsigned int i) {
				assert(i < count4*4);
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				scale[i4][0][i1] = s.x;
				scale[i4][1][i1] = s.y; 
				scale[i4][2][i1] = s.z;
			}
			void SetRotation(const simd::quaternion& r, unsigned int i) {
				assert(i < count4*4);
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				rotation[i4][0][i1] = r.x;
				rotation[i4][1][i1] = r.y; 
				rotation[i4][2][i1] = r.z;
				rotation[i4][3][i1] = r.w;
			}
			void SetTranslation(const simd::vector3& t, unsigned int i) {
				assert(i < count4*4);
				const unsigned int i4 = i / 4;
				const unsigned int i1 = i % 4;
				translation[i4][0][i1] = t.x;
				translation[i4][1][i1] = t.y; 
				translation[i4][2][i1] = t.z;
			}
		};

		typedef std::pair<float, simd::vector3> Vector3WithKey;
		typedef std::pair<Vector3WithKey*, unsigned> VectorTrack;

		typedef std::pair<float, simd::quaternion> QuaternionWithKey;
		typedef std::pair<QuaternionWithKey*, unsigned> QuaternionTrack;

		struct AuxiliarySample
		{
			unsigned* indices;
			Vector3WithKey* values;
			unsigned num_auxiliary = 0;

			static size_t GetSize(size_t num_auxiliary)
			{
				return num_auxiliary * sizeof(Vector3WithKey) + num_auxiliary * sizeof(unsigned);
			}

			AuxiliarySample() {}
			AuxiliarySample(void* mem, unsigned num_auxiliary)
				: num_auxiliary(num_auxiliary)
			{
				auto* _mem = (uint8_t*)mem;

				values = (Vector3WithKey*)_mem;
				indices = (unsigned*)(_mem + num_auxiliary * sizeof(Vector3WithKey));

				memset(values, 0, num_auxiliary * sizeof(Vector3WithKey));
				memset(indices, 0, num_auxiliary * sizeof(unsigned));
			}
		};

		struct AuxiliaryTrack
		{
			unsigned index = (unsigned)-1;
			VectorTrack auxiliary;

			AuxiliaryTrack() {}
			AuxiliaryTrack(unsigned index, const VectorTrack& auxiliary)
				: index(index), auxiliary(auxiliary) {}
		};
		
		struct PrimaryTrack
		{
			VectorTrack scale;
			QuaternionTrack rotation;
			VectorTrack translation;
			
			PrimaryTrack() {}
			PrimaryTrack(const VectorTrack& scale, const QuaternionTrack& rotation, const VectorTrack& translation)
				: scale(scale), rotation(rotation), translation(translation) {}
		};

		AnimationSet( const std::string name, uint32_t index );

		float GetDuration() const { return duration / ticks_per_second; }
		float GetTicksPerSecond() const { return ticks_per_second; };
		PlaybackMode GetPlaybackMode() const { return playback_mode; }
		AnimationType GetAnimationType() const { return animation_type; }

		void SetDuration( float _duration );
		void SetPlaybackMode( PlaybackMode mode );
		void SetAnimationType(AnimationType type);
		void SetTicksPerSecond( float tps );

		void AddAuxiliaryTrack( unsigned index, VectorTrack track );
		void AddTransformTrack( VectorTrack scale_track, QuaternionTrack rotation_track, VectorTrack translation_track );

		void SampleTransformTracks( float time, FullSample& current, FullSample& previous );
		void SampleAuxiliaryTracks( float time, AuxiliarySample& current, AuxiliarySample& previous );

		void SampleScaleFromSingleTransformTrack( unsigned index, float time, FullSample& current, FullSample& previous );
		void SampleRotationFromSingleTransformTrack( unsigned index, float time, FullSample& current, FullSample& previous );
		void SampleTranslationFromSingleTransformTrack( unsigned index, float time, FullSample& current, FullSample& previous );

		Memory::SmallVector<float, 256, Memory::Tag::AnimationTrack> FindKeyFramesInRange(float start_time, float end_time, float min_delta, float& last_time);

		void SampleSingleAuxiliaryTrack( unsigned index, float time, Vector3WithKey* current, Vector3WithKey* previous );
		bool Loaded() const { return loaded; }
		void SetLoaded() { loaded = true; }

		uint32_t GetIndex() const { return index; }
		
		size_t GetPrimaryTrackCount() const { return primary_tracks.size(); }
		size_t GetAuxiliaryTrackCount() const { return auxiliary_tracks.size(); }

		const std::string& GetName() const { return name; }

	private:
		std::string name;
		float duration;
		float ticks_per_second;
		PlaybackMode playback_mode;
		AnimationType animation_type;
		std::atomic_bool loaded; // If false, sampling functions should not be called
		Memory::SmallVector<PrimaryTrack, 64, Memory::Tag::AnimationTrack> primary_tracks;
		Memory::SmallVector<AuxiliaryTrack, 64, Memory::Tag::AnimationTrack> auxiliary_tracks;
		uint32_t index;
	};
}