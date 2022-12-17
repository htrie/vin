
/// Weight calculation

template<typename F4>
void UpdateWeightAdditive(Tracks* tracks, Animation::AnimationInstance& instance)
{
	auto& weights = instance.animation_sample.weights;

	for (uint32_t i = 0; i < tracks->transform_count; i += 4)
	{
		F4& weight = (F4&)(weights[i]);
		weight = F4(instance.weight);
	}
}

template<typename F4, typename U4>
void UpdateWeightAdditiveMasked(Tracks* tracks, Animation::AnimationInstance& instance)
{
	auto& weights = instance.animation_sample.weights;

	for (uint32_t i = 0; i < tracks->transform_count; i += 4)
	{
		F4 bone_mask = F4::select((U4&)(instance.animation_sample.bone_masks[i]), F4(1.0f), F4(0.0f));
		F4& weight = (F4&)(weights[i]);
		weight = weight + F4(instance.weight) * bone_mask;
	}
}

template<typename F4, typename U4>
void UpdateWeightDefault(Tracks* tracks, Animation::AnimationInstance& instance, const float total_weight, const std::array<float, Mesh::MaxNumBones>& total_masked_weights)
{
	auto& weights = instance.animation_sample.weights;

	for (uint32_t i = 0; i < tracks->transform_count; i += 4)
	{
		F4& weight = (F4&)(weights[i]);
		F4& total_masked_weight = (F4&)(total_masked_weights[i]);
		F4 masked_weight_0_to_1 = F4(1.0f) - F4::min(F4(1.0f), total_masked_weight);
		weight = weight + F4(instance.weight / total_weight) * masked_weight_0_to_1;
	}
}

template<typename F4, typename U4>
void UpdateWeightDefaultMasked(Tracks* tracks, Animation::AnimationInstance& instance, const std::array<float, Mesh::MaxNumBones>& total_masked_weights)
{
	auto& weights = instance.animation_sample.weights;

	for (uint32_t i = 0; i < tracks->transform_count; i += 4)
	{
		F4& total_masked_weight = (F4&)(total_masked_weights[i]);
		F4 masked_weight_denom = F4::select(total_masked_weight > F4(1.0f), total_masked_weight, F4(1.0f));
		F4 bone_mask = F4::select((U4&)(instance.animation_sample.bone_masks[i]), F4(1.0f), F4(0.0f));

		F4& weight = (F4&)(weights[i]);
		weight = weight + F4(instance.weight) / masked_weight_denom * bone_mask;
	}
}

/// Blending

template<typename F4>
SIMD_INLINE F4 TracksTime(const F4& previous, const F4& current, const F4& elapsed)
{
	const F4 n = elapsed - previous;
	const F4 d = current - previous;
	const F4 t = F4::select((n < d) & (d > F4::zero()), n / d, F4(1.0f));
	return t;
}

template<typename F4x3, typename F4>
SIMD_INLINE F4x3 TracksInterpolateVector3(const F4x3& old, const F4& previous_key, const F4& current_key, const F4x3& previous_value, const F4x3& current_value, unsigned int i, const F4& elapsed, const F4& weight)
{
	const F4 time = TracksTime<F4>(previous_key, current_key, elapsed);
	const F4x3 target = F4x3::lerp(previous_value, current_value, time);
	return F4x3::lerp(old, target, weight);
}

template<typename F4x3, typename F4>
SIMD_INLINE F4x3 TracksInterpolateVector3Time(const F4& previous_key, const F4& current_key, const F4x3& previous_value, const F4x3& current_value, const F4& elapsed)
{
	const F4 time = TracksTime<F4>(previous_key, current_key, elapsed);
	return F4x3::lerp(previous_value, current_value, time);
}

template<typename F4x4, typename F4>
SIMD_INLINE F4x4 TracksInterpolateQuaternion(const F4x4& old, const F4& previous_key, const F4& current_key, const F4x4& previous_value, const F4x4& current_value, unsigned int i, const F4& elapsed, const F4& weight)
{
	const F4 time = TracksTime<F4>(previous_key, current_key, elapsed);
	const F4x4 target = F4x4::slerp(previous_value, current_value, time);

	return F4x4::slerp(old, target, weight);
}

