#include "WindSource.h"
#include "WindSystem.h"
#include <algorithm>
#include "Visual/Device/Texture.h"
#include "Visual/Device/lodepng/lodepng.h"
#include "Visual/Device/PNG.h"
#include "Common/File/InputFileStream.h"

namespace Physics
{
	
	WindSourceController::WindSourceController(Coords3f coords, float duration)
	{
		SetCoords(coords);
		this->currTime = 0.0f;
		this->duration = duration;
		this->aabb.Reset();
		this->isPlaying = true;
		this->isAttached = true;
	}

	void WindSourceController::SetCoords(Coords3f coords)
	{
		this->coords = coords.ExtractScale(this->scale);
	}

	Coords3f WindSourceController::GetCoords() const
	{
		return this->coords;
	}

	Vector3f WindSourceController::GetScale() const
	{
		return scale;
	}

	AABB3f WindSourceController::GetAABB() const
	{
		return this->aabb;
	}

	void WindSourceController::SetAABB(AABB3f aabb)
	{
		this->aabb = aabb;
	}

	float WindSourceController::GetDuration() const
	{
		return duration;
	}

	void WindSourceController::SetDuration(float duration)
	{
		this->duration = duration;
	}

	void WindSourceController::ResetElapsedTime()
	{
		currTime = 0.0f;
	}

	float WindSourceController::GetElapsedTime() const
	{
		return currTime;
	}

	void WindSourceController::Update(float dt)
	{
		this->currTime += dt;
		if(currTime > duration && duration > 0.0f)
		{
			this->isPlaying = false;
		}
	}

	bool WindSourceController::IsPlaying()
	{
		return this->isPlaying;
	}

	bool WindSourceController::IsAttached()
	{
		return this->isAttached;
	}

	/*Vector3f WindSourceController::GetWindVelocity(Vector3f worldPoint)
	{
		Vector3f localPoint = coords.GetLocalPoint(worldPoint);
		Vector3f normalizedPoint = localPoint & this->size.GetInverse();
		Vector3f normalizedVelocity = windShape->GetNormalizedVelocity(normalizedPoint, currTime / duration);
		Vector3f localVelocity = normalizedVelocity * velocity;
		Vector3f worldVelocity = coords.GetWorldVector(localVelocity);
		return worldVelocity;
	}*/
#undef min
#undef max
	float clamp(float val, float min, float max)
	{
		return std::min(std::max(val, min), max);
	}

	float MiddleWaveSmooth(float val)
	{
		val = clamp(val, 0.0f, 1.0f);
		return val * val * (1.0f - val) * (1.0f - val) / (0.5f * 0.5f * 0.5f * 0.5f);
	}

	float random(float min, float max)
	{
		return min + (max - min) * float(rand()) / float(RAND_MAX);
	}

	ExplosionWindSource::ExplosionWindSource(Coords3f coords, float radius, float height, float duration, float peakVelocity, float wavelength, float initialPhase)
		: controller(WindSourceController(coords, duration))
	{
		this->radius = radius;
		this->height = height;
		this->peakVelocity = peakVelocity;
		this->wavelength = wavelength;
		this->initialPhase = initialPhase;
		this->turbulenceFrequency = 0.0f;
		this->turbulencePhase = 0.0f;
		this->backRadius = 0.0f;
		this->frontRadius = 0.0f;

		#ifdef PHYSICS_VISUALISATIONS
		this->debugRings.resize(5);
		this->debugParticles.resize(50);
		#endif
	}

