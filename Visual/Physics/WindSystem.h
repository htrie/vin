#pragma once
#include <memory>
#include "Utils/CachedArray.h"

#include "Maths/VectorMaths.h"
#include "DeformableMesh.h"
#include "WindSource.h"

namespace simd { class matrix; }

namespace Physics
{
	class WindSystem
	{
	public:
		WindSourceHandle AddWindSourceByType(Coords3f coords, Vector3f size, float duration, float initial_phase, float primary_velocity, float primary_wavelength, float secondary_velocity, float secondary_wavelength, Vector3f dir, WindSourceTypes::Type windSourceType);

		WindSourceHandle AddExplosionWindSource(Coords3f coords, float radius, float height, float duration, float peak_velocity, float wavelength, float initial_phase);
		WindSourceHandle AddVortexWindSource(Coords3f coords, float radius, float height, float radial_velocity, float angular_velocity, float duration, float turbulence_frequency);
		WindSourceHandle AddWakeWindSource(Coords3f coords);
		WindSourceHandle AddDirectionalWindSource(Coords3f coords, Vector3f size, Vector3f velocity, Vector3f turbulence_amplitude, float turbulence_wavelength);
		WindSourceHandle AddTurbulenceWindSource(Coords3f coords, Vector3f const_velocity, float turbulence_intensity, float turbulence_wavelength);

		void AddFireSource(Vector3f pos, float radius, float intensity);
		void GetFireSources(FireSource *dst_sources, size_t &max_count);
		void ResetFireSources();

		// Asset Viewer only
		void ResetAllSources();

		WindSourceController * GetWindSourceController(size_t wind_source_id, WindSourceTypes::Type wind_source_type);
		void RemoveWindSource(size_t wind_source_id, WindSourceTypes::Type wind_source_type);
		friend class WindSourceHandleInfo;

		void Update(float dt);

		Vector3f GetGlobalWindVelocity(Vector3f world_point) const;

		void RenderWireframe(const simd::matrix & view_projection);
	private:
		LinearCachedArray<ExplosionWindSource> explosion_wind_sources;
		LinearCachedArray<VortexWindSource> vortex_wind_sources;
		LinearCachedArray<WakeWindSource> wake_wind_sources;
		LinearCachedArray<DirectionalWindSource> directional_wind_sources;
		LinearCachedArray<TurbulenceWindSource> turbulence_wind_sources;
		std::vector<FireSource> fire_sources;
	};
}