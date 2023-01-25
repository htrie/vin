#include "AnimationSet.h"
#include "AnimationTracks.h"

namespace Animation
{
	
	template<typename V, typename T>
	void SampleTrackKeyValue( unsigned int track, float sample_time, V* keys_values, float& current_key, float& previous_key, T& current_value, T& previous_value, float ticks_per_second )
	{
		sample_time *= ticks_per_second;

		// Determine the index into each animation sequence for the specified time.
		std::size_t mid, lo = 0, hi = keys_values->second-1;
		while( lo < hi )
		{
			mid = (lo + hi) / 2;
			assert(lo < hi);

			if( keys_values->first[mid].first < sample_time )
			{
				lo = mid + 1;
			}
			else
			{
				hi = mid;
			}
		}

		assert( lo < keys_values->second );
		assert( lo >= 0 );

		current_key = keys_values->first[lo].first / ticks_per_second;
		current_value  = keys_values->first[lo].second;

		if( lo > 0 ) --lo; 

		previous_key = keys_values->first[lo].first / ticks_per_second;
		previous_value = keys_values->first[lo].second;
	}

	template<typename V, typename T>
	void FindKeyFrames(float start_time, float end_time, V* keys_values, T& res, float ticks_per_second, float& last_time)
	{
		if (keys_values->second == 0)
			return;

		const float scaled_start_time = start_time * ticks_per_second;
		const float scaled_end_time = end_time * ticks_per_second;

		size_t mid, lo = 0, hi = keys_values->second - 1;
		while (lo < hi)
		{
			mid = (lo + hi) / 2;

			if (keys_values->first[mid].first < scaled_start_time)
				lo = mid + 1;
			else
				hi = mid;
		}

		for (float a = std::ceil(scaled_start_time); a <= std::ceil(scaled_end_time + 1.0f); a += 1.0f)
		{
			const auto scaled_time = a / ticks_per_second;

			if ((last_time < end_time && scaled_time > last_time) || (scaled_time >= end_time && scaled_time < last_time))
				last_time = scaled_time;

			if(a <= scaled_end_time)
				res.push_back(scaled_time);
		}

		const auto first = res.size();
		for (; lo < keys_values->second; lo++)
		{
			const float scaled_time = keys_values->first[lo].first / ticks_per_second;

			if ((last_time < end_time && scaled_time > last_time) || (scaled_time >= end_time && scaled_time < last_time))
				last_time = scaled_time;

			if (keys_values->first[lo].first <= scaled_end_time)
			{
				if(keys_values->first[lo].first >= scaled_start_time && (res.size() == first || res.back() < scaled_time))
					res.push_back(scaled_time);
			}
			else
				break;
		}
	}

	void SampleTrack( float time, const AnimationSet::VectorTrack& keys_values, AnimationSet::Vector3WithKey* current, AnimationSet::Vector3WithKey* previous, float ticks_per_second )
	{
		time *= ticks_per_second;

		// Determine the index into each animation sequence for the specified time.
		std::size_t mid, lo = 0, hi = keys_values.second-1;
		while( lo < hi )
		{
			mid = (lo + hi) / 2;
			assert(lo < hi);

			if( keys_values.first[mid].first < time )
			{
				lo = mid + 1;
			}
			else
			{
				hi = mid;
			}
		}

		assert( lo < keys_values.second );
		assert( lo >= 0 );

		*current = std::make_pair( keys_values.first[lo].first / ticks_per_second, keys_values.first[lo].second );
		if( lo > 0 ) --lo; 
		*previous = std::make_pair( keys_values.first[lo].first / ticks_per_second, keys_values.first[lo].second );
	}


	AnimationSet::AnimationSet( const std::string name_, uint32_t index )
	:	name(name_),
		index(index),
		loaded(false),
		duration(std::numeric_limits<float>::min() )
	{}

	void AnimationSet::SetDuration( float new_duration ) 
	{ 
		if( duration != new_duration )
		{
			duration = new_duration; 
		}
	}

	void AnimationSet::SetPlaybackMode( PlaybackMode mode ) 
	{ 
		playback_mode = mode; 
	}

	void AnimationSet::SetAnimationType(AnimationType type)
	{
		animation_type = type;
	}
	
	void AnimationSet::SetTicksPerSecond( float tps ) 
	{ 
		ticks_per_second = tps; 
	}

	void AnimationSet::AddAuxiliaryTrack( unsigned index, VectorTrack track )
	{
		auxiliary_tracks.emplace_back( index, std::move( track ) );
	}