template<typename F4x4, typename F4>
SIMD_INLINE F4x4 TracksInterpolateQuaternionTime(const F4& previous_key, const F4& current_key, const F4x4& previous_value, const F4x4& current_value, const F4& elapsed)
{
	const F4 time = TracksTime<F4>(previous_key, current_key, elapsed);

	return F4x4::slerp(previous_value, current_value, time);
}

template<typename F4x3, typename F4>
SIMD_INLINE F4x3 TracksInterpolateScale(const AnimationSample& animation_sample, unsigned int i, F4x3& value, const F4& elapsed, const F4& weight)
{
	const F4& SIMD_RESTRICT previous_key = (F4&)animation_sample.previous_major.scale_key[i];
	const F4& SIMD_RESTRICT currrent_key = (F4&)animation_sample.current_major.scale_key[i];
	const F4x3& SIMD_RESTRICT previous_value = (F4x3&)animation_sample.previous_major.scale[i];
	const F4x3& SIMD_RESTRICT current_value = (F4x3&)animation_sample.current_major.scale[i];

	return TracksInterpolateVector3(value, previous_key, currrent_key, previous_value, current_value, i, elapsed, weight);
}

template<typename F4x3, typename F4>
SIMD_INLINE F4x3 TracksInterpolateScaleTime(const AnimationSample& animation_sample, unsigned int i, const F4& elapsed)
{
	const F4& SIMD_RESTRICT previous_key = (F4&)animation_sample.previous_major.scale_key[i];
	const F4& SIMD_RESTRICT currrent_key = (F4&)animation_sample.current_major.scale_key[i];
	const F4x3& SIMD_RESTRICT previous_value = (F4x3&)animation_sample.previous_major.scale[i];
	const F4x3& SIMD_RESTRICT current_value = (F4x3&)animation_sample.current_major.scale[i];

	return TracksInterpolateVector3Time(previous_key, currrent_key, previous_value, current_value, elapsed);
}

template<typename F4x3, typename F4>
SIMD_INLINE F4x3 TracksInterpolateTranslation(const AnimationSample& animation_sample, unsigned int i, F4x3& value, const F4& elapsed, const F4& weight)
{
	const F4& SIMD_RESTRICT previous_key = (F4&)animation_sample.previous_major.translation_key[i];
	const F4& SIMD_RESTRICT currrent_key = (F4&)animation_sample.current_major.translation_key[i];
	const F4x3& SIMD_RESTRICT previous_value = (F4x3&)animation_sample.previous_major.translation[i];
	const F4x3& SIMD_RESTRICT current_value = (F4x3&)animation_sample.current_major.translation[i];

	return TracksInterpolateVector3(value, previous_key, currrent_key, previous_value, current_value, i, elapsed, weight);
}

template<typename F4x3, typename F4>
SIMD_INLINE F4x3 TracksInterpolateTranslationTime(const AnimationSample& animation_sample, unsigned int i, const F4& elapsed)
{
	const F4& SIMD_RESTRICT previous_key = (F4&)animation_sample.previous_major.translation_key[i];
	const F4& SIMD_RESTRICT currrent_key = (F4&)animation_sample.current_major.translation_key[i];
	const F4x3& SIMD_RESTRICT previous_value = (F4x3&)animation_sample.previous_major.translation[i];
	const F4x3& SIMD_RESTRICT current_value = (F4x3&)animation_sample.current_major.translation[i];

	return TracksInterpolateVector3Time(previous_key, currrent_key, previous_value, current_value, elapsed);
}

template<typename F4x4, typename F4>
SIMD_INLINE F4x4 TracksInterpolateRotation(const AnimationSample& animation_sample, unsigned int i, F4x4& value, const F4& elapsed, const F4& weight)
{
	const F4& SIMD_RESTRICT previous_key = (F4&)animation_sample.previous_major.rotation_key[i];
	const F4& SIMD_RESTRICT currrent_key = (F4&)animation_sample.current_major.rotation_key[i];
	const F4x4& SIMD_RESTRICT previous_value = (F4x4&)animation_sample.previous_major.rotation[i];
	const F4x4& SIMD_RESTRICT current_value = (F4x4&)animation_sample.current_major.rotation[i];

	return TracksInterpolateQuaternion(value, previous_key, currrent_key, previous_value, current_value, i, elapsed, weight);
}

