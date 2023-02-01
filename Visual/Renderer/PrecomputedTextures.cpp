#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Renderer/TextureMapping.h"
#include "Visual/Mesh/FixedMesh.h"
#include "Visual/Mesh/VertexLayout.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Utility/DXBuffer.h"

namespace WaveUtil
{
	float Spline(float val)
	{
		return 3.0f * val * val - 2.0f * val * val * val;
	}

	float SmoothEndPoly(float val)
	{
		//a - 2bx
		//a = 1.5
		//a - 2b = 0
		//b = 
		return 1.0f - (val - 1.0f) * (val - 1.0f);
	}

	float GetWaveSurfaceVelocity(float waveCoord)
	{
		float breakPoint = 0.7f;
		float preBreakVelocity = -1.5f / (breakPoint);
		float postBreakVelocity = 1.5f / (1.0f - breakPoint);
		if (waveCoord < breakPoint) return preBreakVelocity;
		return postBreakVelocity;
	}

	float GetSurfaceDensity(float waveCoord)
	{
		return 1.0f + 99.0f * std::pow(waveCoord, 10.0f);
		if (waveCoord < 0.33f) return 1.0f;
		if (waveCoord < 0.66f) return 100.0f;
		return 1.0f;
	}

	float GetWaveSurfaceDst(float waveCoord)
	{
		float res = 0.0f;
		float eps = 1e-3f;
		for (float x = 1.0f; x > waveCoord; x -= eps)
		{
			res += GetWaveSurfaceVelocity(x) * eps;
		}
		return waveCoord + res;
	}

	float GetWaveOffset(float displacedCoord)
	{
		return Spline(1.0f - displacedCoord);//std::pow(displacedCoord, 1.0f);
		/*float waveCoord = displacedCoord;
		for(int i = 0; i < 100; i++)
		{
			waveCoord = waveCoord + (GetWaveSurfaceDst(waveCoord) - displacedCoord) * -1e-1f;
		}
		float test = GetWaveSurfaceDst(waveCoord) - displacedCoord;
		return Clamp(waveCoord - displacedCoord, -1.0f, 1.0f);*/
		float res = 0.0f;
		float eps = 1e-3f;
		for (float x = 0.0f; x < displacedCoord; x += eps)
		{
			res += GetSurfaceDensity(x) * eps;
		}
		float totalMass = 0.0f;
		for (float x = 0.0f; x < 1.0f; x += eps)
		{
			totalMass += GetSurfaceDensity(x) * eps;
		}

		return res / totalMass - displacedCoord;
	}

	Vector2d GetGerstnerWave(float param, float height)
	{
		float pi = 3.14159f;
		float wave_length = 1.0f;
		float wave_number = 2.0f * pi / wave_length;
		float orbit_radius = 1.0f / wave_number * height;

		float wave_phase = param * wave_number;
		return Vector2d(param - sin(wave_phase) * orbit_radius, cos(wave_phase)/* * orbit_radius*/);
	}

	Vector2d GetGersnterWaveOffset(float param, float height)
	{
		/*float best_dist = 1e5f;
		float best_param = 0.0f;
		for(float x = 0.0f; x < 1.0f; x += 1e-3f)
		{
			Vector2d particle_point = GetGerstnerWave(x, height);
			float dist = std::abs(param - particle_point.x);
			if(dist < best_dist)
			{
				best_dist = dist;
				best_param = x;
			}
		}
		Vector2d res_offset;
		res_offset.x = best_param - param;
		res_offset.y = GetGerstnerWave(best_param, height).y;
		return res_offset;*/

		float x = param;
		for (int i = 0; i < 10; i++)
		{
			Vector2d particle_point = GetGerstnerWave(x, height);
			float err = param - particle_point.x;
			x += err * 0.5f;
		}
		Vector2d res_offset;
		res_offset.x = x - param;
		res_offset.y = GetGerstnerWave(x, height).y;
		return res_offset;
	}

	float GetGerstnerWaveZeroWidth(float height)
	{
		float best_pos = 1e5f;
		float best_param = 0.0f;
		for (float x = 0.0f; x < 0.5f; x += 1e-3f)
		{
			Vector2d particle_point = GetGerstnerWave(x, height);
			float pos = std::abs(0.0f - particle_point.y);
			if (pos < best_pos)
			{
				best_pos = pos;
				best_param = particle_point.x;
			}
		}
		return best_param;
	}
}

namespace Renderer
{
	void BuildSurfaceRippleTexture(Device::IDevice *device, std::wstring filename)
	{
		using namespace WaveUtil;
		using namespace TexUtil;

		auto base_texture = ::Texture::Handle(::Texture::ReadableLinearTextureDesc(L"Art/Textures/Procedural/water_heightfield1.png")); //this texture is only in art repo, it's not exported
		Device::LockedRect base_rect;
		base_texture.GetTexture()->LockRect(0, &base_rect, Device::Lock::DEFAULT);
		LockedData<Color32> base_data(base_rect.pBits, base_rect.Pitch);

		unsigned surface_texture_width = base_texture.GetWidth();
		unsigned surface_texture_height = base_texture.GetHeight();
		auto surface_ripple_texture = Device::Texture::CreateTexture("Surface Ripple", device, surface_texture_width, surface_texture_height, 1, Device::UsageHint::DYNAMIC, Device::PixelFormat::A32B32G32R32F, Device::Pool::DEFAULT, false, false, false);

		Device::LockedRect surface_rect;
		surface_ripple_texture->LockRect(0, &surface_rect, Device::Lock::DEFAULT);
		LockedData<Color128> surface_data(surface_rect.pBits, surface_rect.Pitch);

		for (unsigned x = 0; x < surface_texture_width; x++)
		{
			for (unsigned y = 0; y < surface_texture_height; y++)
			{
				Color128 &dst_color = surface_data.Get(simd::vector2_int(x, y));
				dst_color.r = base_data.Get(simd::vector2_int(x, y)).ConvertTo128().r;
				dst_color.g = 0.0f;
				dst_color.b = 0.0f;
				dst_color.a = 1.0f;
			}
		}


		auto Wrapper = [&](simd::vector2_int tex_pos)
		{
			return simd::vector2_int(
				(tex_pos.x % surface_texture_width + surface_texture_width) % surface_texture_width,
				(tex_pos.y % surface_texture_height + surface_texture_height) % surface_texture_height);
		};

		auto HeightGetter = [&](simd::vector2_int tex_pos) -> float
		{
			float height_mult = surface_texture_width * 0.02f;
			return surface_data.Get(tex_pos).r * height_mult;
		};

		for (int x = 0; x < (int)surface_texture_width; x++)
		{
			for (int y = 0; y < (int)surface_texture_height; y++)
			{
				Color128 &dst_color = surface_data.Get(simd::vector2_int(x, y));
				dst_color.r = base_data.Get(simd::vector2_int(x, y)).ConvertTo128().r;

				int left = (x - 1 + surface_texture_width) % surface_texture_width;
				int right = (x + 1) % surface_texture_width;
				dst_color.g = (surface_data.Get(simd::vector2_int(right, y)).r - surface_data.Get(simd::vector2_int(left, y)).r) / 2.0f;

				int down = (y - 1 + surface_texture_height) % surface_texture_height;
				int up = (y + 1) % surface_texture_height;
				dst_color.b = (surface_data.Get(simd::vector2_int(x, up)).r - surface_data.Get(simd::vector2_int(x, down)).r) / 2.0f;

				/*simd::vector3 start_point = simd::vector3(x, y, HeightGetter(Vector2i(x, y)));
				dst_color.a = PathTraceSubsurfaceScattering(GeomFunc, start_point, simd::vector3(0.0f, 0.0f, -1.0f), 0.2f, 0.9f, 5e-1f);*/
			}
		}
		for (int x = 0; x < (int)surface_texture_width; x++)
		{
			for (int y = 0; y < (int)surface_texture_height; y++)
			{
				Color128 &dst_color = surface_data.Get(simd::vector2_int(x, y));
				dst_color.r = base_data.Get(simd::vector2_int(x, y)).ConvertTo128().r;
				float center_value = surface_data.Get(simd::vector2_int(x, y)).r;

				int left = (x - 1 + surface_texture_width) % surface_texture_width;
				int right = (x + 1) % surface_texture_width;
				float x_curvature = (surface_data.Get(simd::vector2_int(right, y)).r + surface_data.Get(simd::vector2_int(left, y)).r - 2.0f * center_value);

				int down = (y - 1 + surface_texture_height) % surface_texture_height;
				int up = (y + 1) % surface_texture_height;
				float y_curvature = (surface_data.Get(simd::vector2_int(x, up)).r + surface_data.Get(simd::vector2_int(x, down)).r - 2.0f * center_value);

				dst_color.a = x_curvature + y_curvature;
			}
		}

		base_texture.GetTexture()->UnlockRect(0);

		surface_ripple_texture->UnlockRect(0);
		surface_ripple_texture->SaveToFile(filename.c_str(), Device::ImageFileFormat::DDS);
	}