	void AnimationSet::AddTransformTrack( VectorTrack scale_track, QuaternionTrack rotation_track, VectorTrack translation_track )
	{
		primary_tracks.emplace_back( std::move( scale_track ), std::move( rotation_track ), std::move( translation_track ) );
	}

	void AnimationSet::SampleTransformTracks( float time, FullSample& current, FullSample& previous )
	{
		for( unsigned int i = 0; i < primary_tracks.size(); ++i )
		{
			SampleScaleFromSingleTransformTrack( i, time, current, previous );
			SampleRotationFromSingleTransformTrack( i, time, current, previous );
			SampleTranslationFromSingleTransformTrack( i, time, current, previous );
		}
	}

	void AnimationSet::SampleAuxiliaryTracks( float time, AuxiliarySample& current, AuxiliarySample& previous )
	{
		if( current.num_auxiliary < auxiliary_tracks.size() || previous.num_auxiliary < auxiliary_tracks.size() )
			return;

		for( unsigned i = 0; i < auxiliary_tracks.size(); ++i )
		{
			current.indices[i] = auxiliary_tracks[i].index;
			SampleSingleAuxiliaryTrack( i, time, &current.values[i], &previous.values[i] );
		}
	}
	
	void AnimationSet::SampleScaleFromSingleTransformTrack( unsigned index, float time, FullSample& current, FullSample& previous )
	{
		float current_key;
		float previous_key;

		simd::vector3 current_value;
		simd::vector3 previous_value;

		SampleTrackKeyValue( index, time, &primary_tracks[index].scale, current_key, previous_key, current_value, previous_value, ticks_per_second );

		current.SetScaleKey(current_key, index);
		previous.SetScaleKey(previous_key, index);

		current.SetScale(current_value, index);
		previous.SetScale(previous_value, index);
	}

	void AnimationSet::SampleRotationFromSingleTransformTrack( unsigned index, float time, FullSample& current, FullSample& previous )
	{
		float current_key;
		float previous_key;

		simd::quaternion current_value;
		simd::quaternion previous_value;

		SampleTrackKeyValue( index, time, &primary_tracks[index].rotation, current_key, previous_key, current_value, previous_value, ticks_per_second );

		current.SetRotationKey(current_key, index);
		previous.SetRotationKey(previous_key, index);

		current.SetRotation(current_value, index);
		previous.SetRotation(previous_value, index);
	}

	void AnimationSet::SampleTranslationFromSingleTransformTrack( unsigned index, float time, FullSample& current, FullSample& previous )
	{
		float current_key;
		float previous_key;

		simd::vector3 current_value;
		simd::vector3 previous_value;

		SampleTrackKeyValue( index, time, &primary_tracks[index].translation, current_key, previous_key, current_value, previous_value, ticks_per_second );

		current.SetTranslationKey(current_key, index);
		previous.SetTranslationKey(previous_key, index);

		current.SetTranslation(current_value, index);
		previous.SetTranslation(previous_value, index);
	}

	Memory::SmallVector<float, 256, Memory::Tag::AnimationTrack> AnimationSet::FindKeyFramesInRange(float start_time, float end_time, float min_delta, float& last_time)
	{
		Memory::SmallVector<float, 256, Memory::Tag::AnimationTrack> res;
		for (const auto& track : primary_tracks)
		{
			FindKeyFrames(start_time, end_time, &track.translation, res, ticks_per_second, last_time);
			FindKeyFrames(start_time, end_time, &track.rotation, res, ticks_per_second, last_time);
			FindKeyFrames(start_time, end_time, &track.scale, res, ticks_per_second, last_time);
			res.resize(TracksReduceNextKeyFrames(res.data(), res.size(), end_time, min_delta));
		}

		for (const auto& track : auxiliary_tracks)
		{
			FindKeyFrames(start_time, end_time, &track.auxiliary, res, ticks_per_second, last_time);
			res.resize(TracksReduceNextKeyFrames(res.data(), res.size(), end_time, min_delta));
		}

		return res;
	}
	
	void AnimationSet::SampleSingleAuxiliaryTrack( unsigned index, float time, Vector3WithKey* current, Vector3WithKey* previous )
	{
		// NOTE: This code was added because if you have 1 animation that hides a light, that animation will not have the auxiliary tracks for the light.
		if( !(index < auxiliary_tracks.size()) )
			return;

		SampleTrack( time, auxiliary_tracks[index].auxiliary, current, previous, ticks_per_second );
	}

}