	Vector3f ExplosionWindSource::GetWindVelocity(Vector3f point) const
	{
		float actualRadius = this->radius * this->controller.GetScale().x;
		float actualHeight = this->height * this->controller.GetScale().z;
		Vector3f centerDelta = point - controller.GetCoords().pos;

		Vector3f centerDir = -Vector3f(centerDelta.x, centerDelta.y, 0.0f).GetNorm();
		Vector3f tangentDir = centerDir ^ Vector3f(0.0f, 0.0f, 1.0f);
		
		float dist = centerDelta.Len();
		
		float ringPhase = clamp((dist - backRadius) / (frontRadius - backRadius), 0.0f, 1.0f);
		float ringMult = MiddleWaveSmooth(ringPhase);
		float radialRatio = clamp(dist / actualRadius, 0.0f, 1.0f);
		float radialMult = 1.0f - radialRatio * radialRatio;

		Vector3f radialVelocity = -centerDir * ringMult * this->peakVelocity * radialMult;
		//Vector3f turbulenceVelocity = Vector3f(0.0f, 0.0f, 1.0f) * ringMult * this->turbulenceFrequency

		/*float angularVelocity = clamp((1.0f / (dist + offset) - 1.0f / (1.0f + offset)) / (1.0f / offset - 1.0f / (1.0f + offset)), 0.0f, 1.0f);
		float tangentNoise = sinf(dist * 2.0f * 0.0f + normalizedTime * angularVelocity * 200.0f);
		float verticalNoise = sinf(dist * 7.0f * 0.0f + normalizedTime * angularVelocity * 270.0f) + clamp(1.0f - dist, 0.0f, 1.0f) * 0.5f;
		return Vector3f(0.0f, 0.0f, 1.0f) * verticalNoise * 0.4f + tangentDir * (angularVelocity * (0.7f + 0.3f * tangentNoise)) + centerDir * (1.0f - clamp(dist, 0.01f, 1.0f)) * 0.05f;*/
		return radialVelocity;
	}


	void ExplosionWindSource::Update(float dt)
	{
		this->controller.Update(dt);
		Coords3f coords = this->controller.GetCoords();

		float actualRadius = this->radius * this->controller.GetScale().x;
		float actualHeight = this->height * this->controller.GetScale().z;

		Vector3f delta = Vector3f(
			std::abs(coords.xVector.x) * actualRadius + std::abs(coords.yVector.x) * actualRadius + std::abs(coords.zVector.x) * actualHeight,
			std::abs(coords.xVector.y) * actualRadius + std::abs(coords.yVector.y) * actualRadius + std::abs(coords.zVector.y) * actualHeight,
			std::abs(coords.xVector.z) * actualRadius + std::abs(coords.yVector.z) * actualRadius + std::abs(coords.zVector.z) * actualHeight);
		this->controller.SetAABB(AABB3f(coords.pos - delta, coords.pos + delta));

		float progress = controller.GetDuration() > 0.0f ? controller.GetElapsedTime() / controller.GetDuration() : 0.5f;

		float endPhase = 1.0f + wavelength;

		this->frontRadius = (this->initialPhase + (endPhase - this->initialPhase) * progress) * actualRadius;
		this->backRadius = frontRadius - actualRadius * wavelength;

		if(debugRings.size() > 0)
		{
			for(size_t ringIndex = 0; ringIndex < debugRings.size() - 1; ringIndex++)
			{
				float ratio = float(ringIndex) / (float(debugRings.size() - 2));
				debugRings[ringIndex].radius = std::min(actualRadius, std::max(0.0f, backRadius + ratio * (frontRadius - backRadius)));
				debugRings[ringIndex].height = actualHeight;
				debugRings[ringIndex].angle = 0.0f;
				debugRings[ringIndex].color = 0xff448844;
			}
			debugRings.back().radius = actualRadius;
			debugRings.back().height = actualHeight;
			debugRings.back().angle = 0.0f;
			debugRings.back().color = 0xff884444;
		}

		for(size_t particleIndex = 0; particleIndex < debugParticles.size(); particleIndex++)
		{
			debugParticles[particleIndex].lifetime -= dt;
			AABB3f aabb = controller.GetAABB();
			bool isAlive = aabb.Includes(debugParticles[particleIndex].pos);
			if(debugParticles[particleIndex].lifetime < 0.0f)
			{
				isAlive = false;
			}
			if(!isAlive)
			{
				Vector3f worldPos = Vector3f(random(aabb.box_point1.x, aabb.box_point2.x), random(aabb.box_point1.y, aabb.box_point2.y), random(aabb.box_point1.z, aabb.box_point2.z));
				debugParticles[particleIndex].pos = worldPos;
				debugParticles[particleIndex].lifetime = random(0.5f, 1.0f);
			}
		}
		for(size_t particleIndex = 0; particleIndex < debugParticles.size(); particleIndex++)
		{
			debugParticles[particleIndex].pos += GetWindVelocity(debugParticles[particleIndex].pos) * dt;
			Vector3f particlePos = debugParticles[particleIndex].pos;
		}
	}

