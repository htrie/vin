#pragma once

template<typename T>
class AABB3
{
private:
  bool empty;
public:
  AABB3()
  {
    empty = 1;
  }
  AABB3(const Vector3<T> &_box_point1, const Vector3<T> &_box_point2)
  {
    empty = 1;
    Set(_box_point1, _box_point2);
  }
  bool Intersects(const AABB3<T> &aabb) const
  {
    if(empty || aabb.empty) return 0;
    /*float min1 =      box_point1.x;
    float max1 =      box_point2.x;
    float min2 = aabb.box_point1.x;
    float max2 = aabb.box_point2.x;*/
    if((box_point1.x > aabb.box_point2.x) || (aabb.box_point1.x > box_point2.x)) return 0;
    if((box_point1.y > aabb.box_point2.y) || (aabb.box_point1.y > box_point2.y)) return 0;
    if((box_point1.z > aabb.box_point2.z) || (aabb.box_point1.z > box_point2.z)) return 0;
    return 1;
  }
  template<bool isFiniteCast>
  bool Intersects(Vector3<T> rayStart, Vector3<T> rayDir, float &paramMin, float &paramMax)
  {
    // r.dir is unit direction vector of ray
    Vector3f invDir(T(1.0) / rayDir.x, T(1.0) / rayDir.y, T(1.0) / rayDir.z);

    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
    float t1 = (box_point1.x - rayStart.x) * invDir.x;
    float t2 = (box_point2.x - rayStart.x) * invDir.x;
    float t3 = (box_point1.y - rayStart.y) * invDir.y;
    float t4 = (box_point2.y - rayStart.y) * invDir.y;
    float t5 = (box_point1.z - rayStart.z) * invDir.z;
    float t6 = (box_point2.z - rayStart.z) * invDir.z;

    paramMin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    paramMax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
    if (isFiniteCast && paramMax < 0)
    {
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (paramMin > paramMax)
    {
        return false;
    }
    return true;
  }
  bool Includes(const Vector3<T> &point) const
  {
    if(empty) return 0;
    if ((point.x < box_point1.x) || (point.x > box_point2.x) ||
      (point.y < box_point1.y) || (point.y > box_point2.y) ||
      (point.z < box_point1.z) || (point.z > box_point2.z))
    {
      return 0;
    }
    return 1;
  }
	bool Includes(const AABB3<T> &aabb) const
	{
    return Includes(aabb.box_point1) && Includes(aabb.box_point2);
	}

  void Set(const Vector3<T> &_box_point1, const Vector3<T> &_box_point2)
  {
    empty = 0;
    box_point1 = _box_point1;
    box_point2 = _box_point2;
  }
  void Reset()
  {
    empty = 1;
    box_point1 = Vector3<T>::zeroVector();
    box_point2 = Vector3<T>::zeroVector();
  }
  void Expand(const Vector3<T> &additionalPoint)
  {
    if(empty)
    {
      empty = 0;
      box_point1 = additionalPoint;
      box_point2 = additionalPoint;
    }else
    {
      box_point1.x = Min(box_point1.x, additionalPoint.x);
      box_point1.y = Min(box_point1.y, additionalPoint.y);
      box_point1.z = Min(box_point1.z, additionalPoint.z);

      box_point2.x = Max(box_point2.x, additionalPoint.x);
      box_point2.y = Max(box_point2.y, additionalPoint.y);
      box_point2.z = Max(box_point2.z, additionalPoint.z);
    }
  }
  void Expand(const AABB3<T> &internalAABB)
  {
    Expand(internalAABB.box_point1);
    Expand(internalAABB.box_point2);
  }
  void Expand(const T mult)
  {
    Vector3<T> size = box_point2 - box_point1;
    box_point1 -= size * (mult - T(1.0)) * T(0.5);
    box_point2 += size * (mult - T(1.0)) * T(0.5);
  }
  const T Square()
  {
    Vector3<T> size = box_point2 - box_point1;
    return (size.x * size.y + size.x * size.z + size.y * size.z) * T(2.0);
  }
  Vector3<T> GetSize()
  {
    return box_point2 - box_point1;
  }

  Vector3<T> box_point1, box_point2;
};

typedef AABB3<float>	AABB3f;
typedef AABB3<double>	AABB3d;