template<typename F4x4, typename F4>
SIMD_INLINE F4x4 TracksInterpolateRotationTime(const AnimationSample& animation_sample, unsigned int i, const F4& elapsed)
{
	const F4& SIMD_RESTRICT previous_key = (F4&)animation_sample.previous_major.rotation_key[i];
	const F4& SIMD_RESTRICT currrent_key = (F4&)animation_sample.current_major.rotation_key[i];
	const F4x4& SIMD_RESTRICT previous_value = (F4x4&)animation_sample.previous_major.rotation[i];
	const F4x4& SIMD_RESTRICT current_value = (F4x4&)animation_sample.current_major.rotation[i];

	return TracksInterpolateQuaternionTime(previous_key, currrent_key, previous_value, current_value, elapsed);
}

template <typename F4x4, typename F4x3, typename F4>
std::tuple<F4x3, F4x3, F4x4> InterpolateDefault(const AnimationSample& animation_sample, unsigned int i, const F4& elapsed)
{
	return std::make_tuple(
		TracksInterpolateScaleTime<F4x3, F4>(animation_sample, i, elapsed),
		TracksInterpolateTranslationTime<F4x3, F4>(animation_sample, i, elapsed),
		TracksInterpolateRotationTime<F4x4, F4>(animation_sample, i, elapsed)
	);
}

template <typename F4x4, typename F4x3, typename F4>
std::tuple<F4x3, F4x3, F4x4, F4> BlendDefaultFollowing(const AnimationSample& animation_sample, unsigned int i, F4x3& scale, F4x3& translation, F4x4& rotation, const F4& elapsed, F4 weights)
{
	// When lerping more than 2 animations, lerp coefficients should be properly calculated:
	// lerp(lerp(a1, a2, k1), a3, k2), where k1 and k2 - lerp coefficients and a1, a2, a3 - animations being lerped
	// Let w1, w2 and w3 be the normalized animation weights
	// k1 = w2 / (w1 + w2), k2 = w3 / (w1 + w2 + w3)
	// In case of 4 animations, last lerp coefficient would be k3 = w4 / (w1 + w2 + w3 + w4)
	F4 current_weight = (F4&)animation_sample.weights[i * 4] + F4(1e-4f);
	weights = weights + current_weight; // weights would be equal to w1 at the 1st iteration here, so adding current_weight which is w2
	F4 interpolate_weight = F4::min(current_weight / weights, F4(1.0f)); // w2 / (w1 + w2 + ...)

	return std::make_tuple(
		TracksInterpolateScale<F4x3, F4>(animation_sample, i, scale, elapsed, interpolate_weight),
		TracksInterpolateTranslation<F4x3, F4>(animation_sample, i, translation, elapsed, interpolate_weight),
		TracksInterpolateRotation<F4x4, F4>(animation_sample, i, rotation, elapsed, interpolate_weight),
		weights
	);
}

template <typename F4x4, typename F4x3, typename F4>
std::tuple<F4x3, F4x3, F4x4> BlendAdditive(const AnimationSample& animation_sample, unsigned int i, F4x3& scale, F4x3& translation, F4x4& rotation, const F4& elapsed)
{
	F4 interpolate_weight = (F4&)animation_sample.weights[i * 4];

	const F4x3 additive_scale = TracksInterpolateScaleTime<F4x3, F4>(animation_sample, i, elapsed);
	const F4x3 additive_translation = TracksInterpolateTranslationTime<F4x3, F4>(animation_sample, i, elapsed);
	const F4x4 additive_rotation = TracksInterpolateRotationTime<F4x4, F4>(animation_sample, i, elapsed);

	return std::make_tuple(
		F4x3::lerp(scale, scale * additive_scale, interpolate_weight),
		F4x3::lerp(translation, translation + additive_translation, interpolate_weight),
		F4x4::slerp(rotation, F4x4::mulquat(additive_rotation, rotation), interpolate_weight)
	);
}