	VortexWindSource::VortexWindSource(Coords3f coords, float radius, float height, float radialVelocity, float angularVelocity, float duration, float turbulenceAmplitude)
		: controller(WindSourceController(coords, duration))
	{
		this->radius = radius;
		this->height = height;
		this->radialVelocity = radialVelocity;
		this->angularVelocity = angularVelocity;
		this->turbulenceAmplitude = turbulenceAmplitude;
		this->turbulencePhase = 0.0f;

		float actualRadius = this->radius * this->controller.GetScale().x;

		#ifdef PHYSICS_VISUALISATIONS
		this->debugRings.resize(5);
		for(size_t ringIndex = 0; ringIndex < debugRings.size(); ringIndex++)
		{
			debugRings[ringIndex].angle = 0.0f;
			debugRings[ringIndex].radius = actualRadius * float(ringIndex) / (float(debugRings.size()) - 1.0f + 1e-2f);
			debugRings[ringIndex].height = 0.0f;
			debugRings[ringIndex].color = 0;
		}
		#endif
	}

	Vector3f VortexWindSource::GetWindVelocity(Vector3f point) const
	{
		float actualRadius = this->radius * this->controller.GetScale().x;
		float actualHeight = this->height * this->controller.GetScale().z;

		Vector3f centerDelta = point - controller.GetCoords().pos;

		Vector3f relCenterDelta = this->controller.GetCoords().GetLocalPoint(centerDelta);
		relCenterDelta.z = 0.0f;

		Vector3f planarCenterDelta =  this->controller.GetCoords().GetWorldPoint(relCenterDelta);

		Vector3f centerDir = -planarCenterDelta.GetNorm();
		Vector3f upDir = this->controller.GetCoords().zVector;
		Vector3f tangentDir = upDir ^ centerDir;
		
		float dist = centerDelta.Len();

		Vector3f planarVelocity = GetPlanarVelocity(dist);
		return 
			planarVelocity.x * dist * tangentDir + 
			planarVelocity.y * centerDir +
			planarVelocity.z * upDir;
	}

	void VortexWindSource::Update(float dt)
	{
		this->controller.Update(dt);

		float actualRadius = this->radius * this->controller.GetScale().x;
		float actualHeight = this->height * this->controller.GetScale().z;


		if(debugRings.size() > 0)
		{
			for(size_t ringIndex = 0; ringIndex < debugRings.size() - 1; ringIndex++)
			{
				Vector3f planarVelocity = GetPlanarVelocity(debugRings[ringIndex].radius);
				debugRings[ringIndex].radius += planarVelocity.y * dt;
				float threshold = 0.1f;
				debugRings[ringIndex].radius = std::fmod(debugRings[ringIndex].radius - actualRadius *threshold, actualRadius * (1.0f - threshold * 2.0f));
				if(debugRings[ringIndex].radius < 0.0f)
				{
					debugRings[ringIndex].radius += actualRadius * (1.0f - threshold * 2.0f);
				}
				debugRings[ringIndex].radius += actualRadius * threshold;

				debugRings[ringIndex].angle += planarVelocity.x * dt;
				debugRings[ringIndex].height = actualHeight;
				debugRings[ringIndex].color = 0xff448844;
			}

			debugRings.back().radius = actualRadius;
			debugRings.back().height = actualHeight;
			debugRings.back().angle = 0.0f;
			debugRings.back().color = 0xff884444;
		}

		this->turbulencePhase += dt;

		Coords3f coords = this->controller.GetCoords();
		Vector3f delta = Vector3f(
			std::abs(coords.xVector.x) * actualRadius + std::abs(coords.yVector.x) * actualRadius + std::abs(coords.zVector.x) * actualHeight,
			std::abs(coords.xVector.y) * actualRadius + std::abs(coords.yVector.y) * actualRadius + std::abs(coords.zVector.y) * actualHeight,
			std::abs(coords.xVector.z) * actualRadius + std::abs(coords.yVector.z) * actualRadius + std::abs(coords.zVector.z) * actualHeight);
		this->controller.SetAABB(AABB3f(coords.pos - delta, coords.pos + delta));
	}

