#pragma once
#include "Maths/VectorMaths.h"
#include "Handles.h"
namespace Physics
{
	struct WindSourceTypes
	{
		enum Type
		{
			Explosion,
			Vortex, 
			Wake,
			Directional,
			Turbulence,
			FireSource,
			NumTypes,
			Unknown = NumTypes
		};
	};
	class WindSystem;

	class WindSourceHandleInfo;
	typedef SimpleUniqueHandle<WindSourceHandleInfo> WindSourceHandle;

	class WindSourceController
	{
	public:

		WindSourceController(Coords3f coords, float duration);
		void SetCoords(Coords3f coords);
		Coords3f GetCoords() const;
		Vector3f GetScale() const;
		AABB3f GetAABB() const;
		void SetAABB(AABB3f aabb);
		float GetDuration() const;
		void SetDuration(float duration);
		void ResetElapsedTime();
		float GetElapsedTime() const;
		bool IsPlaying();
		bool IsAttached();

		void Update(float dt);
		//virtual Vector3f GetWindVelocity(Vector3f worldPoint);

	private:
		Coords3f coords;
		Vector3f scale;
		AABB3f aabb;
		float currTime;
		bool isPlaying;
		float duration;
		bool isAttached;
		friend class WindSourceHandleInfo;
	};


	class WindSourceHandleInfo
	{
	public:
		WindSourceHandleInfo();
		WindSourceHandleInfo(size_t windSourceId, WindSourceTypes::Type windSourceType, WindSystem *wind_system);
		typedef WindSourceController ObjectType;
		WindSourceController *Get() const;
		void Detach();
		void Release();
		bool IsValid() const;
		WindSystem *wind_system;
		size_t windSourceId;
		WindSourceTypes::Type windSourceType;
		friend class WindSystem;
	};


	struct DebugParticle
	{
		Vector3f pos;
		float lifetime;
	};

	struct DebugRing
	{
		float radius;
		float height;
		float angle;
		int color;
	};

	class ExplosionWindSource
	{
	public:
		ExplosionWindSource(Coords3f coords, float radius, float height, float duration, float peakVelocity, float wavelength, float initialPhase);
		Vector3f GetWindVelocity(Vector3f point) const;
		void Update(float dt);
		WindSourceController controller;


		std::vector<DebugRing> debugRings;
		std::vector<DebugParticle> debugParticles;
	private:
		float backRadius;
		float frontRadius;
		float wavelength;
		float initialPhase;
		float radius; //not scaled
		float height; //not scaled
		float peakVelocity;
		float turbulenceFrequency;
		float turbulencePhase;
	};
	class VortexWindSource
	{
	public:
		VortexWindSource(Coords3f coords, float radius, float height, float radialVelocity, float angularVelocity, float duration, float turbulenceAmplitude);
		Vector3f GetWindVelocity(Vector3f point) const;
		void Update(float dt);

		std::vector<DebugRing> debugRings;
		std::vector<DebugParticle> debugParticles;
		WindSourceController controller;
	private:
		Vector3f GetPlanarVelocity(float radius) const;
		float radius;
		float height;
		float radialVelocity;
		float angularVelocity;
		float turbulenceAmplitude;
		float turbulencePhase;
	};
	class WakeWindSource
	{
	public:
		WakeWindSource(Coords3f coords, float vortexSpawnPeriod);
		Vector3f GetWindVelocity(Vector3f point) const;
		void Update(float dt);
		WindSourceController controller;
		struct Vortex
		{
			Vortex()
			{
				this->coords = Coords3f::defCoords();
				this->dstPos = Vector3f::zero();
				this->angularVelocity = 0.0f;
				this->effectiveRadius = 0.0f;
				this->elapsedTime = 0.0f;
			}
			Coords3f coords;
			Vector3f dstPos;
			float angularVelocity;
			float effectiveRadius;
			float elapsedTime;
		};
		const static int vorticesCount = 4;
		Vortex vortices[vorticesCount];

		std::vector<DebugParticle> debugParticles;
	private:
		//float vortexSpawnDistance;
		float vortexSpawnPeriod;
		int currVortexSpawnIndex;
		Coords3f lastSpawnCoords;
		float lastSpawnTime;
		bool isActive;
	};
	class DirectionalWindSource
	{
	public:
		DirectionalWindSource(Coords3f coords, Vector3f size, Vector3f velocity, Vector3f turbulenceAmplitude, float turbulenceWavelength );
		Vector3f GetWindVelocity(Vector3f point) const;
		void Update(float dt);
		Vector3f GetSize();
		WindSourceController controller;
	private:
		Vector3f size;
		Vector3f velocity;
		float time;
		float angle;
		Vector3f turbulenceAmplitude;
		float turbulenceWavelength;
		Vector3f turbulenceVelocity;
	};

	class TurbulenceWindSource
	{
	public:
		TurbulenceWindSource(Coords3f coords, Vector3f planar_velocity, float turbulence_intensity, float turbulence_wavelength);
		Vector3f GetWindVelocity(Vector3f point) const;
		void Update(float dt);
		WindSourceController controller;
	private:
		float time;
		Vector3f const_velocity;
		Vector2f planar_offset;
		float turbulence_intensity;
		float turbulence_wavelength;
	};

	struct FireSource
	{
		Vector3f pos;
		float radius;
		float intensity;
	};
}