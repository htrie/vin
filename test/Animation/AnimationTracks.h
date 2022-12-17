#pragma once

#include "Visual/Mesh/AnimatedLightState.h"
#include "Visual/Mesh/AnimatedSkeleton.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"

namespace Animation
{
	namespace MemoryStats
	{
		void NextFrame();
		void OutputStats(std::ostream& stream);
		void ResetStats();
	}

	typedef Memory::Vector<simd::matrix, Memory::Tag::AnimationPalette> MatrixPalette;
	typedef Memory::Vector<simd::vector4, Memory::Tag::AnimationPalette> Vector4Palette;

	struct Tracks;

	void InitializeAnimationTracks();
	void DeinitializeAnimationTracks();
	void ClearPools();

	Tracks* TracksCreate(const Mesh::AnimatedSkeletonHandle& skeleton);
	void TracksDestroy(Tracks* tracks);
	
	const simd::matrix* TracksGetTransformPalette(const Tracks* tracks);
	const std::shared_ptr<MatrixPalette>& TracksGetFinalTransformPalette(const Tracks* tracks);
	const std::shared_ptr<Vector4Palette>& TracksGetUVAlphaPalette(const Tracks * tracks);
	int TracksGetPaletteSize(const Tracks* tracks);
	const AABB TracksGetAABB(const Tracks* tracks);
	const Mesh::LightStates& TracksGetCurrentLightState(const Tracks* tracks);

	void TracksSetFinalTransformPalette(Tracks* tracks, const simd::matrix& m, unsigned int index);

	void TracksRenderFrameMove( Tracks* tracks, const Mesh::AnimatedSkeletonHandle& skeleton, const float elapsed_time );

	void TracksOnAnimationStart( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode, const float weight );
	void TracksOnPrimaryAnimationStart( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode, const float blend_ratio );
	void TracksOnSecondaryAnimationStart( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode, const float blend_ratio );
	void TracksAppendAnimation( Tracks* tracks, AnimationSet* animation_set, const uint32_t layer, const std::vector<unsigned>* mask_bone_indices, const float animation_speed_multiplier, const float blend_time, const float begin, const float end, const float elapsed, AnimationSet::PlaybackMode playback_mode );
	void TracksFadeAllAnimations(Tracks* tracks, const float blend_time, const uint32_t layer = 0.0f);
	void TracksOnAnimationSpeedChange(Tracks* tracks, const float animation_speed_multiplier, const uint32_t layer = 0);
	void TracksOnSecondaryBlendRatioChange( Tracks* tracks, const float blend_ratio );
	void TracksOnGlobalAnimationSpeedChange( Tracks* tracks, const float new_speed, const float old_speed );
	void TracksOnAnimationPositionSet(Tracks* tracks, const float animation_elapsed_time, const uint32_t layer = 0);
	float TracksGetCurrentAnimationPosition( Tracks* tracks, const uint32_t layer = 0 );
	
	uint32_t TracksGetAnimationIndex(const Tracks* tracks, uint32_t layer);

	Memory::SmallVector<float, 256, Memory::Tag::AnimationTrack> TracksGetNextKeyFrames(const Tracks* tracks, float elapsed_time, float* next_keyframe, float min_delta = 0.001f);
	size_t TracksReduceNextKeyFrames(float* times, size_t count, float elapsed_time, float min_delta = 0.001f);

	// Just for debugging output
	std::vector< std::pair< uint32_t, uint32_t > > TracksGetLayeredAnimationIndices( const Tracks* tracks );

#if DEBUG_GUI_ENABLED
	enum DebugGUIStep { ProgressBar, Weight, Speed, Additional };
	void RenderTracksDebugGui(const Tracks* const tracks, DebugGUIStep step, bool display_additional = true, bool display_previous = false);
#endif
}