	Vector3f VortexWindSource::GetPlanarVelocity(float radius) const
	{
		float actualRadius = this->radius * this->controller.GetScale().x;
		float ratio = clamp(radius / actualRadius, 0.0f, 1.0f);
		
		float angularVelocity = this->angularVelocity * (1.0f / (ratio + 1e-1f) - 1.0f / (1.0f + 1e-1f));
		float radialVelocity = this->radialVelocity * (1.0f - ratio * ratio * ratio * ratio);
		float upwardsVelocity = 0.0f;

		float spatialPhase =  (radius / 100.0f) * 10.0f;
		upwardsVelocity += sinf(this->turbulencePhase * 23.0f + spatialPhase) * this->turbulenceAmplitude * (fabs(radialVelocity) + fabs(angularVelocity) * radius) * 2.0f;
		radialVelocity += sinf(this->turbulencePhase * 35.0f + spatialPhase) * this->turbulenceAmplitude * radialVelocity;
		angularVelocity += sinf(this->turbulencePhase * 42.0f + spatialPhase) * this->turbulenceAmplitude *angularVelocity;

		return Vector3f(angularVelocity, radialVelocity, upwardsVelocity);
	}

	WakeWindSource::WakeWindSource(Coords3f coords, float vortexSpawnPeriod)
		: controller(WindSourceController(coords, -1.0f))
	{
		//this->vortexSpawnDistance = 0.0f;
		this->vortexSpawnPeriod = vortexSpawnPeriod;
		this->currVortexSpawnIndex = 0;
		this->lastSpawnCoords = coords;
		this->lastSpawnTime = 0.0f;
		this->isActive = true;
		#ifdef PHYSICS_VISUALISATIONS
		{
			this->debugParticles.resize(100);
		}
		#endif
	}

	Vector3f WakeWindSource::GetWindVelocity(Vector3f point) const
	{
		/*if(!isActive)
			return Vector3f::zero();*/
		Vector3f resultVelocity = Vector3f(0.0f, 0.0f, 0.0f);
		for(size_t vortexIndex = 0; vortexIndex < vorticesCount; vortexIndex++)
		{
			Vector3f delta = point - vortices[vortexIndex].coords.pos;
			Vector3f angularVelocity = vortices[vortexIndex].coords.zVector * vortices[vortexIndex].angularVelocity;
			
			const float radius = vortices[vortexIndex].effectiveRadius;
			if (radius > 0.0f)
			{
				float radialRatio = (delta.Len() / radius);
				//float radialFaloff = std::max(0.0f, 1.0f - radialRatio * radialRatio);
				float radialFaloff = std::max(0.0f, 1.0f - radialRatio);
				//radialFaloff *= radialFaloff;// * radialFaloff;
				//float radialFaloff = 1.0 - radialRatio;
				Vector3f vortexLinearVelocity = (delta ^ angularVelocity) * radialFaloff;

				resultVelocity += vortexLinearVelocity;
			}
		}
		return resultVelocity;
	}