	struct MipLevel
	{
		std::vector<simd::vector4> data;
		size_t width, height, depth;
	};

	unsigned char ToUint8(float norm_float)
	{
		return (unsigned char)(Clamp(norm_float, 0.0f, 1.0f) * 255.0f);
	}
	void LoadGeomMipLevel(Device::Texture *tex, const MipLevel &level, size_t level_index)
	{
		Device::LockedBox volume_lock;
		tex->LockBox(UINT(level_index), &volume_lock, Device::Lock::DEFAULT);
		for (int z = 0; z < level.depth; z++)
		{
			for (int x = 0; x < level.width; x++)
			{
				for (int y = 0; y < level.height; y++)
				{
					TexUtil::Color32 *row = (TexUtil::Color32*)((char*)volume_lock.pBits + volume_lock.RowPitch * y + volume_lock.SlicePitch * z);
					auto *const dst_texel = row + x;
					simd::vector4 src_texel = level.data[x + y * level.width + z * level.width * level.height];
					dst_texel->r = ToUint8(1.0f / (src_texel.x + 1.0f));
					//dst_texel->r = ToUint8(src_texel.x / 10.0f);
					dst_texel->g = ToUint8(src_texel.y * 0.5f + 0.5f);
					dst_texel->b = ToUint8(src_texel.z * 0.5f + 0.5f);
					dst_texel->a = ToUint8(src_texel.w * 0.5f + 0.5f);
				}
			}
		}
		tex->UnlockBox(UINT(level_index));
	}

	void LoadColorMipLevel(Device::Texture *tex, const MipLevel &level, size_t level_index)
	{
		Device::LockedBox volume_lock;
		tex->LockBox(UINT(level_index), &volume_lock, Device::Lock::DEFAULT);
		for (int z = 0; z < level.depth; z++)
		{
			for (int x = 0; x < level.width; x++)
			{
				for (int y = 0; y < level.height; y++)
				{
					TexUtil::Color32 *row = (TexUtil::Color32*)((char*)volume_lock.pBits + volume_lock.RowPitch * y + volume_lock.SlicePitch * z);
					auto *const dst_texel = row + x;
					simd::vector4 src_texel = level.data[x + y * level.width + z * level.width * level.height];
					dst_texel->r = ToUint8(src_texel.x);
					dst_texel->g = ToUint8(src_texel.y);
					dst_texel->b = ToUint8(src_texel.z);
					dst_texel->a = ToUint8(src_texel.w);
				}
			}
		}
		tex->UnlockBox(UINT(level_index));
	}


	float Random(float min, float max)
	{
		return min + (max - min) * (float(std::rand()) / float(RAND_MAX));
	};
	
	float frac(float val)
	{
		return val - std::floor(val);
	};
	float rand_seed(float seed)
	{
		return frac(sin(seed) * 43758.5453123f);
	};

	float perlin(float val)
	{
		float low = floor(val);
		float ratio = val - low;
		float a = rand_seed(low);
		float b = rand_seed(low + 1.0f);
		return a * (1.0f - ratio) + b * ratio;
	}


	static simd::vector3 GetPointRayDist(simd::vector3 point, simd::vector3 origin, simd::vector3 dir)
	{
		float param = (origin + point).dot(dir) / -dir.dot(dir);
		return point - (origin + dir * param);
	}


	static simd::vector3 RotateVec(simd::vector3 vec, simd::vector3 axis, const float sin_ang, const float cos_ang)
	{
		simd::vector3 x = vec - axis * (axis * vec);
		simd::vector3 y = x.cross(axis);
		simd::vector3 delta = x * cos_ang + y * sin_ang - x;
		return vec + delta;
	}

	static simd::vector3 RotateVec(simd::vector3 vec, simd::vector3 axis0, simd::vector3 axis1)
	{
		simd::vector3 axis = axis0.cross(axis1);
		float sin_ang = axis.len();
		if (sin_ang != 0)
		{
			axis /= sin_ang; //normalize
			float cos_ang = axis0.dot(axis1);
			return RotateVec(vec, axis, sin_ang, cos_ang);
		}
		return vec;
	}

	static simd::vector2 HammersleyNorm(int i, int N)
	{
		// principle: reverse bit sequence of i
		uint32_t b = (uint32_t(i) << 16u) | (uint32_t(i) >> 16u);
		b = (b & 0x55555555u) << 1u | (b & 0xAAAAAAAAu) >> 1u;
		b = (b & 0x33333333u) << 2u | (b & 0xCCCCCCCCu) >> 2u;
		b = (b & 0x0F0F0F0Fu) << 4u | (b & 0xF0F0F0F0u) >> 4u;
		b = (b & 0x00FF00FFu) << 8u | (b & 0xFF00FF00u) >> 8u;

		return simd::vector2( static_cast< float >( i ), static_cast< float >( b ) ) / simd::vector2( static_cast< float >( N ), static_cast< float >( 0xffffffffU ) );
	}

	struct GrassRaycaster
	{
		GrassRaycaster()
		{
			this->density = 1;
			this->thickness = 0.4f;
			this->messiness = 1.0f;

			int count = 16 * density;
			//float dist_scale = count / 2.0f;



			float pi = 3.14f;


			for (int x = 0; x < count; x++)
			{
				for (int y = 0; y < count; y++)
				{
					newbie.pos = simd::vector2((float(x) + 0.5f) / float(count), (float(y) + 0.5f) / float(count));

					//newbie.pos += simd::vector2(random(-1.0f, 1.0f), random(-1.0f, 1.0f)) * (1.0f / float(count) * 0.25f);
					newbie.radius = 0.0f;// radius;

					float seed = float(cylinders.size());

					newbie.angular_offset = simd::vector2(
						(rand_seed(seed + 0.05f) * 2.0f - 1.0f) * 0.1f,
						(rand_seed(seed + 0.10f) * 2.0f - 1.0f) * 0.1f);

					newbie.curl_offset_const = simd::vector2(
						rand_seed(seed + 0.20f) * 2.0f * pi,
						rand_seed(seed + 0.30f) * 2.0f * pi);

					newbie.curl_offset_linear = simd::vector2(
						2.0f * 1.6f * (1.0f + 0.5f * rand_seed(seed + 0.15f)),
						2.0f * 1.7f * (1.0f + 0.5f * rand_seed(seed + 0.25f)));


					newbie.radius = 0.25f + 0.5f * rand_seed(seed + 0.35f);
					float ang = rand_seed(seed + 0.45f) * 3.1415f * 2.0f;
					float dist = sqrt(rand_seed(seed + 0.5f));
					//newbie.rosette_offset = simd::vector2(Random(-1.0f, 1.0f), Random(-1.0f, 1.0f)) * 1.0f;
					newbie.rosette_offset = simd::vector2(cos(ang), sin(ang)) * dist / static_cast< float >( density ) * messiness;


					newbie.height = rand_seed(seed + 0.55f) * 0.3f;
					cylinders.push_back(newbie);
				}
			}
		}
		struct RaycastResult
		{
			float dist;
			simd::vector3 normal;
		};

