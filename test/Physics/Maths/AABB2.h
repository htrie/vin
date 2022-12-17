#pragma once

template<typename T>
class AABB2
{
private:
  bool empty;
public:
  AABB2()
  {
    empty = true;
  }
  AABB2(const Vector2<T> &_box_point1, const Vector2<T> &_box_point2)
  {
    empty = true;
    Set(_box_point1, _box_point2);
  }

  bool Intersects(const AABB2<T> &aabb) const
  {
    if(empty || aabb.empty) return 0;
    /*float min1 =      box_point1.x;
    float max1 =      box_point2.x;
    float min2 = aabb.box_point1.x;
    float max2 = aabb.box_point2.x;*/
    if((box_point1.x > aabb.box_point2.x) || (aabb.box_point1.x > box_point2.x)) return 0;
    if((box_point1.y > aabb.box_point2.y) || (aabb.box_point1.y > box_point2.y)) return 0;
    return 1;
  }

  template<bool isFiniteCast>
  bool Intersects(Vector2<T> rayStart, Vector2<T> rayDir, float &paramMin, float &paramMax)
  {
    // r.dir is unit direction vector of ray
    Vector2f invDir(1.0f / rayDir.x, 1.0f / rayDir.y);

    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
    float t1 = (box_point1.x - rayStart.x) * invDir.x;
    float t2 = (box_point2.x - rayStart.x) * invDir.x;
    float t3 = (box_point1.y - rayStart.y) * invDir.y;
    float t4 = (box_point2.y - rayStart.y) * invDir.y;

    paramMin = std::max(std::min(t1, t2), std::min(t3, t4));
    paramMax = std::min(std::max(t1, t2), std::max(t3, t4));

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
  bool Includes(const Vector2<T> &point) const
  {
    if(empty) return 0;
    if ((point.x < box_point1.x) || (point.x > box_point2.x) ||
      (point.y < box_point1.y) || (point.y > box_point2.y))
    {
      return 0;
    }
    return 1;
  }
  bool Includes(const AABB2<T> &aabb) const
  {
    return Includes(aabb.box_point1) && Includes(aabb.box_point2);
  }
  void Set(const Vector2<T> &_box_point1, const Vector2<T> &_box_point2)
  {
    empty = 0;
    box_point1 = _box_point1;
    box_point2 = _box_point2;
  }
  void Reset()
  {
    empty = 1;
    box_point1 = Vector2<T>::zeroVector();
    box_point2 = Vector2<T>::zeroVector();
  }
  void Expand(const Vector2<T> &additionalPoint)
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

      box_point2.x = Max(box_point2.x, additionalPoint.x);
      box_point2.y = Max(box_point2.y, additionalPoint.y);
    }
  }
  void Expand(const AABB2<T> &internalAABB)
  {
    Expand(internalAABB.box_point1);
    Expand(internalAABB.box_point2);
  }
  void Expand(const T mult)
  {
    Vector2<T> size = box_point2 - box_point1;
    box_point1 -= size * (mult - T(1.0)) * T(0.5);
    box_point2 += size * (mult - T(1.0)) * T(0.5);
  }
  const T Square() //perimeter actually, used for AABBTree balancing
  {
    Vector2<T> size = box_point2 - box_point1;
    return (size.x + size.y) * T(2.0);
  }
  Vector2<T> GetSize()
  {
    return box_point2 - box_point1;
  }
  Vector2<T> box_point1, box_point2;
};

typedef AABB2<float>	AABB2f;
typedef AABB2<double>	AABB2d;

