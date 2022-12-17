#include "Visual/Device/Line.h"

#include "WindSystem.h"

namespace Physics
{
	simd::vector3 MakeDXVector(Vector3f vector);
	Vector3f MakeVector3f(simd::vector3 vector);
	simd::matrix MakeDXMatrix(Coords3f coords);
	Coords3f MakeCoords3f(const simd::matrix& mat);

	std::mutex wind_add_mutex;
	WindSourceHandle WindSystem::AddWindSourceByType(Coords3f coords, Vector3f size, float duration, float initial_phase, float primary_velocity, float primary_wavelength, float secondary_velocity, float secondary_wavelength, Vector3f dir, WindSourceTypes::Type wind_source_type)
	{
		switch(wind_source_type)
		{
			case WindSourceTypes::Explosion:
			{
				return AddExplosionWindSource(coords, size.x, size.z, duration, primary_velocity, primary_wavelength, initial_phase);
			}break;
			case WindSourceTypes::Vortex:
			{
				return AddVortexWindSource(coords, size.x, size.z, primary_velocity, secondary_velocity, duration, secondary_wavelength);
			}break;
			case WindSourceTypes::Wake:
			{
				return AddWakeWindSource(coords);
			}break;			
			case WindSourceTypes::Directional:
			{
				return AddDirectionalWindSource(coords, size, dir * primary_velocity, dir * secondary_velocity, secondary_wavelength);
			}break;
			case WindSourceTypes::Turbulence:
			{
				return AddTurbulenceWindSource(coords, dir * primary_velocity, secondary_velocity, secondary_wavelength);
			}break;
			case WindSourceTypes::FireSource:
			{
				assert(0); //fire source has no wind handle and is added in a different way
			}break;
		}
		assert(!"Unknown wind type");
		return WindSourceHandle();
	}