		RaycastResult CastRay(simd::vector2 point, simd::vector2 dir)
		{

			float const_radius = 0.005f; //to avoid aliasing on the tip
			float linear_radius = 0.1f * thickness;

			RaycastResult res;
			res.normal = simd::vector3(-1.0f, -1.0f, -1.0f);
			res.dist = 100.0f;

			float step = 1.0f / 512.0f * thickness * 5.0f;
			for (float dist = 0.0f; (dist < 10.0f / density); dist += step)
			{
				simd::vector2 curr_point = point + dir * dist;
				curr_point.x = frac(curr_point.x);
				curr_point.y = frac(curr_point.y);


				int cylinder_index = 0;
				for (auto &cylinder : cylinders)
				{
					simd::vector2 seed = simd::vector2(dist, float(cylinder_index));

					simd::vector2 curr_cylinder_pos = cylinder.GetOffsetPos(dist, static_cast< float >( density ) );
					curr_cylinder_pos.x = frac(curr_cylinder_pos.x);
					curr_cylinder_pos.y = frac(curr_cylinder_pos.y);
					float cylinder_height = cylinder.height;

					simd::vector3 curr_cylinder_dir = cylinder.GetDir(dist, static_cast< float >( density ) );

					if (dist > cylinder_height)
					{
						float blade_coord = std::max(0.0f, dist - cylinder_height);
						float blade_width = (1.0f - 1.0f / (1.0f + blade_coord * 30.0f)) * 0.4f;
						//float blade_width = pow(blade_coord, 0.5f);
						//float blade_width = blade_coord;
						float r = const_radius + linear_radius * blade_width * cylinder.radius;

						for (int y_offset = -1; y_offset <= 1; y_offset++)
						{
							for (int x_offset = -1; x_offset <= 1; x_offset++)
							{
								simd::vector2 delta = curr_point - curr_cylinder_pos + simd::vector2(float(x_offset), float(y_offset));
								simd::vector2 flat_normal = delta.normalize();
								/*simd::vector3 origin = simd::vector3(curr_cylinder_pos + simd::vector2(float(x_offset), float(y_offset)), 0.0f);
								simd::vector3 point = simd::vector3(curr_point, 0.0f);

								simd::vector3 delta = GetPointRayDist(point, origin, curr_cylinder_dir);*/

								if (delta.len() < r/* && flat_normal.dot(dir) < 0.0f*/)
								{
									res.dist = dist;
									//res.normal = delta.normalize();// simd::vector3(flat_normal.x, flat_normal.y, 0.0f);

									//res.normal = simd::vector3(flat_normal.x, flat_normal.y, 0.0f);
									//res.normal = RotateVec(res.normal, simd::vector3(0.0f, 0.0f, 1.0f), curr_cylinder_dir);
									res.normal = curr_cylinder_dir.cross({ 0.0f, 0.0f, 1.0f }).cross(curr_cylinder_dir).normalize();
									//if (res.normal.dot(simd::vector3(dir, 0.0f)) > 0.0f)
									if (res.normal.z > 0.0f)
											res.normal *= -1.0f;
									//res.normal = RotateVec(res.normal, simd::vector3(0.0f, 0.0f, 1.0f), curr_cylinder_dir);


									return res;
								}
							}
						}
					}
					cylinder_index++;
				}
			}
			return res;
		};
	private:
		int density;
		float thickness;
		float messiness;

		struct Cylinder
		{
			simd::vector2 pos;
			float radius;

			float height;
			simd::vector2 angular_offset;
			simd::vector2 curl_offset_const;
			simd::vector2 curl_offset_linear;
			simd::vector2 rosette_offset;

			simd::vector2 GetOffsetPos(float dist, float density)
			{
				simd::vector2 offset = simd::vector2(0.0f, 0.0f);

				//cylinder_offset += cylinder.angular_offset * (5.0f * 0.0f + dist * dist_scale) / dist_scale;// *3.0f;
				offset += rosette_offset * (1.0f / (1.0f + dist * density));

				offset += simd::vector2(
					cos(dist * curl_offset_linear.x + curl_offset_const.x),
					cos(dist * curl_offset_linear.y + curl_offset_const.y)) * 0.03f;

				return pos + offset;
			}

			simd::vector3 GetDir(float dist, float density)
			{
				float eps = 1e-2f;
				simd::vector3 p0 = simd::vector3(GetOffsetPos(dist, density), 0.0f);
				simd::vector3 p1 = simd::vector3(GetOffsetPos(dist + eps, density), eps);
				return (p1 - p0).normalize();
			}
		};
		std::vector<Cylinder> cylinders;
		Cylinder newbie;
	};

	struct FungusRaycaster
	{
		FungusRaycaster()
		{
			this->density = 1;
			this->thickness = 0.6f;
			this->messiness = 0.2f;

			int count = 2 * density;
			//float dist_scale = count / 2.0f;



			float pi = 3.14f;


			for (int x = 0; x < count; x++)
			{
				for (int y = 0; y < count; y++)
				{
					float seed = float(cylinders.size());
					newbie.pos = HammersleyNorm(x + y * count, count * count); //simd::vector2((float(x) + 0.5f) / float(count), (float(y) + 0.5f) / float(count));
					/*{
						float ang = rand_seed(seed + 0.70f) * 3.1415f * 2.0f;
						float dist = sqrt(rand_seed(seed + 0.75f));
						//newbie.rosette_offset = simd::vector2(Random(-1.0f, 1.0f), Random(-1.0f, 1.0f)) * 1.0f;
						newbie.pos += simd::vector2(cos(ang), sin(ang)) * dist / density * 0.1f * 0.0f;
					}*/

					//newbie.pos += simd::vector2(random(-1.0f, 1.0f), random(-1.0f, 1.0f)) * (1.0f / float(count) * 0.25f);
					newbie.radius = 0.0f;// radius;


					newbie.curl_offset_const = simd::vector2(
						rand_seed(seed + 0.20f) * 2.0f * pi,
						rand_seed(seed + 0.30f) * 2.0f * pi);

					newbie.curl_offset_linear = simd::vector2(
						6.0f * (-1.0f + 2.0f * rand_seed(seed + 0.15f)),
						6.0f * (-1.0f + 2.0f * rand_seed(seed + 0.25f)));


					newbie.radius = 0.25f + 0.25f * rand_seed(seed + 0.35f);
					{
						simd::vector2 r = HammersleyNorm(y + x * count, count * count);

						/*float ang = r.x * 3.1415f * 2.0f;
						float dist = sqrt(r.y);*/

						float ang = rand_seed(seed + 0.55f) * 3.1415f * 2.0f;
						float dist = sqrt(rand_seed(seed + 0.5f));
						//newbie.rosette_offset = simd::vector2(Random(-1.0f, 1.0f), Random(-1.0f, 1.0f)) * 1.0f;
						newbie.rosette_offset = simd::vector2(cos(ang), sin(ang)) * dist / static_cast< float >( density ) * messiness;
					}


					newbie.height = rand_seed(seed + 0.65f) * 0.3f;
					cylinders.push_back(newbie);
				}
			}
		}
		struct RaycastResult
		{
			float dist;
			simd::vector3 normal;
		};

		RaycastResult CastRay(simd::vector2 point, simd::vector2 dir)
		{

			float const_radius = 0.005f; //to avoid aliasing on the tip
			float linear_radius = 0.1f * thickness;

			RaycastResult res;
			res.normal = simd::vector3(-1.0f, -1.0f, -1.0f);
			res.dist = 100.0f;

			float step = 1.0f / 512.0f * thickness * 10.0f;
			for (float dist = 0.0f; (dist < 10.0f / density); dist += step)
			{
				simd::vector2 curr_point = point + dir * dist;
				curr_point.x = frac(curr_point.x);
				curr_point.y = frac(curr_point.y);


				int cylinder_index = 0;
				for (auto &cylinder : cylinders)
				{
					simd::vector2 seed = simd::vector2(dist, float(cylinder_index));

					simd::vector2 curr_cylinder_pos = cylinder.GetOffsetPos(dist, static_cast< float >( density) );
					curr_cylinder_pos.x = frac(curr_cylinder_pos.x);
					curr_cylinder_pos.y = frac(curr_cylinder_pos.y);
					float cylinder_height = cylinder.height;

					simd::vector3 curr_cylinder_dir = cylinder.GetDir(dist, static_cast< float >( density) );

					if (dist > cylinder_height)
					{
						float blade_coord = std::max(0.0f, dist - cylinder_height);
						float blade_width = (1.0f - 1.0f / (1.0f + blade_coord * 7.0f));
						blade_width *= 1.0f / (1.0f + std::max(0.0f, blade_coord - 0.45f) * 30.0f) * 2.5f + 0.05f;
						blade_width += std::max(0.0f, blade_coord - 0.45f) * 1.0f;
						//blade_width += std::max(0.0f, blade_coord) * 1.0f;
						//blade_width *= (1.0f + std::pow((sin(blade_coord * 20.0f) + 1.0f) * 0.5f, 250) * 1.0f);
						//float blade_width = pow(blade_coord, 0.5f);
						//float blade_width = blade_coord;
						float r = const_radius + linear_radius * blade_width * cylinder.radius;

						for (int y_offset = -1; y_offset <= 1; y_offset++)
						{
							for (int x_offset = -1; x_offset <= 1; x_offset++)
							{
								simd::vector2 delta = curr_point - curr_cylinder_pos + simd::vector2(float(x_offset), float(y_offset));
								simd::vector2 flat_normal = delta.normalize();
								/*simd::vector3 origin = simd::vector3(curr_cylinder_pos + simd::vector2(float(x_offset), float(y_offset)), 0.0f);
								simd::vector3 point = simd::vector3(curr_point, 0.0f);

								simd::vector3 delta = GetPointRayDist(point, origin, curr_cylinder_dir);*/

								if (delta.len() < r/* && flat_normal.dot(dir) < 0.0f*/)
								{
									res.dist = dist;
									//res.normal = delta.normalize();// simd::vector3(flat_normal.x, flat_normal.y, 0.0f);

									res.normal = simd::vector3(flat_normal.x, flat_normal.y, 0.0f);
									res.normal = RotateVec(res.normal, simd::vector3(0.0f, 0.0f, 1.0f), curr_cylinder_dir);
									//res.normal = curr_cylinder_dir.cross({ 0.0f, 0.0f, 1.0f }).cross(curr_cylinder_dir).normalize();
									//if (res.normal.dot(simd::vector3(dir, 0.0f)) > 0.0f)
									//if (res.normal.z > 0.0f)
									//		res.normal *= -1.0f;
									//res.normal = RotateVec(res.normal, simd::vector3(0.0f, 0.0f, 1.0f), curr_cylinder_dir);


									return res;
								}
							}
						}
					}
					cylinder_index++;
				}
			}
			return res;
		};
	private:
		int density;
		float thickness;
		float messiness;


