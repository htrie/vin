#pragma once

template<typename T>
class Box
{
public:
	Box()
	{
		coords = Coords3f::defCoords();
		half_size = Vector3<T>(T(1.0), T(1.0), T(1.0));
	}
	Vector3<T> GetSupportPoint(Vector3<T> v)
	{
		return Vector3<T>(42.0f, 42.0f, 42.0f); //DEBUG
	}
	RayIntersection<T> GetIntersection(Ray<T> ray)
	{
		RayIntersection<T> res;
		res.min_scale = 0.0f; //DEBUG
		res.max_scale = 42.0f;
		res.exists = true;
		return res;
	}

	PointLocation<T> GetClosestSurfacePointLocal(Vector3<T> localPoint)
	{
		PointLocation<T> res;
		res.point = Vector3f(42.0f, 42.0f, 42.0f);
		res.normal = Vector3f(1.0f, 0.0f, 0.0f);
		res.isInner = (abs(localPoint.x) < half_size.x) && (abs(localPoint.y) < half_size.y) && (abs(localPoint.z) < half_size.z);

		if (!res.isInner)
			return res;
		
		struct SurfacePoint
		{
			Vector3f point;
			Vector3f normal;
			float depth;
		};
		SurfacePoint points[3];
		points[0].normal = Vector3f(sgn(localPoint.x), 0.0f, 0.0f);
		points[0].point = Vector3f(half_size.x * sgn(localPoint.x), localPoint.y, localPoint.z);
		points[0].depth = abs(points[0].point.x - localPoint.x);

		points[1].normal = Vector3f(0.0f, sgn(localPoint.y), 0.0f);
		points[1].point = Vector3f(localPoint.x, half_size.y * sgn(localPoint.y), localPoint.z);
		points[1].depth = abs(points[1].point.y - localPoint.y);

		points[2].normal = Vector3f(0.0f, 0.0f, sgn(localPoint.z));
		points[2].point = Vector3f(localPoint.x, localPoint.y, half_size.z * sgn(localPoint.z));
		points[2].depth = abs(points[2].point.z - localPoint.z);

		SurfacePoint resPoint = points[0];
		for (int i = 1; i < 3; i++)
		{
			if (resPoint.depth > points[i].depth)
				resPoint = points[i];
		}

		res.point = resPoint.point;
		res.normal = resPoint.normal;
		return res;
	}
	PointLocation<T> GetClosestSurfacePoint(Vector3<T> worldPoint)
	{
		Vector3f localPoint = coords.GetLocalPoint(worldPoint);
		PointLocation<T> localRes = GetClosestSurfacePointLocal(localPoint);
		PointLocation<T> res = localRes;

		res.point = coords.GetWorldPoint(res.point);
		res.normal = coords.GetWorldVector(res.normal);

		return res;
	}

	Coords3f coords;
	Vector3f half_size;
};
