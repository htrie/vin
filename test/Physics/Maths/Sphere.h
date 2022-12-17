#pragma once

template<typename T>
struct Ray
{
	Vector3<T> origin;
	Vector3<T> dir;
};

template<typename T>
struct PointLocation
{
	Vector3<T> point;
	Vector3<T> normal;
	bool isInner;
};

template<typename T>
struct RayIntersection
{
	T min_scale, max_scale;
	bool exists;
};

template<typename T>
class Sphere
{
public:
	RayIntersection<T> GetRayIntersection(Ray<T> ray)
	{
		RayIntersection<T> res;
		T scale = -T(1.0);

		T a = ray.dir * ray.dir;
		Vector3<T> delta = ray.origin - coords.pos;
		T b = T(2.0) * (delta * ray.dir);
		T c = (coords.pos * coords.pos) + (ray.origin * ray.origin) - T(2.0) * (ray.origin * coords.pos) - radius * radius;
		T disc = b * b - T(4.0) * a * c;
		if (disc > 0)
		{
			T sqrtDisc = sqrt(disc);

			scale = (-b - sqrtDisc) / (T(2.0) * a);
			//isInnerIntersection = false;
			res.min_scale = (-b - sqrtDisc) / (T(2.0) * a);
			res.max_scale = (-b + sqrtDisc) / (T(2.0) * a);
			res.exists = true;
		}
		else
		{
			res.exists = false;
		}
		return res;
	}

	PointLocation<T> GetClosestSurfacePoint(Vector3<T> point)
	{
		PointLocation<T> res;
		Vector3<T> delta = (point - coords.pos);
		T len = delta.Len();
		res.isInner = len < radius;
		res.normal = delta * (1.0f / len);
		res.point = coords.pos + res.normal * radius;
		return res;
	}
	Coords3<T> coords;
	T radius;
};