		struct Cylinder
		{
			simd::vector2 pos;
			float radius;

			float height;
			simd::vector2 curl_offset_const;
			simd::vector2 curl_offset_linear;
			simd::vector2 rosette_offset;

			simd::vector2 GetOffsetPos(float dist, float density)
			{
				simd::vector2 offset = simd::vector2(0.0f, 0.0f);

				//cylinder_offset += cylinder.angular_offset * (5.0f * 0.0f + dist * dist_scale) / dist_scale;// *3.0f;
				//offset += rosette_offset * dist * density + const_offset;
				float coord = std::max(0.0f, dist - height);
				offset += rosette_offset * (1.0f - 1.0f / (1.0f + 3.0f * coord * density));
				float x = coord * curl_offset_linear.x;// +curl_offset_const.x;
				float y = coord * curl_offset_linear.y;// +curl_offset_const.y;
				//float a = std::max(0.0f, dist - height) * 0.05f * Clamp((dist - height - 0.35f) * 10.0f, 0.5f, 1.0f);
				float a = 0.05f * 0.0f;
				for (int i = 0; i < 2; i++)
				{
					//offset += simd::vector2(
					//  perlin(x) * 2.0f - 1.0f,
					//  perlin(y) * 2.0f - 1.0f) * a;
					offset += simd::vector2(
						sin(x),
						sin(y)) * a;
					x *= 2.0f;
					y *= 2.0f;
					a /= 2.0f;
				}

				return pos + offset;
			}

			simd::vector3 GetDir(float dist, float density)
			{
				float eps = 1e-2f;
				simd::vector3 p0 = simd::vector3(GetOffsetPos(dist, density), 0.0f);
				simd::vector3 p1 = simd::vector3(GetOffsetPos(dist + eps, density), eps);
				return (p1 - p0).normalize();
			}
		};
		std::vector<Cylinder> cylinders;
		Cylinder newbie;
	};

	static float RayPlaneIntersect(simd::vector3 plane_point, simd::vector3 plane_norm, simd::vector3 ray_origin, simd::vector3 ray_dir)
	{
		return -(ray_origin - plane_point).dot(plane_norm) / ray_dir.dot(plane_norm);
	}

	bool RayTriangleIntersect(simd::vector3 ray_origin, simd::vector3 ray_dir, simd::vector3 triangle_points[3], float &ray_param, simd::vector2 &uv)
	{
		//https://cadxfem.org/inf/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf

		simd::vector3 E1 = triangle_points[1] - triangle_points[0];
		simd::vector3 E2 = triangle_points[2] - triangle_points[0];
		simd::vector3 D = ray_dir;
		simd::vector3 T = ray_origin - triangle_points[0];
		simd::vector3 P = ray_dir.cross(E2);
		simd::vector3 Q = T.cross(E1);

		float divider = P.dot(E1);
		if (fabs(divider) < 1e-7f)
			return false;

		float mult = 1.0f / divider;


		ray_param = Q.dot(E2) * mult;
		uv.x = P.dot(T) * mult;
		uv.y = Q.dot(D) * mult;
		if (uv.x < 0.0f || uv.x > 1.0f)
			return false;

		if (uv.y < 0.0f || uv.x + uv.y > 1.0f)
			return false;

		if (ray_param < 0.0f)
			return false;

		return true;
	}

	struct CrystalRaycaster
	{
		CrystalRaycaster()
		{
			float density = 1.0f;
			float messiness = 1.0f;

			int count = 8;// 16 * density;

			float pi = 3.14f;

			for (int x = 0; x < count; x++)
			{
				for (int y = 0; y < count; y++)
				{
					Spike newbie;
					simd::vector2 tip_point2 = HammersleyNorm(x + y * count, count * count);
					newbie.tip_point = simd::vector3(tip_point2.x, tip_point2.y, 0.0f);

					float seed = float(spikes.size());
					newbie.tip_point.z = rand_seed(seed + 0.0f) * 0.3f;
					
					float ang = rand_seed(seed + 0.05f) * 2.0f * 3.1415f;
					float radius = rand_seed(seed + 0.1f) * messiness;
					newbie.dir = simd::vector3(cos(ang) * radius, sin(ang) * radius, 1.0f).normalize();
					newbie.tangent = newbie.dir.cross(simd::vector3(0.0f, 0.0f, 1.0f)).normalize();
					newbie.norm = newbie.dir.cross(newbie.tangent);

					newbie.max_width = 10.0f / 2.0f * 0.1f;
					newbie.width_ang = 0.05f;

					spikes.push_back(newbie);
				}
			}
		}
		struct RaycastResult
		{
			float dist;
			simd::vector3 normal;
		};

		RaycastResult CastRay(simd::vector2 point, simd::vector2 dir)
		{
			RaycastResult res;
			res.normal = simd::vector3(-1.0f, -1.0f, -1.0f);
			res.dist = 100.0f;

			simd::vector3 curr_point = simd::vector3(point.x, point.y, 0.0f);
			float step = 1.0f / 64.0f;
			for (float dist = 0.0f; (dist < 15.0f); dist += step)
			{
				curr_point.x = frac(curr_point.x);
				curr_point.y = frac(curr_point.y);

				float depth_ang = 1.0f;
				simd::vector3 next_point = curr_point + simd::vector3(
					dir.x * step,
					dir.y * step,
					step * depth_ang);

				for (auto &spike : spikes)
				{
					for (int y_offset = -1; y_offset <= 1; y_offset++)
					{
						for (int x_offset = -1; x_offset <= 1; x_offset++)
						{
							simd::vector3 offset = simd::vector3(float(x_offset), float(y_offset), 0.0f);
							simd::vector3 edge_points[] = {
								curr_point + offset,
								next_point + offset
							};

							simd::vector3 int_point;
							simd::vector3 int_norm;
							if (spike.GetEdgeIntersection(edge_points, int_point, int_norm))
							{
								res.dist = dist;
								res.normal = int_norm;
								if (int_norm.dot(edge_points[1] - edge_points[0]) > 0.0f)
									int_norm *= -1.0f;

								return res;
							}
						}
					}
				}
				curr_point = next_point;
			}
			return res;
		};
	private:
		struct Spike
		{
			simd::vector3 tip_point;

			simd::vector3 dir;
			simd::vector3 tangent;
			simd::vector3 norm;

			float max_width;
			float width_ang;


			/*simd::vector2 GetStemUV(simd::vector2 leaf_uv, float leaf_ang, float stem_width)
			{
				return simd::vector2();
			}*/
			bool GetEdgeIntersection(simd::vector3 edge_points[2], simd::vector3 &int_point, simd::vector3 &int_norm)
			{
				float param = RayPlaneIntersect(tip_point, norm, edge_points[0], edge_points[1] - edge_points[0]);
				if (param < 0.0f || param > 1.0f)
					return false;

				int_point = edge_points[0] + (edge_points[1] - edge_points[0]) * param;
				int_norm = this->norm;
				simd::vector3 delta = int_point - tip_point;
				simd::vector2 uv = simd::vector2(
					delta.dot(tangent),
					delta.dot(dir)
				);

				return (abs(uv.x) < max_width) && (abs(uv.x) < width_ang * (1.0f + frac(-uv.y * 50.0f) * 5.0f) * uv.y);
			}
		};
		std::vector<Spike> spikes;
	};