	WindSourceHandle WindSystem::AddExplosionWindSource(Coords3f coords, float radius, float height, float duration, float peak_velocity, float wavelength, float initial_phase)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		return WindSourceHandle(WindSourceHandleInfo(explosion_wind_sources.Add( ExplosionWindSource( coords, radius, height, duration, peak_velocity, wavelength, initial_phase ) ), WindSourceTypes::Explosion, this), false);
	}
	WindSourceHandle WindSystem::AddVortexWindSource(Coords3f coords, float radius, float height, float radial_velocity, float angular_velocity, float duration, float turbulence_frequency)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		return WindSourceHandle(WindSourceHandleInfo(vortex_wind_sources.Add( VortexWindSource( coords, radius, height, radial_velocity, angular_velocity, duration, turbulence_frequency ) ), WindSourceTypes::Vortex, this), false);
	}
	WindSourceHandle WindSystem::AddWakeWindSource(Coords3f coords)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		return WindSourceHandle(WindSourceHandleInfo(wake_wind_sources.Add( WakeWindSource( coords, 200.0f ) ), WindSourceTypes::Wake, this), false);
	}
	WindSourceHandle WindSystem::AddDirectionalWindSource(Coords3f coords, Vector3f size, Vector3f velocity, Vector3f turbulence_amplitude, float turbulence_wavelength)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		return WindSourceHandle(WindSourceHandleInfo(directional_wind_sources.Add(DirectionalWindSource(coords, size, velocity, turbulence_amplitude, turbulence_wavelength)), WindSourceTypes::Directional, this), false);
	}
	WindSourceHandle WindSystem::AddTurbulenceWindSource(Coords3f coords, Vector3f const_velocity, float turbulence_intensity, float turbulence_wavelength)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		return WindSourceHandle(WindSourceHandleInfo(turbulence_wind_sources.Add(TurbulenceWindSource(coords, const_velocity, turbulence_intensity, turbulence_wavelength)), WindSourceTypes::Turbulence, this), false);
	}

	void WindSystem::AddFireSource(Vector3f pos, float radius, float intensity)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		FireSource newbie;
		newbie.pos = pos;
		newbie.radius = radius;
		newbie.intensity = intensity;
		fire_sources.push_back(newbie);
	}

	void WindSystem::GetFireSources(FireSource *dst_sources, size_t &max_count)
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		max_count = std::min(fire_sources.size(), max_count);
		if (max_count > 0)
			std::copy(fire_sources.begin(), fire_sources.begin() + max_count, dst_sources);
	}

	void WindSystem::ResetFireSources()
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);
		fire_sources.clear();
	}

	void WindSystem::ResetAllSources()
	{
		std::lock_guard<std::mutex> lock(wind_add_mutex);

		fire_sources.clear();
		for (size_t i = 0; i < explosion_wind_sources.GetElementsCount(); ++i)
			explosion_wind_sources.RemoveByIndex(i);
		for (size_t i = 0; i < vortex_wind_sources.GetElementsCount(); ++i)
			vortex_wind_sources.RemoveByIndex(i);
		for (size_t i = 0; i < wake_wind_sources.GetElementsCount(); ++i)
			wake_wind_sources.RemoveByIndex(i);
		for (size_t i = 0; i < directional_wind_sources.GetElementsCount(); ++i)
			directional_wind_sources.RemoveByIndex(i);
		for (size_t i = 0; i < turbulence_wind_sources.GetElementsCount(); ++i)
			turbulence_wind_sources.RemoveByIndex(i);
	}

	WindSourceController * WindSystem::GetWindSourceController(size_t wind_source_id, WindSourceTypes::Type wind_source_type)
	{
		switch(wind_source_type)
		{
			case WindSourceTypes::Explosion:
			{
				return &(explosion_wind_sources.GetById(wind_source_id).controller);
			}break;
			case WindSourceTypes::Vortex:
			{
				return &(vortex_wind_sources.GetById(wind_source_id).controller);
			}break;
			case WindSourceTypes::Wake:
			{
				return &(wake_wind_sources.GetById(wind_source_id).controller);
			}break;
			case WindSourceTypes::Directional:
			{
				return &(directional_wind_sources.GetById(wind_source_id).controller);
			}break;
			case WindSourceTypes::Turbulence:
			{
				return &(turbulence_wind_sources.GetById(wind_source_id).controller);
			}break;
		}
		return nullptr;
	}
	void WindSystem::RemoveWindSource(size_t wind_source_id, WindSourceTypes::Type wind_source_type)
	{
		switch(wind_source_type)
		{
			case WindSourceTypes::Explosion:
			{
				explosion_wind_sources.RemoveById(wind_source_id);
			}break;
			case WindSourceTypes::Vortex:
			{
				vortex_wind_sources.RemoveById(wind_source_id);
			}break;
			case WindSourceTypes::Wake:
			{
				wake_wind_sources.RemoveById(wind_source_id);
			}break;
			case WindSourceTypes::Directional:
			{
				directional_wind_sources.RemoveById(wind_source_id);
			}break;
			case WindSourceTypes::Turbulence:
			{
				turbulence_wind_sources.RemoveById(wind_source_id);
			}break;
		}
	}
	void WindSystem::Update(float timeStep)
	{
		for(size_t wind_source_index = 0; wind_source_index < explosion_wind_sources.GetElementsCount(); wind_source_index++)
		{
			if(!explosion_wind_sources.GetByIndex(wind_source_index).controller.IsPlaying() && !explosion_wind_sources.GetByIndex(wind_source_index).controller.IsAttached())
			{
				explosion_wind_sources.RemoveByIndex(wind_source_index);
			}else
			{
				explosion_wind_sources.GetByIndex(wind_source_index).Update(timeStep);
			}
		}
		for(size_t wind_source_index = 0; wind_source_index < vortex_wind_sources.GetElementsCount(); wind_source_index++)
		{
			if(!vortex_wind_sources.GetByIndex(wind_source_index).controller.IsPlaying() && !vortex_wind_sources.GetByIndex(wind_source_index).controller.IsAttached())
			{
				vortex_wind_sources.RemoveByIndex(wind_source_index);
			}else
			{
				vortex_wind_sources.GetByIndex(wind_source_index).Update(timeStep);
			}
		}
		for(size_t wind_source_index = 0; wind_source_index < wake_wind_sources.GetElementsCount(); wind_source_index++)
		{
			if(!wake_wind_sources.GetByIndex(wind_source_index).controller.IsPlaying() && !wake_wind_sources.GetByIndex(wind_source_index).controller.IsAttached())
			{
				wake_wind_sources.RemoveByIndex(wind_source_index);
			}else
			{
				wake_wind_sources.GetByIndex(wind_source_index).Update(timeStep);
			}
		}
		for(size_t wind_source_index = 0; wind_source_index < directional_wind_sources.GetElementsCount(); wind_source_index++)
		{
			if(!directional_wind_sources.GetByIndex(wind_source_index).controller.IsPlaying() && !directional_wind_sources.GetByIndex(wind_source_index).controller.IsAttached())
			{
				directional_wind_sources.RemoveByIndex(wind_source_index);
			}else
			{
				directional_wind_sources.GetByIndex(wind_source_index).Update(timeStep);
			}
		}
		for (size_t wind_source_index = 0; wind_source_index < turbulence_wind_sources.GetElementsCount(); wind_source_index++)
		{
			if (!turbulence_wind_sources.GetByIndex(wind_source_index).controller.IsPlaying() && !turbulence_wind_sources.GetByIndex(wind_source_index).controller.IsAttached())
			{
				turbulence_wind_sources.RemoveByIndex(wind_source_index);
			}
			else
			{
				turbulence_wind_sources.GetByIndex(wind_source_index).Update(timeStep);
			}
		}
		explosion_wind_sources.Update();
		vortex_wind_sources.Update();
		wake_wind_sources.Update();
		directional_wind_sources.Update();
		turbulence_wind_sources.Update();
	}


	void RenderWindController(Device::Line & line, const simd::matrix & final_transform, Coords3f coords, AABB3f aabb, float duration, float elapsedTime)
	{
		Vector3f pos = coords.pos;
		coords = Coords3f::defCoords();
		coords.pos = pos;
		Vector3f quad_vertices[6];

		Vector3f size = Vector3f(50.0f, 50.0f, 20.0f);
		quad_vertices[0] = coords.pos + coords.xVector * size.x;
		quad_vertices[1] = coords.pos + coords.yVector * size.y;
		quad_vertices[2] = coords.pos - coords.xVector * size.x;
		quad_vertices[3] = coords.pos - coords.yVector * size.y;
		quad_vertices[4] = coords.pos + coords.zVector * size.z;
		quad_vertices[5] = coords.pos - coords.zVector * size.z;

		for(int phase = 0; phase < 2; phase++)
		{
			simd::color color;
			float scale;
			if(phase == 0)
			{
				color = duration > 0.0f ? 0xff88ff88 : 0xffff8888;
				scale = 1.0f;
			}else
			{
				if(duration <= 0.0f)
					continue;
				color = 0xff555555;
				scale = 1.0f - elapsedTime / duration;
			}

			simd::vector3 line_verts[2];

			for(int i = 0; i < 4; i++)
			{
				//side
				line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[i] - coords.pos) * scale);
				line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[(i + 1) % 4] - coords.pos)* scale);
				line.DrawTransform(&line_verts[0], 2, &final_transform, color);

				//top
				line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[i] - coords.pos) * scale);
				line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[4] - coords.pos) * scale);
				line.DrawTransform(&line_verts[0], 2, &final_transform, color);

				//bottom
				line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[i] - coords.pos) * scale);
				line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[5] - coords.pos) * scale);
				line.DrawTransform(&line_verts[0], 2, &final_transform, color);
			}
		}
	}

	void RenderWakeVortex(Device::Line & line, const simd::matrix & final_transform, Coords3f coords)
	{
		Vector3f quad_vertices[6];

		Vector3f size = Vector3f(50.0f, 50.0f, 20.0f);
		quad_vertices[0] = coords.pos + coords.xVector * size.x;
		quad_vertices[1] = coords.pos + coords.yVector * size.y;
		quad_vertices[2] = coords.pos - coords.xVector * size.x;
		quad_vertices[3] = coords.pos - coords.yVector * size.y;
		quad_vertices[4] = coords.pos + coords.zVector * size.z;
		quad_vertices[5] = coords.pos - coords.zVector * size.z;

		simd::color color;
		float scale = 1.0f;
		color = 0x8888ffff;

		simd::vector3 line_verts[2];

		for(int i = 0; i < 4; i++)
		{
			//side
			line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[i] - coords.pos) * scale);
			line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[(i + 1) % 4] - coords.pos)* scale);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color.c());

			//top
			line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[i] - coords.pos) * scale);
			line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[4] - coords.pos) * scale);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color.c());

			//bottom
			line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[i] - coords.pos) * scale);
			line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[5] - coords.pos) * scale);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color.c());
		}
	}

	void RenderDebugParticle(Device::Line & line, const simd::matrix & final_transform, Vector3f point)
	{
		Vector3f quad_vertices[6];
		Coords3f coords = Coords3f::defCoords();
		coords.pos = point;
		Vector3f size = Vector3f(5.0f, 5.0f, 2.0f);
		quad_vertices[0] = coords.pos - coords.xVector * size.x;
		quad_vertices[1] = coords.pos + coords.xVector * size.x;
		quad_vertices[2] = coords.pos - coords.yVector * size.y;
		quad_vertices[3] = coords.pos + coords.yVector * size.y;

		simd::color color;
		float scale = 1.0f;
		color = 0x88bbbbbb;

		simd::vector3 line_verts[2];

		//side
		line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[0] - coords.pos) * scale);
		line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[1] - coords.pos)* scale);
		line.DrawTransform(&line_verts[0], 2, &final_transform, color.c());

		//top
		line_verts[0] = MakeDXVector(coords.pos + (quad_vertices[2] - coords.pos) * scale);
		line_verts[1] = MakeDXVector(coords.pos + (quad_vertices[3] - coords.pos) * scale);
		line.DrawTransform(&line_verts[0], 2, &final_transform, color.c());
	}

	void RenderDebugPrism(Device::Line & line, const simd::matrix & final_transform, Coords3f coords, Vector3f size, float angle, int segmentsCount, int color)
	{
		for(int segmentIndex = 0; segmentIndex < segmentsCount; segmentIndex++)
		{
			float ang = 2.0f * float(pi) / float(segmentsCount) * (float(segmentIndex) - 0.5f) + angle;
			float nextAng = ang + 2.0f * float(pi) / float(segmentsCount);

			simd::vector3 line_verts[2];
			Vector3f curr_radial_point = coords.GetWorldPoint(Vector3f(cos(ang) * size.x,     sin(ang) * size.y,     0.0f));
			Vector3f next_radial_point = coords.GetWorldPoint(Vector3f(cos(nextAng) * size.x, sin(nextAng) * size.y, 0.0f));

			Vector3f normal = coords.zVector * size.z * 0.5f;
			line_verts[0] = MakeDXVector(curr_radial_point - normal);
			line_verts[1] = MakeDXVector(curr_radial_point + normal);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
			line_verts[0] = MakeDXVector(curr_radial_point - normal);
			line_verts[1] = MakeDXVector(next_radial_point - normal);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
			line_verts[0] = MakeDXVector(curr_radial_point + normal);
			line_verts[1] = MakeDXVector(next_radial_point + normal);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
		}
	}


	void RenderDebugRing(Device::Line & line, const simd::matrix & final_transform, DebugRing ring, Coords3f coords, Vector3f scale)
	{
		RenderDebugPrism(line, final_transform, coords, Vector3f(ring.radius, ring.radius, ring.height) & scale, ring.angle, 10, ring.color);
	}
	void RenderDebugBox(Device::Line & line, const simd::matrix & final_transform, Coords3f coords, Vector3f size, int color)
	{
		int segmentsCount = 4;
		Vector3f offsets[4];
		offsets[0] = Vector3f( 1.0f, -1.0f, 0.0f);
		offsets[1] = Vector3f( 1.0f,  1.0f, 0.0f);
		offsets[2] = Vector3f(-1.0f,  1.0f, 0.0f);
		offsets[3] = Vector3f(-1.0f, -1.0f, 0.0f);
		for(int segmentIndex = 0; segmentIndex < segmentsCount; segmentIndex++)
		{
			float ang = 2.0f * float(pi) / float(segmentsCount) * (float(segmentIndex) - 0.5f);
			float nextAng = ang + 2.0f * float(pi) / float(segmentsCount);

			simd::vector3 line_verts[2];
			Vector3f curr_radial_point = coords.GetWorldPoint(offsets[segmentIndex] & size);
			Vector3f next_radial_point = coords.GetWorldPoint(offsets[(segmentIndex + 1) % segmentsCount] & size);

			Vector3f normal = coords.zVector * size.z * 0.5f;
			line_verts[0] = MakeDXVector(curr_radial_point - normal);
			line_verts[1] = MakeDXVector(curr_radial_point + normal);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
			line_verts[0] = MakeDXVector(curr_radial_point - normal);
			line_verts[1] = MakeDXVector(next_radial_point - normal);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
			line_verts[0] = MakeDXVector(curr_radial_point + normal);
			line_verts[1] = MakeDXVector(next_radial_point + normal);
			line.DrawTransform(&line_verts[0], 2, &final_transform, color);
		}
	}

	void WindSystem::RenderWireframe(Device::Line & line, const simd::matrix & final_transform)
	{
		for (size_t wind_index = 0; wind_index < explosion_wind_sources.GetElementsCount(); wind_index++)
		{
			auto &wind = explosion_wind_sources.GetByIndex(wind_index);
			RenderWindController(line, final_transform, wind.controller.GetCoords(), wind.controller.GetAABB(), wind.controller.GetDuration(), wind.controller.GetElapsedTime());

			for(size_t particle_index = 0; particle_index < wind.debugParticles.size(); particle_index++)
			{
				Vector3f particle_pos = wind.debugParticles[particle_index].pos;
				RenderDebugParticle(line, final_transform, particle_pos);
			}
			for(size_t ring_index = 0; ring_index < wind.debugRings.size(); ring_index++)
			{
				RenderDebugRing(line, final_transform, wind.debugRings[ring_index], wind.controller.GetCoords(), Vector3f(1.0f, 1.0f, 1.0f));
			}
		}
		for (size_t wind_index = 0; wind_index < vortex_wind_sources.GetElementsCount(); wind_index++)
		{
			auto &wind = vortex_wind_sources.GetByIndex(wind_index);
			RenderWindController(line, final_transform, wind.controller.GetCoords(), wind.controller.GetAABB(), wind.controller.GetDuration(), wind.controller.GetElapsedTime());
			for(size_t ring_index = 0; ring_index < wind.debugRings.size(); ring_index++)
			{
				RenderDebugRing(line, final_transform, wind.debugRings[ring_index], wind.controller.GetCoords(), Vector3f(1.0f, 1.0f, 1.0f));
			}
		}
		for (size_t wind_index = 0; wind_index < wake_wind_sources.GetElementsCount(); wind_index++)
		{
			auto &wind = wake_wind_sources.GetByIndex(wind_index);
			RenderWindController(line, final_transform, wind.controller.GetCoords(), wind.controller.GetAABB(), wind.controller.GetDuration(), wind.controller.GetElapsedTime());
			for(int vortex_index = 0; vortex_index < WakeWindSource::vorticesCount; vortex_index++)
			{
				Coords3f vortexCoords = wind.vortices[vortex_index].coords;
				RenderWakeVortex(line, final_transform, vortexCoords);
			}
			for(size_t particle_index = 0; particle_index < wind.debugParticles.size(); particle_index++)
			{
				Vector3f particle_pos = wind.debugParticles[particle_index].pos;
				RenderDebugParticle(line, final_transform, particle_pos);
			}
		}
		for (size_t wind_index = 0; wind_index < directional_wind_sources.GetElementsCount(); wind_index++)
		{
			auto &wind = directional_wind_sources.GetByIndex(wind_index);
			RenderWindController(line, final_transform, wind.controller.GetCoords(), wind.controller.GetAABB(), wind.controller.GetDuration(), wind.controller.GetElapsedTime());

			RenderDebugBox(line, final_transform, wind.controller.GetCoords(), wind.GetSize() & wind.controller.GetScale(), 0xff999999);
		}
	}
	Vector3f WindSystem::GetGlobalWindVelocity(Vector3f worldPoint) const
	{
		Vector3f res_velocity = Vector3f::zero();
		for(size_t explosion_index = 0; explosion_index < explosion_wind_sources.GetElementsCount(); explosion_index++)
		{
			res_velocity += explosion_wind_sources.GetByIndex(explosion_index).GetWindVelocity(worldPoint);
		}
		for(size_t vortex_index = 0; vortex_index < vortex_wind_sources.GetElementsCount(); vortex_index++)
		{
			res_velocity += vortex_wind_sources.GetByIndex(vortex_index).GetWindVelocity(worldPoint);
		}
		for(size_t wake_index = 0; wake_index < wake_wind_sources.GetElementsCount(); wake_index++)
		{
			res_velocity += wake_wind_sources.GetByIndex(wake_index).GetWindVelocity(worldPoint);
		}
		for (size_t directonal_index = 0; directonal_index < directional_wind_sources.GetElementsCount(); directonal_index++)
		{
			res_velocity += directional_wind_sources.GetByIndex(directonal_index).GetWindVelocity(worldPoint);
		}
		for(size_t turbulence_index = 0; turbulence_index < turbulence_wind_sources.GetElementsCount(); turbulence_index++)
		{
			res_velocity += turbulence_wind_sources.GetByIndex(turbulence_index).GetWindVelocity(worldPoint);
		}
		return res_velocity;
	}
}