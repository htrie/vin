#pragma once
template<typename T>
Vector3<T> VecMul(Vector3<T> v0, Vector3<T> v1)
{
	return Vector3<T>(v0.x * v1.x, v0.y * v1.y, v0.z * v1.z);
}

template<typename T>
class Ellipsoid
{
public:
	Ellipsoid()
	{
		coords = Coords3<T>::defCoords();
		SetRadii(Vector3<T>(T(1.0), T(1.0), T(1.0)));
	}
	void SetRadii(Vector3<T> radii)
	{
		this->radii = radii;
		this->invRadii = radii.GetInverse();
	}
	Vector3<T> GetRadii()
	{
		return radii;
	}
	Vector3<T> GetSupportPoint(Vector3<T> v)
	{
		Vector3<T> localVec = coords.GetLocalVector(v);
		Vector3<T> normalSpaceVec = localVec & radii;
		Vector3<T> normalSpacePoint = normalSpaceVec.GetNorm();
		return normalSpacePoint & radii;
	}
	RayIntersection<T> GetRayIntersectionLocal(Ray<T> localRay)
	{
		RayIntersection<T> res;

		Ray<T> normalRay;
		normalRay.origin = localRay.origin & invRadii;
		normalRay.dir = localRay.dir & invRadii;

		Sphere<T> sphere;
		sphere.coords = Coords3f::defCoords();
		sphere.radius = T(1.0f);
		RayIntersection<T> sphereInt = sphere.GetRayIntersection(normalRay);

		res.exists = sphereInt.exists;
		if (res.exists)
		{
			res.min_scale = sphereInt.min_scale;
			res.max_scale = sphereInt.max_scale;
		}
		return res;
	}
	RayIntersection<T> GetRayIntersection(Ray<T> ray)
	{
		RayIntersection<T> res;

		Ray<T> localRay;
		localRay.origin = this->coords.GetLocalPoint(ray.origin);
		localRay.dir = this->coords.GetLocalVector(ray.dir);
		RayIntersection<T> localRes = GetRayIntersectionLocal(localRay);

		res = localRes;
		return res;
	}
	T GetImplicitFunctionLocal(Vector3<T> localPoint)
	{
		return (localPoint & invRadii).SquareLen() - T(1.0);
	}
	T GetImplicitFunction(Vector3<T> point)
	{
		return GetImplicitFunctionLocal(coords.GetLocalPoint(point));
	}
	Vector3<T> GetSurfaceGradientLocal(Vector3<T> localPoint)
	{
		return (localPoint & (invRadii & invRadii)) * T(2.0);
	}
	Vector3<T> GetSurfaceGradient(Vector3<T> point)
	{
		return coords.GetWorldVector(GetSurfaceGradientLocal(coords.GetLocalPoint(point)));
	}

	T GetApproximateDistanceNonprecise(Vector3<T> point)
	{
		Vector3<T> localPoint = coords.GetLocalPoint(point);
		Vector3<T> invradii = VecInv(this->radii);
		T minRad = this->radii.x;
		if (minRad > this->radii.y)
			minRad = this->radii.y;
		if (minRad > this->radii.z)
			minRad = this->radii.z;
		return ((localPoint & invradii).Length() - T(1.0)) * (minRad);
	}

	PointLocation<T> GetClosestSurfacePointLocal(Vector3<T> localPoint, int iterationsCount = 5)
	{
		PointLocation<T> localRes;
		Vector3<T> currLocalPoint = localPoint;
		if (fabs(localPoint.x) > this->radii.x ||
		    fabs(localPoint.y) > this->radii.y ||
		    fabs(localPoint.z) > this->radii.z)
		{
			localRes.isInner = false;
			return localRes;
		}
		Vector3<T> currLocalDir = GetSurfaceGradientLocal(currLocalPoint);
		localRes.isInner = GetImplicitFunctionLocal(currLocalPoint) < 0.0f;
		for (int iteration = 0; iteration < iterationsCount; iteration++)
		{
			Ray<T> localRay;
			localRay.origin = localPoint;
			localRay.dir = currLocalDir;

			RayIntersection<T> localIntersection = GetRayIntersectionLocal(localRay);
			if (!localIntersection.exists) //this is not supposed to happen but may due to numerical instabilities
			{
				localRes.isInner = false;
				return localRes;
			}
			localRes.isInner = localIntersection.max_scale > 0.0f;
			Vector3<T> newLocalPoint = localRay.origin + localRay.dir * localIntersection.max_scale;
			Vector3<T> newLocalDir = GetSurfaceGradientLocal(newLocalPoint);// .GetNorm();


			currLocalDir = newLocalDir;
			currLocalPoint = newLocalPoint;
		}
		localRes.point = currLocalPoint;
		localRes.normal = currLocalDir.GetNorm();
		return localRes;
	}
	PointLocation<T> GetClosestSurfacePoint(Vector3<T> point, int iterationsCount = 5)
	{
		Vector3f localPoint = coords.GetLocalPoint(point);
		PointLocation<T> localRes = GetClosestSurfacePointLocal(localPoint, iterationsCount);
		PointLocation<T> res = localRes;

		res.point = coords.GetWorldPoint(res.point);
		res.normal = coords.GetWorldVector(res.normal);
		return res;
	}

	Coords3<T> coords;
private:
	Vector3<T> radii;
	Vector3<T> invRadii;
};