	struct MeshRaycaster
	{
		MeshRaycaster()
		{
			std::wstring fmt_filename = L"Art/LeafLevel01.fmt";
			simd::vector3 model_scale = simd::vector3(1.0f, -1.0f, -1.0f) / 100.0f;

			Mesh::FixedMeshHandle mesh_handle(fmt_filename);
			std::vector<uint8_t> raw_vertex_data = Utility::ReadVertices(mesh_handle->GetVertexBuffer());

			this->indices = Utility::ReadIndices<size_t>(mesh_handle->GetIndexBuffer());

			{
				using namespace TexUtil;
				auto albedo_texture = ::Texture::Handle(::Texture::ReadableLinearTextureDesc(L"Art/DeadLeavesColour.png")); //this texture is only in art repo, it's not exported
				albedo_tex_data.size = simd::vector2_int(albedo_texture.GetWidth(), albedo_texture.GetHeight());
				albedo_tex_data.pixels.resize(albedo_tex_data.size.x * albedo_tex_data.size.y);

				Device::LockedRect base_rect;
				albedo_texture.GetTexture()->LockRect(0, &base_rect, Device::Lock::DEFAULT);
				LockedData<Color32> base_data(base_rect.pBits, base_rect.Pitch);
				for (size_t y = 0; y < albedo_tex_data.size.y; y++)
				{
					for (size_t x = 0; x < albedo_tex_data.size.x; x++)
					{
						albedo_tex_data.pixels[x + y * albedo_tex_data.size.x] = base_data.Get(simd::vector2_int( static_cast< int >( x ), static_cast< int >( y ) )).ConvertTo128();
					}
				}
				albedo_texture.GetTexture()->UnlockRect(0);
			}

			Mesh::VertexLayout layout(mesh_handle->GetFlags());
			this->vertices.resize(mesh_handle->GetVertexCount());
			for (size_t vertex_index = 0; vertex_index < vertices.size(); vertex_index++)
			{
				const auto& raw_position = layout.GetPosition((uint32_t)vertex_index, raw_vertex_data.data());
				const auto& raw_normal = layout.GetNormal((uint32_t)vertex_index, raw_vertex_data.data());
				const auto& raw_uv = layout.GetUV1((uint32_t)vertex_index, raw_vertex_data.data());

				auto &vertex = vertices[vertex_index];
				vertex.pos = raw_position * model_scale;
				vertex.pos.z = 1.0f - vertex.pos.z;
				//vertex.position.z -= 1.0f;
				simd::vector3 norm = simd::vector3(raw_normal.x, raw_normal.y, raw_normal.z) / 255.0f * 2.0f - simd::vector3(1.0f, 1.0f, 1.0f);
				norm = (norm * model_scale).normalize();
				vertex.norm.x = norm.x;
				vertex.norm.y = norm.y;
				vertex.norm.z = norm.z;
				vertex.norm.z *= -1.0f;

				vertex.uv.x = frac(raw_uv.x.tofloat());
				vertex.uv.y = frac(raw_uv.y.tofloat());
			}
		}
		struct RaycastResult
		{
			float dist;
			simd::vector3 normal;
			simd::vector4 color;
		};

		RaycastResult CastRay(simd::vector2 point, simd::vector2 dir)
		{
			RaycastResult res;
			res.normal = simd::vector3(-1.0f, -1.0f, -1.0f);
			res.dist = 100.0f;
			res.color = simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);

			simd::vector2_int prev_cell_index = { -1, -1 };

			simd::vector3 initial_point = simd::vector3(point.x, point.y, 0.0f);
			simd::vector3 curr_point = initial_point;
			float step = 1.0f / 100.0f;
			for (float dist = 0.0f; (dist < 15.0f); dist += step)
			{
				simd::vector2_int curr_cell_index = { int(floor(curr_point.x)), int(floor(curr_point.y)) };

				float depth_ang = 1.0f;
				simd::vector3 next_point = curr_point + simd::vector3(
					dir.x * step,
					dir.y * step,
					step * depth_ang);

				float max_dist = 1e5;
				float closest_int = max_dist;

				if (curr_cell_index.x != prev_cell_index.x || curr_cell_index.y != prev_cell_index.y)
				{
					prev_cell_index = curr_cell_index;
					size_t triangles_count = indices.size() / 3;
					for (size_t triangle_index = 0; triangle_index < triangles_count; triangle_index++)
					{
						simd::vector3 triangle_points[3];
						simd::vector3 triangle_normals[3];
						simd::vector2 triangle_uvs[3];
						for (size_t vertex_number = 0; vertex_number < 3; vertex_number++)
						{
							size_t vertex_index = indices[triangle_index * 3 + vertex_number];
							triangle_points[vertex_number] = vertices[vertex_index].pos;
							triangle_normals[vertex_number] = vertices[vertex_index].norm;
							triangle_uvs[vertex_number] = vertices[vertex_index].uv;
						}

						for (int y_offset = -1; y_offset <= 1; y_offset++)
						{
							for (int x_offset = -1; x_offset <= 1; x_offset++)
							{
								simd::vector3 offset = simd::vector3(float(x_offset), float(y_offset), 0.0f);

								float ray_param;
								simd::vector2 uv;
								simd::vector3 dir = next_point - curr_point;
								simd::vector3 clamped_point = { frac(curr_point.x), frac(curr_point.y), curr_point.z };

								clamped_point.z = std::min(clamped_point.z, 0.95f); //bad extrusion

								if (RayTriangleIntersect(clamped_point + offset, dir, triangle_points, ray_param, uv) && ray_param < closest_int)
								{
									//simd::vector3 norm = (triangle_points[1] - triangle_points[0]).cross(triangle_points[2] - triangle_points[0]).normalize();
									simd::vector3 norm = (triangle_normals[1] * uv.x + triangle_normals[2] * uv.y + triangle_normals[0] * (1.0f - uv.x - uv.y)).normalize();
									simd::vector2 int_uv = triangle_uvs[1] * uv.x + triangle_uvs[2] * uv.y + triangle_uvs[0] * (1.0f - uv.x - uv.y);

									simd::vector3 int_point = next_point + dir * ray_param;
									auto color = this->albedo_tex_data.Sample(int_uv);
									if (color.w > 0.5f)
									{
										res.dist = (int_point - initial_point).len();// dist + step * ray_param;
										if (norm.dot(dir) > 0.0f)
											norm *= -1.0f;
										res.normal = norm;
										res.color = color;
										closest_int = ray_param;
									}
								}
							}
						}
					}
					if (closest_int < max_dist)
						return res;
				}
				curr_point = next_point;
			}
			return res;
		};
	private:
		struct Vertex
		{
			simd::vector3 pos;
			simd::vector3 norm;
			simd::vector2 uv;
		};
		std::vector<Vertex> vertices;
		std::vector<size_t> indices;