	void WakeWindSource::Update(float dt)
	{
		/*float vortexLinearSpeedMult = 2.5f;
		float vortexAngularDamping = 0.5f;
		float angularVelocityBuildup = 1.5;
		vortexSpawnPeriod = 200.0f;
		float vortexRadius = 250.5f;
		float vortexStreetWidth = 250.0f; //250cm vortex width
		float vortexAngularVelocityMult = 2.0f * 1.0f / (0.5f * vortexStreetWidth);*/

		float vortexLinearSpeedMult = 2.5f;
		float vortexAngularDamping = 0.8f;
		float angularVelocityBuildup = 1.5;
		vortexSpawnPeriod = 100.0f;
		float vortexRadius = 250.5f;
		float vortexStreetWidth = 250.0f; //250cm vortex width
		float vortexAngularVelocityMult = 0.9f / (0.5f * vortexStreetWidth);
		float decayTimeThreshold = 5.0f; //after 5 seconds without spawning vortices it disables itself

		this->controller.Update(dt);

		Coords3f currCoords = this->controller.GetCoords();
		float currTime = controller.GetElapsedTime();

		Vector3f delta = Vector3f(1000.0f, 1000.0f, 1000.0f);
		this->controller.SetAABB(AABB3f(currCoords.pos - delta, currCoords.pos + delta));

		Vector3f displacement = currCoords.pos - lastSpawnCoords.pos;
		if(displacement.Len() > vortexSpawnPeriod)
		{
			/*float vortexRadius = 250.5f;
			float vortexStreetWidth = 100.0f; //100cm vortex width*/
			Vector3f avgVelocity = (currCoords.pos - lastSpawnCoords.pos) / std::max(currTime - lastSpawnTime, 0.1f);
			float maxAvgVelocity = 1e4f; //100m/s
			if(avgVelocity.Len() > maxAvgVelocity)
				avgVelocity = avgVelocity.GetNorm() * maxAvgVelocity;


			Vector3f dir = displacement.GetNorm();
			Vector3f tangent = (currCoords.pos - lastSpawnCoords.pos) ^ Vector3f(0.0f, 0.0f, 1.0f);
			if(tangent.SquareLen() > 1e-7f)
				tangent.Normalize();

			lastSpawnCoords = currCoords;
			lastSpawnTime = currTime;

			float evenness = (currVortexSpawnIndex % 2 == 0 ? 1.0f : -1.0f);
			vortices[currVortexSpawnIndex].coords = Coords3f::defCoords();
			vortices[currVortexSpawnIndex].coords.pos = currCoords.pos;
			vortices[currVortexSpawnIndex].angularVelocity = avgVelocity.Len() * vortexAngularVelocityMult * evenness;
			vortices[currVortexSpawnIndex].effectiveRadius = vortexRadius;
			vortices[currVortexSpawnIndex].dstPos = currCoords.pos + tangent * evenness * vortexStreetWidth * 0.5f;
			vortices[currVortexSpawnIndex].elapsedTime = 0.0f;
			currVortexSpawnIndex = (currVortexSpawnIndex + 1) % vorticesCount;
		}

		this->isActive = (currTime - lastSpawnTime) < decayTimeThreshold;

		for(size_t vortexIndex = 0; vortexIndex < vorticesCount; vortexIndex++)
		{
			vortices[vortexIndex].elapsedTime += dt;
			vortices[vortexIndex].angularVelocity *= exp(-dt * vortexAngularDamping);
			float angularVelocity = (1.0f - exp(-vortices[vortexIndex].elapsedTime * angularVelocityBuildup)) * vortices[vortexIndex].angularVelocity;
			vortices[vortexIndex].coords.Rotate(vortices[vortexIndex].coords.pos, vortices[vortexIndex].coords.zVector, angularVelocity * dt);
			vortices[vortexIndex].coords.pos = vortices[vortexIndex].dstPos + (vortices[vortexIndex].coords.pos - vortices[vortexIndex].dstPos) * exp(-dt * vortexLinearSpeedMult);
		}

		for(size_t particleIndex = 0; particleIndex < debugParticles.size(); particleIndex++)
		{
			debugParticles[particleIndex].lifetime -= dt;
			bool isAlive = false;
			for(size_t vortexIndex = 0; vortexIndex < vorticesCount; vortexIndex++)
			{
				if((debugParticles[particleIndex].pos - vortices[vortexIndex].coords.pos).SquareLen() < sqr(vortices[vortexIndex].effectiveRadius))
				{
					isAlive = true;
				}
			}
			if(debugParticles[particleIndex].lifetime < 0.0f)
			{
				isAlive = false;
			}
			if(!isAlive)
			{
				int vortexIndex = std::rand() % vorticesCount;
				float radius = vortices[vortexIndex].effectiveRadius;
				Vector3f relPos = Vector3f(random(-radius, radius), random(-radius, radius), random(-radius, radius));
				debugParticles[particleIndex].pos = relPos + vortices[vortexIndex].coords.pos;
				debugParticles[particleIndex].lifetime = 2.0f; //particles live 3 seconds
			}
		}
		for(size_t particleIndex = 0; particleIndex < debugParticles.size(); particleIndex++)
		{
			debugParticles[particleIndex].pos += GetWindVelocity(debugParticles[particleIndex].pos) * dt;
			Vector3f particlePos = debugParticles[particleIndex].pos;
		}
	}

	


