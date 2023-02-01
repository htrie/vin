#pragma once

template<typename T>
class Capsule
{
public:
	RayIntersection<T> GetRayIntersection(Ray<T> ray)
	{
		Vector3<T> l = (spheres[1].coords.pos - spheres[0].coords.pos);
		T len = l.Len() + 1e-7f;
		Vector3<T> n = l / len;

		Vector3<T> p0 = spheres[0].coords.pos;
		Vector3<T> p1 = spheres[1].coords.pos;
		T r0 = spheres[0].radius;
		T r1 = spheres[1].radius;

		float sina = (r1 - r0) / (len + 1e-7f);
		float cosa = std::sqrt(std::max(0.0f, 1.0f - sina * sina));
		float tga = sina / (cosa + 1e-7f);

		T a = T(0.0);
		T b = T(0.0);
		T c = T(0.0);
		AddQuadratic(ray.origin - p0, ray.dir, T(-1.0), a, b, c);
		AddQuadratic((ray.origin - p0) * n, ray.dir * n, T(1.0), a, b, c);
		AddQuadratic(r0 / cosa + ((ray.origin - p0) * n) * tga, (ray.dir * n) * tga, T(1.0), a, b, c);

		RayIntersection<T> res;
		res.exists = false;
		if (SolveQuadratic(a, b, c, res.min_scale, res.max_scale))
		{
			Vector3<T> minPoint = ray.origin + ray.dir * res.min_scale;
			Vector3<T> maxPoint = ray.origin + ray.dir * res.max_scale;

			int count = 2;
			if ((minPoint - p0) * n < -r0 * sina || (minPoint - p1) * n > -r1 * sina)
			{
				count--;
				res.min_scale = res.max_scale;
			}
			if ((maxPoint - p0) * n < -r0 * sina || (maxPoint - p1) * n > -r1 * sina)
			{
				count--;
				res.max_scale = res.min_scale;
			}
			res.exists = count > 0;
		}
		//std::tie(res.min_scale, res.max_scale) = res.exists ? std::make_tuple(res.min_scale, res.max_scale) : std::make_tuple(T(1e7), T(-1e7));

		for (int sphereNumber = 0; sphereNumber < 2; sphereNumber++)
		{
			RayIntersection<T> sphereIntersection = spheres[sphereNumber].GetRayIntersection(ray);
			if (sphereIntersection.exists)
			{
				if (!res.exists)
					res = sphereIntersection;
				res.min_scale = std::min(res.min_scale, sphereIntersection.min_scale);
				res.max_scale = std::max(res.max_scale, sphereIntersection.max_scale);
			}
		}

		res.exists = res.exists && res.min_scale < res.max_scale;

		return res;
	}

	PointLocation<T> GetClosestSurfacePoint(Vector3<T> point)
	{
		Coords3f localCoords;
		localCoords.pos = spheres[0].coords.pos;
		localCoords.xVector = (spheres[1].coords.pos - spheres[0].coords.pos).GetNorm();
		localCoords.zVector = ((point - spheres[0].coords.pos) ^ localCoords.xVector).GetNorm();
		localCoords.yVector = localCoords.xVector ^ localCoords.zVector;

		float len = (spheres[1].coords.pos - spheres[0].coords.pos).Len();
		Vector3f localPlaneNorm;
		localPlaneNorm.x = (spheres[0].radius - spheres[1].radius) / (len + 1e-7f);
		localPlaneNorm.y = std::sqrt(std::max(0.0f, 1.0f - localPlaneNorm.x * localPlaneNorm.x));
		localPlaneNorm.z = 0.0f;
		Vector3f localPlanePoints[2];
		localPlanePoints[0] = localPlaneNorm * spheres[0].radius;
		localPlanePoints[1] = Vector3f(len, 0.0f, 0.0f) + localPlaneNorm * spheres[1].radius;

		Vector3f localPoint = localCoords.GetLocalPoint(point);

		float depth = (localPlanePoints[0] - localPoint) * localPlaneNorm;
		Vector3f localSurfacePoint = localPoint + localPlaneNorm * depth;

		float ratio = ((localPoint - localPlanePoints[0]) * (localPlanePoints[1] - localPlanePoints[0])) / ((localPlanePoints[1] - localPlanePoints[0]).SquareLen() + 1e-7f);

		PointLocation<T> res;
		res.point = localCoords.GetWorldPoint(localSurfacePoint);
		res.normal = localCoords.GetWorldVector(localPlaneNorm);

		if (ratio < 0.0f || ratio > 1.0f)
			depth = T(-1e7);

		for (int sphereNumber = 0; sphereNumber < 2; sphereNumber++)
		{
			PointLocation<T> sphereLocation = spheres[sphereNumber].GetClosestSurfacePoint(point);
			T sphereDepth = (sphereLocation.point - point) * sphereLocation.normal;
			if (sphereDepth > depth)
			{
				depth = sphereDepth;
				res = sphereLocation;
			}
		}
		res.isInner = depth > 0.0f;
		return res;
	}
	Sphere<T> spheres[2];
private:
	template<typename ValueType>
	void AddQuadratic(ValueType constParam, ValueType linearParam, T mult, T &squareCoeff, T &linearCoeff, T &constCoeff)
	{
		squareCoeff += linearParam * linearParam * mult;
		linearCoeff += (constParam * linearParam + linearParam * constParam) * mult;
		constCoeff  += constParam * constParam * mult;
	}
	bool SolveQuadratic(T squareCoeff, T linearCoeff, T constCoeff, T &root0, T &root1)
	{
		T disc = linearCoeff * linearCoeff - T(4.0) * squareCoeff * constCoeff;
		if (disc < 0)
			return false;
		root0 = (-linearCoeff - std::sqrt(disc)) / (T(2.0) * squareCoeff);
		root1 = (-linearCoeff + std::sqrt(disc)) / (T(2.0) * squareCoeff);
		std::tie(root0, root1) = ((root0 < root1) ? std::make_tuple(root0, root1) : std::make_tuple(root1, root0));
		return true;
	}
};