		struct TexData
		{
			simd::vector4 Sample(simd::vector2 uv)
			{
				simd::vector2 pixel_coord_2f = { uv.x * size.x, uv.y * size.y };
				simd::vector2_int pixel_coord_2i = {int(pixel_coord_2f.x), int(pixel_coord_2f.y) };

				simd::vector2 ratio = { frac(pixel_coord_2f.x), frac(pixel_coord_2f.y) };
				pixel_coord_2i.x = pixel_coord_2i.x % size.x;
				pixel_coord_2i.y = pixel_coord_2i.y % size.y;

				//auto pixel = pixels[pixel_coord.x + pixel_coord.y * size.x];
				auto min_2i = pixel_coord_2i;
				auto max_2i = simd::vector2_int{ (pixel_coord_2i.x + 1) % size.x, (pixel_coord_2i.y + 1) % size.y };
				auto pixel =
					pixels[min_2i.x + min_2i.y * size.x] * (1.0f - ratio.x) * (1.0f - ratio.y) +
					pixels[max_2i.x + min_2i.y * size.x] * (ratio.x) * (1.0f - ratio.y) +
					pixels[max_2i.x + max_2i.y * size.x] * ratio.x * ratio.y +
					pixels[min_2i.x + max_2i.y * size.x] * (1.0f - ratio.x) * ratio.y;
				return { pixel.r, pixel.g, pixel.b, pixel.a };
			}
			std::vector<TexUtil::Color128> pixels;
			simd::vector2_int size;
		};
		TexData albedo_tex_data;
	};

	void BuildRaycastTexture(Device::IDevice *device, std::wstring filename)
	{
		/*using Raycaster = GrassRaycaster;
		int spacial_size = 128;
		int angular_size = 16;*/

		/*using Raycaster = FungusRaycaster;
		int spacial_size = 128;
		int angular_size = 16;*/

		/*using Raycaster = CrystalRaycaster;
		int spacial_size = 128;
		int angular_size = 16;*/

		using Raycaster = MeshRaycaster;
		int spacial_size = 128;
		int angular_size = 16;

		int subpixel_size = 1;
		int angle_subpixel_size = 1;

		Raycaster raycaster;

		/*float radius_mult = 0.5f;
		newbie.pos = simd::vector2(0.35, 0.8);
		newbie.radius = 0.2f * radius_mult;
		cylinders.push_back(newbie);

		newbie.pos = simd::vector2(0.30, 0.30);
		newbie.radius = 0.30f * radius_mult;
		cylinders.push_back(newbie);

		newbie.pos = simd::vector2(0.77, 0.53);
		newbie.radius = 0.23f * radius_mult;
		cylinders.push_back(newbie);

		newbie.pos = simd::vector2(0.85, 0.15);
		newbie.radius = 0.15f * radius_mult;
		cylinders.push_back(newbie);*/


		


		

		MipLevel geom_mip0;
		geom_mip0.width = spacial_size;
		geom_mip0.height = spacial_size;
		geom_mip0.depth = angular_size;
		geom_mip0.data.resize(spacial_size * spacial_size * angular_size);

		MipLevel color_mip0;
		color_mip0.width = spacial_size;
		color_mip0.height = spacial_size;
		color_mip0.depth = angular_size;
		color_mip0.data.resize(spacial_size * spacial_size * angular_size);

		std::vector<simd::vector2> subpixel_offsets;
		for (int x = 0; x < subpixel_size; x++)
		{
			for (int y = 0; y < subpixel_size; y++)
			{
				subpixel_offsets.push_back(simd::vector2(x + 0.5f, y + 0.5f) / float(subpixel_size) - simd::vector2(0.5f, 0.5f));
			}
		}

		std::vector<float> angle_subpixel_offsets;
		for (int x = 0; x < angle_subpixel_size; x++)
		{
			angle_subpixel_offsets.push_back((x + 0.5f) / float(angle_subpixel_size) - 0.5f);
		}

		for (int ang_index = 0; ang_index < angular_size; ang_index++)
		{
			for (int x = 0; x < spacial_size; x++)
			{
				for (int y = 0; y < spacial_size; y++)
				{
					simd::vector2 norm_coord = simd::vector2(float(x + 0.5f) / float(spacial_size), float(y + 0.5f) / float(spacial_size));
						
					Raycaster::RaycastResult sum_res;
					sum_res.dist = 0.0f;
					sum_res.normal = simd::vector3(0.0f, 0.0f, 0.0f);
					sum_res.color = simd::vector4(0.0f, 0.0f, 0.0f, 0.0f);
					float sum_weight = 0.0f;

					for (auto angle_subpixel_offset : angle_subpixel_offsets)
					{
						float ang = float(ang_index + 0.5f + angle_subpixel_offset) / float(angular_size) * 3.1415f * 2.0f; //offset to avoid cycling
						simd::vector2 dir = simd::vector2(cos(ang), sin(ang));


						for (auto subpixel_offset : subpixel_offsets)
						{
							simd::vector2 norm_offset = subpixel_offset * simd::vector2(1.0f / float(spacial_size), 1.0f / float(spacial_size));
							Raycaster::RaycastResult res = raycaster.CastRay(norm_coord + norm_offset, dir);
							sum_res.dist += res.dist;
							sum_res.normal += res.normal;
							sum_res.color = sum_res.color + res.color;
							sum_weight += 1.0f;
						}
					}
					sum_res.dist *= 1.0f / sum_weight;
					sum_res.normal *= 1.0f / sum_weight;
					sum_res.color = sum_res.color / sum_weight;

					float avg_ang = float(ang_index + 0.5f) / float(angular_size) * 3.1415f * 2.0f; //offset to avoid cycling
					simd::vector2 dir = simd::vector2(cos(avg_ang), sin(avg_ang));

					simd::vector3 perp = simd::vector3(-dir.y, dir.x, 0.0f);

					{
						simd::vector4 &dst_texel = geom_mip0.data[x + y * spacial_size + ang_index * spacial_size * spacial_size];
						dst_texel.x = sum_res.dist;
						dst_texel.y = sum_res.normal.x;
						dst_texel.z = sum_res.normal.y;
						dst_texel.w = sum_res.normal.z;
					}

					{
						simd::vector4 &dst_texel = color_mip0.data[x + y * spacial_size + ang_index * spacial_size * spacial_size];
						dst_texel = sum_res.color;
					}
				}
			}
		}

		auto geom_tex = Device::Texture::CreateVolumeTexture("Raycast Volume", device, UINT(geom_mip0.width), UINT(geom_mip0.height), UINT(geom_mip0.depth), UINT(1), Device::UsageHint::DEFAULT, Device::PixelFormat::A8R8G8B8, Device::Pool::SYSTEMMEM, false);
		LoadGeomMipLevel(geom_tex.Get(), geom_mip0, 0);
		geom_tex->SaveToFile((filename + L"_geom.dds").c_str(), Device::ImageFileFormat::DDS);

		auto color_tex = Device::Texture::CreateVolumeTexture("Raycast Volume", device, UINT(color_mip0.width), UINT(color_mip0.height), UINT(color_mip0.depth), UINT(1), Device::UsageHint::DEFAULT, Device::PixelFormat::A8R8G8B8, Device::Pool::SYSTEMMEM, true);
		LoadColorMipLevel(color_tex.Get(), color_mip0, 0);
		color_tex->SaveToFile((filename + L"_color.dds").c_str(), Device::ImageFileFormat::DDS);
	}

	using float3 = simd::vector3;
	using float2 = simd::vector2;

	//particle displacement
	float2 GetGerstnerDisplacementProfile(float wave_coord, float gerstner_radius)
	{
		float pi = 3.141592f;
		float ang = 2.0f * pi * wave_coord;
		return float2(-sin(ang), cos(ang)) * gerstner_radius;
	}

	using ErrFunc = std::function<float(float)>;
	float Bisect(ErrFunc func, float min_val, float max_val, int steps_count)
	{
		float min_err = func(min_val);
		float max_err = func(max_val);
		for (int i = 0; i < steps_count; i++)
		{
			assert(min_err * max_err <= 0.0f);

			float mid_val = (min_val + max_val) * 0.5f;
			float mid_err = func(mid_val);
			if (mid_err * min_err <= 0.0f)
			{
				max_val = mid_val;
				max_err = mid_err;
			}
			else
			{
				min_val = mid_val;
				min_err = mid_err;
			}
		}
		return (min_val + max_val) * 0.5f;
	}

	//uv offset
	float2 GetGerstnerOffsetProfile(float wave_coord, float gerstner_radius)
	{
		//returns x - wave_coord, where x satisfies: x + GetGerstnerDisplacementProfile(x) = wave_coord
		auto err_func = [wave_coord, gerstner_radius](float x) -> float
		{
			return x + GetGerstnerDisplacementProfile(x, gerstner_radius).x - wave_coord;
		};

		float eps = 1e-2f;

		float prev_val = 0.5f;
		float prev_err = err_func(prev_val);

		float dir = wave_coord < 0.5f ? -eps : eps;

		float curr_val = prev_val;
		for(curr_val = prev_val; curr_val > -1.0f && curr_val < 2.0f; curr_val += dir)
		{
			float curr_err = err_func(curr_val);
			if (curr_err * prev_err <= 0.0f)
				break;
			prev_val = curr_val;
			prev_err = curr_err;
		}

		float x = Bisect(err_func, std::min(prev_val, curr_val), std::max(prev_val, curr_val), 10);

		float2 displacement = GetGerstnerDisplacementProfile(x, gerstner_radius);
		return float2(-displacement.x, displacement.y);
	}

	void SmoothWaveProfileRow(TexUtil::Color128* color_row, size_t row_width, float blur_weight, size_t it_count)
	{
		std::vector<TexUtil::Color128> tmp;
		tmp.resize(row_width);
		for (size_t i = 0; i < it_count; i++)
		{
			for (size_t x = 0; x < row_width; x++)
			{
				tmp[x] = color_row[x];
				TexUtil::Color128 blurred_color = (
					color_row[(x - 1 + row_width) % row_width] +
					color_row[(x + row_width) % row_width] +
					color_row[(x + 1 + row_width) % row_width]) * (1.0f / 3.0f);
				tmp[x].r = Lerp(tmp[x].r, blurred_color.r, blur_weight);
				tmp[x].b = Lerp(tmp[x].b, blurred_color.b, blur_weight);
			}
			for (size_t x = 0; x < row_width; x++)
			{
				color_row[x] = tmp[x];
			}
		}
	}

	void BuildWaveProfileDerivatives(TexUtil::Color128* color_row, size_t row_width)
	{
		std::vector<TexUtil::Color128> derivatives;
		derivatives.resize(row_width);

		float step_size = 1.0f / row_width;
		for (size_t x = 0; x < row_width; x++)
		{
			derivatives[x] = (color_row[(x + 1) % row_width] + color_row[(x - 1 + row_width) % row_width] * -1.0f) * (1.0f / (2.0f * step_size));
		}
		for (size_t x = 0; x < row_width; x++)
		{
			color_row[x].g = derivatives[x].r;
			color_row[x].a = derivatives[x].b;
		}
	}

	void BuildWaveProfileTexture(Device::IDevice *device, std::wstring wave_texture)
	{
		int texture_width = 256;
		int texture_height = 256;
		auto profile_texture = Device::Texture::CreateTexture("Wave Profile", device, texture_width, texture_height, 1, Device::UsageHint::DYNAMIC, Device::PixelFormat::A32B32G32R32F, Device::Pool::DEFAULT, false, false, false);

		using namespace WaveUtil;
		using namespace TexUtil;

		Device::LockedRect rect;
		profile_texture->LockRect(0, &rect, Device::Lock::DEFAULT);
		LockedData<Color128> tidal_data(rect.pBits, rect.Pitch);

		float pi = 3.141592f;

		if (rect.pBits)
		{
			for (int y = 0; y < texture_height; y++)
			{
				float break_ratio = float(y + 0.5f) / float(texture_height);
				float gerstner_radius = std::clamp(break_ratio * 2.0f, 0.0f, 1.0f) / (2.0f * pi);
				float shore_ratio = std::clamp(break_ratio * 2.0f - 1.0f, 0.0f, 1.0f);

				float avg_height = 0.0f;
				for (int x = 0; x < texture_width; x++)
				{
					float wave_phase = float(x + 0.5f) / float(texture_width);

					float2 offset = GetGerstnerOffsetProfile(wave_phase, gerstner_radius);

					Color128 col;
					col.r = offset.x;
					col.g = 0.0f;
					col.b = offset.y;
					col.a = 0.0f;

					avg_height += offset.y / float(texture_width);
					tidal_data.Get(simd::vector2_int(x, y)) = col;
				}
				for (int x = 0; x < texture_width; x++)
				{
					tidal_data.Get(simd::vector2_int(x, y)).g -= avg_height;
				}
				SmoothWaveProfileRow(&tidal_data.Get(simd::vector2_int(0, y)), texture_width, shore_ratio, 500);
				BuildWaveProfileDerivatives(&tidal_data.Get(simd::vector2_int(0, y)), texture_width);
			}
		}
		profile_texture->UnlockRect(0);
		profile_texture->SaveToFile(wave_texture.c_str(), Device::ImageFileFormat::DDS);
	}

	void SmoothWaveProfileRowRG(float2* color_row, size_t row_width, float blur_weight, size_t it_count)
	{
		std::vector<float2> tmp;
		tmp.resize(row_width);
		for (size_t i = 0; i < it_count; i++)
		{
			for (size_t x = 0; x < row_width; x++)
			{
				tmp[x] = color_row[x];
				float2 blurred_color = (
					color_row[(x - 1 + row_width) % row_width] +
					color_row[(x + row_width) % row_width] +
					color_row[(x + 1 + row_width) % row_width]) * (1.0f / 3.0f);
				tmp[x] = Lerp(tmp[x], blurred_color, blur_weight);
			}
			for (size_t x = 0; x < row_width; x++)
			{
				color_row[x] = tmp[x];
			}
		}
	}

	void BuildWavePositiveProfileTexture(Device::IDevice* device, std::wstring wave_texture)
	{
		int texture_width = 256;
		int texture_height = 256;
		auto profile_texture = Device::Texture::CreateTexture("Wave Profile", device, texture_width, texture_height, 1, Device::UsageHint::DYNAMIC, Device::PixelFormat::G32R32F, Device::Pool::DEFAULT, false, false, false);

		using namespace WaveUtil;
		using namespace TexUtil;
		Device::LockedRect rect;
		profile_texture->LockRect(0, &rect, Device::Lock::DEFAULT);
		LockedData<float2> tidal_data(rect.pBits, rect.Pitch);

		float pi = 3.141592f;

		if (rect.pBits)
		{
			for (int y = 0; y < texture_height; y++)
			{
				float break_ratio = float(y + 0.5f) / float(texture_height);
				float gerstner_radius = std::clamp(break_ratio * 2.0f, 0.0f, 1.0f) / (2.0f * pi);
				float shore_ratio = std::clamp(break_ratio * 2.0f - 1.0f, 0.0f, 1.0f);

				float avg_height = 0.0f;
				for (int x = 0; x < texture_width; x++)
				{
					float wave_phase = float(x + 0.5f) / float(texture_width);

					float2 offset = GetGerstnerOffsetProfile(wave_phase - 0.5f, gerstner_radius);
					tidal_data.Get(simd::vector2_int(x, y)) = offset;
				}
				SmoothWaveProfileRowRG(&tidal_data.Get(simd::vector2_int(0, y)), texture_width, shore_ratio, 500);
				float zero_offset = (tidal_data.Get(simd::vector2_int(0, y)).y + tidal_data.Get(simd::vector2_int(texture_width - 1, y)).y) * 0.5f;
				for (int x = 0; x < texture_width; x++)
				{
					tidal_data.Get(simd::vector2_int(x, y)).y -= zero_offset;
				}
			}
		}
		profile_texture->UnlockRect(0);
		profile_texture->SaveToFile(wave_texture.c_str(), Device::ImageFileFormat::DDS);
	}



	float Fresnel(float VdotH)
	{
		return exp2((-5.55473f * VdotH - 6.98316f) * VdotH);
	}

	float saturate(float val)
	{
		return std::max(0.0f, std::min(1.0f, val));
	}

	float3 lerp(float3 a, float3 b, float param)
	{
		return a * (1.0f - param) + b * param;
	}
	
	float dot(float3 v0, float3 v1)
	{
		return v0.dot(v1);
	}

	float3 normalize(float3 v)
	{
		return v.normalize();
	}


	float GlossinessToRoughness(float glossiness)
	{
		return saturate(1.0f - glossiness);
	}

	float GeometryOcclusionSmithTerm(float alpha, float VdotN, float LdotN) // * VdotN * LdotN
	{
		//geometric term
		float k = alpha * 0.5f;
		float G_SmithI = (VdotN * (1.0f - k) + k);
		float G_SmithO = (LdotN * (1.0f - k) + k);
		return 1.0f / (G_SmithI * G_SmithO);
	}

	float GGXMicrofacetDistribution(float NdotH, float alpha2) // / pi
	{
		return alpha2 / pow((alpha2 - 1.0f) * NdotH * NdotH + 1.0f, 2.0f);
	}

	struct TBN
	{
		float3 tangent;
		float3 binormal;
		float3 normal;
	};
	float3 GenerateGGXImportanceSampleH(TBN tbn_basis , float roughness, float2 seed)
	{
		float pi = 3.1415f;
		float alpha = roughness * roughness;
		float phi = 2.0f * pi * seed.x;

		float cos_theta = sqrt((1.0f - seed.y) / (1.0f + (alpha * alpha - 1.0f) * seed.y));
		float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

		float3 local_dir;
		local_dir.x = sin_theta * cos(phi);
		local_dir.y = sin_theta * sin(phi);
		local_dir.z = cos_theta;

		return tbn_basis.tangent * local_dir.x + tbn_basis.binormal * local_dir.y + tbn_basis.normal * local_dir.z;
	}

	/*float2 HammersleyNorm(int i, int N) 
	{
		// principle: reverse bit sequence of i
		using uint = uint32_t;
		uint b = (uint(i) << 16u) | (uint(i) >> 16u);
		b = (b & 0x55555555u) << 1u | (b & 0xAAAAAAAAu) >> 1u;
		b = (b & 0x33333333u) << 2u | (b & 0xCCCCCCCCu) >> 2u;
		b = (b & 0x0F0F0F0Fu) << 4u | (b & 0xF0F0F0F0u) >> 4u;
		b = (b & 0x00FF00FFu) << 8u | (b & 0xFF00FF00u) >> 8u;

		return float2(i, b) / float2(N, 0xffffffffU);
	}*/

	simd::vector2 GetEnvironmentGGXCoeffs(float VdotN, float glossiness)
	{
		TBN tbn_basis;
		tbn_basis.tangent = { 1.0f, 0.0f, 0.0f };
		tbn_basis.binormal = { 0.0f, 1.0f, 0.0f };
		tbn_basis.normal = { 0.0f, 0.0f, -1.0f };
		float3 V = { sqrt(1.0f - VdotN * VdotN), 0.0f, -VdotN };
		float3 N = tbn_basis.normal;

		float roughness = GlossinessToRoughness(glossiness);
		float alpha = roughness * roughness;

		float pi = 3.1415f;

		float2 sum_brdf_contribution = 0.0f;
		float sum_weight = 1e-7f;

		int samples_count = 1024;
		for (int i = 0; i < samples_count; i++)
		{
			float2 sample_seed = HammersleyNorm(i, samples_count);// { Random(0.0f, 1.0f), Random(0.0f, 1.0f) };
			float3 H = GenerateGGXImportanceSampleH(tbn_basis, roughness, sample_seed);
			float3 L = H * dot(V, H) * 2.0f - V;

			float NdotV = saturate(dot(N, V));
			float NdotL = saturate(dot(N, L));
			float NdotH = saturate(dot(N, H));
			float VdotH = saturate(dot(V, H));

			float G = GeometryOcclusionSmithTerm(alpha, NdotV, NdotL) * (/*NdotV * */NdotL);
			//float3 F = specular_color + (1.0f - specular_color) * Fresnel(VdotH);
			//float3 F = specular_color * (1.0f - Fresnel(VdotH)) + Fresnel(VdotH);
			float fresnel = Fresnel(VdotH);
			float2 F = { (1.0f - fresnel), fresnel };
			float2 brdf_contribution = 0;
			if (NdotL > 0)
			{
				brdf_contribution = F * G * VdotH / (/*NdotV * */ NdotH);
			}

			// Incident light = SampleColor * NoL
			// Microfacet specular = D*G*F / (4*NoL*NoV)
			// pdf = D * NoH / (4 * VoH)
			sum_brdf_contribution = sum_brdf_contribution + brdf_contribution;

			sum_weight += 1.0f;
		}

		return sum_brdf_contribution * (1.0f / sum_weight);
	}
	void BuildEnvironmentGGXTexture(Device::IDevice *device, std::wstring filename)
	{
		int texture_width = 64;
		int texture_height = 64;
		auto ggx_texture = Device::Texture::CreateTexture("Env GGX", device, texture_width, texture_height, 1, Device::UsageHint::DYNAMIC, Device::PixelFormat::G16R16F, Device::Pool::DEFAULT, false, false, false);

		Device::LockedRect gradient_rect;
		ggx_texture->LockRect(0, &gradient_rect, Device::Lock::DEFAULT);
		TexUtil::LockedData<simd::vector2_half> gradient_data(gradient_rect.pBits, gradient_rect.Pitch);

		if (gradient_rect.pBits)
		{
			for (int y = 0; y < texture_height; y++)
			{
				for (int x = 0; x < texture_width; x++)
				{
					float VdotN = float(x + 0.5f) / float(texture_width);
					float glossiness = float(y + 0.5f) / float(texture_height);
					simd::vector2 brdf_coeffs = GetEnvironmentGGXCoeffs(VdotN, glossiness);
					gradient_data.Get({ x, y }) = simd::vector2_half(brdf_coeffs.y, brdf_coeffs.x); //format is GR, not RG
				}
			}
		}
		ggx_texture->UnlockRect(0);
		ggx_texture->SaveToFile(filename.c_str(), Device::ImageFileFormat::DDS);
	}


	simd::vector3 RandomPointOnSphere()
	{
		while (1)
		{
			simd::vector3 point = simd::vector3(Random(-1.0f, 1.0f), Random(-1.0f, 1.0f), Random(-1.0f, 1.0f));
			if (point.len() < 1.0f)
				return point.normalize();
		}
		return simd::vector3(0.0f, 0.0f, 0.0f); //unreachable
	}

	struct InfiniteCone
	{
		InfiniteCone(simd::vector3 point, simd::vector3 axis, float ang)
		{
			this->point = point;
			this->axis = axis;
			this->planar_norm = simd::vector2(cos(ang), -sin(ang));
		}
		simd::vector3 point;
		simd::vector3 axis;
		simd::vector2 planar_norm;

		bool IsPointInside(simd::vector3 test_point)
		{
			simd::vector3 delta = test_point - point;
			simd::vector3 plane_norm = GetLocalPlaneNorm(test_point);
			return delta.dot(plane_norm) < 0.0f;
		}
		simd::vector3 GetLocalPlaneNorm(simd::vector3 test_point)
		{
			simd::vector3 delta = test_point - point;

			simd::vector3 x_vector = -delta.cross(axis).cross(axis);
			if (x_vector.len() > 1e-7f)
				x_vector.normalize();
			else
				x_vector = simd::vector3(0.0f, 1.0f, 0.0f);
			simd::vector3 y_vector = axis;
			return x_vector * planar_norm.x + y_vector * planar_norm.y;
		}
	};
	struct ScatterCounts
	{
		float medium[4];
		float avg_scatter_count;
		float avg_dist;
		//float subsurface[2];
		//float emission;
	};
	float GetFresnel(float NdotVi, float ni, float nt)
	{
		float t = 1.0f - ni * ni / (nt * nt) * (1.0f - NdotVi * NdotVi);
		if (t < 0)
			return 1.0f;
		float NdotVt = sqrt(t);
		float r_per = (ni * NdotVi - nt * NdotVt) / (ni * NdotVi + nt * NdotVt);
		float r_par = (nt * NdotVi - ni * NdotVt) / (nt * NdotVi + ni * NdotVt);
		return (r_per * r_per + r_par * r_par) / 2.0f;
	}

	ScatterCounts PathTraceSubsurfaceScattering(float ang, float scatter_density)
	{
		int rays_count = 10000;
		int steps_count = 1000;
		float sum_scattering = 0.0f;

		std::array<int, 4> sum_medium_scatter_count;
		//std::array<int, 2> sum_subsurface_scatter_count;
		std::fill(sum_medium_scatter_count.begin(), sum_medium_scatter_count.end(), 0);
		//std::fill(sum_subsurface_scatter_count.begin(), sum_subsurface_scatter_count.end(), 0);
		int total_count = 0;
		float total_dist = 0.0f;

		float radius = 1.0f;
		for (int ray_index = 0; ray_index < rays_count; ray_index++)
		{
			float step_size = 1e-2f;
			simd::vector3 pos = { -radius, 0.0f, 0.0f };
			simd::vector3 dir = { cos(ang), sin(ang), 0.0f };

			int medium_scatter_count = 0;
			int subsurface_scatter_count = 0;
			int emission_scatter_count = 0;
			for (int i = 0; i < steps_count; i++)
			{
				pos += dir * step_size;
				total_dist += step_size;
				if (pos.len() > radius)
					break;

				float scatter_probability = exp(-step_size * scatter_density);
				if (Random(0.0f, 1.0f) > scatter_probability)
				{
					dir = RandomPointOnSphere();
					medium_scatter_count++;
				}

				/*std::string msg;
				for (float NdotV = 0.0f; NdotV < 1.0f; NdotV += 0.01f)
				{
					msg += "NdotV: " + std::to_string(NdotV) + " f : " + std::to_string(GetFresnel(NdotV, 1.0f, 1.33f)) + "\n";
				}
				OutputDebugStringA(msg.c_str());

				return ScatterCounts();*/
				if ((pos + dir * step_size).len() > radius)
				{
					simd::vector3 norm = pos.normalize();
					float fresnel = GetFresnel(norm.dot(dir), 1.33f, 1.0f);

					if (Random(0.0f, 1.0f) < fresnel)
					{
						dir -= norm * norm.dot(dir) * 2.0f;
					}
				}
			}
			sum_medium_scatter_count[std::min(medium_scatter_count, 3)]++;
			total_count += medium_scatter_count;
			//sum_subsurface_scatter_count[std::min(subsurface_scatter_count, 1)]++;
		}
		ScatterCounts res;
		res.avg_scatter_count = float(total_count) / float(rays_count);
		res.avg_dist = float(total_dist) / rays_count;
		for (int i = 0; i < 4; i++)
		{
			res.medium[i] = float(sum_medium_scatter_count[i]) / float(rays_count);
		}
		/*for (int i = 0; i < 2; i++)
		{
			res.subsurface[i] = float(sum_subsurface_scatter_count[i]) / float(rays_count);
		}*/
		return res;
	}

	void BuildScatteringLookupTexture(Device::IDevice *device, std::wstring filename)
	{
		int texture_width = 64;
		int texture_height = 64;
		auto scattering_texture = Device::Texture::CreateTexture("Scattering", device, texture_width, texture_height, 1, Device::UsageHint::DYNAMIC, Device::PixelFormat::A32B32G32R32F, Device::Pool::DEFAULT, false, false, false);

		Device::LockedRect lookup_rect;
		scattering_texture->LockRect(0, &lookup_rect, Device::Lock::DEFAULT);
		TexUtil::LockedData<simd::vector4> lookup_data(lookup_rect.pBits, lookup_rect.Pitch);

		if (lookup_rect.pBits)
		{
			for (int y = 0; y < texture_height; y++)
			{
				for (int x = 0; x < texture_width; x++)
				{
					float angle_scale = float(x + 0.5f) / float(texture_width);
					float free_run_scale = float(y + 0.5f) / float(texture_height);

					float cos_ang = angle_scale;
					float ang = acos(cos_ang);
					//float subsurface_dist = 1.0f;
					float free_run_dist = 1.0f / free_run_scale - 1.0f;

					ScatterCounts scatter_counts = PathTraceSubsurfaceScattering(ang, 1.0f / free_run_dist);
					lookup_data.Get({ x, y }) = simd::vector4(scatter_counts.avg_scatter_count, scatter_counts.avg_dist, 0.0f, 1.0f);
					//lookup_data.Get({ x, y }) = simd::vector4(scatter_counts.subsurface[1], 0*scatter_counts.medium[0], 0*scatter_counts.medium[1], 0*scatter_counts.medium[2]);
				}
			}
		}
		scattering_texture->UnlockRect(0);
		scattering_texture->SaveToFile(filename.c_str(), Device::ImageFileFormat::DDS);
	}
}