	DirectionalWindSource::DirectionalWindSource(Coords3f coords, Vector3f size, Vector3f velocity, Vector3f turbulenceAmplitude, float turbulenceWavelength)
		: controller(WindSourceController(coords, -1.0f))
	{
		this->size = size;
		this->velocity = velocity;
		this->time = 0.0f;
		this->turbulenceAmplitude = turbulenceAmplitude;
		this->turbulenceWavelength = turbulenceWavelength;
	}
	Vector3f DirectionalWindSource::GetWindVelocity(Vector3f point) const
	{
		return this->controller.GetCoords().GetWorldVector(velocity + turbulenceVelocity);
	}
	void DirectionalWindSource::Update(float dt)
	{
		this->time += dt;
		this->controller.Update(dt);
		float freq = this->turbulenceAmplitude.Len() / (turbulenceWavelength + 1e-3f);
		this->turbulenceVelocity = Vector3f::zero();
		Vector3f tangentVelocity = turbulenceAmplitude ^ Vector3f(0.0f, 0.0f, 1.0f);
		struct Wavelet
		{
			float freqMult;
			float cutoff;
			Vector3f amplitude;
		};
		Wavelet wavelets[2];
		wavelets[0].freqMult = 1.0f;
		wavelets[0].cutoff = 0.4f;
		wavelets[0].amplitude = turbulenceAmplitude * 0.2f + tangentVelocity * 0.5f;

		wavelets[1].freqMult = 1.7f;
		wavelets[1].cutoff = 0.7f;
		wavelets[1].amplitude = turbulenceAmplitude * 0.8f - tangentVelocity * 0.2f;

		for(int waveletIndex = 0; waveletIndex < sizeof(wavelets) / sizeof(wavelets[0]); waveletIndex++)
		{
			float mult = std::max(0.0f, sin(wavelets[waveletIndex].freqMult * freq * time) * 0.5f + 0.5f - wavelets[waveletIndex].cutoff) / (1.0f - wavelets[waveletIndex].cutoff);
			this->turbulenceVelocity += wavelets[waveletIndex].amplitude * mult;
		}
	}

	Vector3f DirectionalWindSource::GetSize()
	{
		return this->size;
	}

	float Uint8ToSFloat(uint8_t val)
	{
		return (float(val) / 255.0f - 0.5f) * 2.0f;
	}
	struct TurbulenceTextureData
	{
		TurbulenceTextureData()
		{
			File::InputFileStream stream(L"Art/2DArt/Lookup/png/turbulence.png");
			assert(stream.GetFileSize() > 0);
			PNG png(std::string("Wind texture"), (uint8_t*)stream.GetFilePointer(), stream.GetFileSize());
			size = Vector2<int>(png.GetWidth(), png.GetHeight());
			pixels.resize(size.x * size.y);
			size_t stride = 4;
			for (size_t y = 0; y < png.GetHeight(); y++)
			{
				for (size_t x = 0; x < png.GetWidth(); x++)
				{
					pixels[x + size.x * y] = Vector2f(
						Uint8ToSFloat(png.GetImageData()[(x + y * size.x) * stride + 0]),
						Uint8ToSFloat(png.GetImageData()[(x + y * size.x) * stride + 1])
					);
				}
			}
		}
		static int WrapCoord(int coord, int size)
		{
			return (size + (coord % size)) % size; //to handle negatives
		}
		static Vector2<int> WrapCoord2i(Vector2<int> coord, Vector2<int> size)
		{
			return { WrapCoord(coord.x, size.x), WrapCoord(coord.y, size.y) };
		}
		Vector2f Interpolate(Vector2f uv)
		{
			Vector2f pixel_float = { uv.x * float(size.x), uv.y * float(size.y) };
			Vector2f pixel_lo_float = { floor(pixel_float.x), floor(pixel_float.y) };
			Vector2f ratio = pixel_float - pixel_lo_float;
			Vector2<int> pixel_lo = WrapCoord2i(Vector2<int>((int)pixel_lo_float.x, (int)pixel_lo_float.y), size);
			Vector2<int> pixel_hi = WrapCoord2i(Vector2<int>((int)pixel_lo_float.x, (int)pixel_lo_float.y) + Vector2<int>(1, 1), size);
			return
				Get({ pixel_lo.x, pixel_lo.y }) * (1.0f - ratio.x) * (1.0f - ratio.y) +
				Get({ pixel_hi.x, pixel_lo.y }) * (0.0f + ratio.x) * (1.0f - ratio.y) +
				Get({ pixel_hi.x, pixel_hi.y }) * (0.0f + ratio.x) * (0.0f + ratio.y) +
				Get({ pixel_lo.x, pixel_hi.y }) * (1.0f - ratio.x) * (0.0f + ratio.y);
		}
		Vector2f Get(Vector2<int> coord)
		{
			return pixels[coord.x + coord.y * size.x];
		}
		Memory::Vector<Vector2f, Memory::Tag::Environment> pixels;
		Vector2<int> size;
	};
	std::unique_ptr<TurbulenceTextureData> turbulence_tex_data;

	TurbulenceWindSource::TurbulenceWindSource(Coords3f coords, Vector3f const_velocity, float turbulence_intensity, float turbulence_wavelength) :
		controller(WindSourceController(coords, -1.0f)) ,
		const_velocity(const_velocity),
		turbulence_intensity(turbulence_intensity),
		turbulence_wavelength(turbulence_wavelength),
		time(0.0f),
		planar_offset({0.0f, 0.0f})
	{
		if (!Physics::turbulence_tex_data) Physics::turbulence_tex_data = std::make_unique<TurbulenceTextureData>();
	}
	Vector3f TurbulenceWindSource::GetWindVelocity(Vector3f point) const
	{
		Vector2f uv = (Vector2f(point.x, point.y) - planar_offset) / turbulence_wavelength; //10m period
		Vector2f planar_wind = turbulence_tex_data->Interpolate(uv) * turbulence_intensity;
		return Vector3f(planar_wind.x, planar_wind.y, 0.0f) + const_velocity;
		//return this->controller.GetCoords().GetWorldVector(velocity + turbulenceVelocity);
	}
	void TurbulenceWindSource::Update(float dt)
	{
		this->time += dt;
		this->controller.Update(dt);
		this->planar_offset = Vector2f(const_velocity.x, const_velocity.y) * time;
	}


	WindSourceHandleInfo::WindSourceHandleInfo()
	{
		this->wind_system = nullptr;
		this->windSourceId = size_t(-1);
		this->windSourceType = WindSourceTypes::Explosion;
	}

	WindSourceHandleInfo::WindSourceHandleInfo(size_t windSourceId, WindSourceTypes::Type windSourceType, WindSystem *wind_system)
	{
		this->wind_system = wind_system;
		this->windSourceId = windSourceId;
		this->windSourceType = windSourceType;
	}
	void WindSourceHandleInfo::Detach()
	{
		if(this->windSourceId == size_t(-1))
			return;
		Get()->isAttached = false;
		this->windSourceId = size_t(-1);
	}
	void WindSourceHandleInfo::Release()
	{
		this->wind_system->RemoveWindSource(this->windSourceId, this->windSourceType);
	}
	bool WindSourceHandleInfo::IsValid() const
	{
		//assert(!isWeakHandle);
		return this->wind_system && this->windSourceId != size_t(-1);
	}
	WindSourceController * WindSourceHandleInfo::Get() const
	{
		return wind_system->GetWindSourceController(this->windSourceId, this->windSourceType);
	}
}