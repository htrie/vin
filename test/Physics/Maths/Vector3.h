#pragma once

template<typename T>
struct Vector3
{
  T x, y, z;
  T & operator[](const int i)
  {
    return *(&(x) + i);
  }

  inline Vector3() {}
  //inline Vector3<T>(const Vector3<T> & rhs) { *this = rhs; }
  inline Vector3(const T &_x, const T &_y, const T &_z): x(_x), y(_y), z(_z) { }

  inline T Len() const
  {
    return sqrt(x * x + y * y + z * z);
  }

  inline T SquareLen() const
  {
    return x * x + y * y + z * z;
  }

  inline void Normalize()
  {
    T l = sqrt(x * x + y * y + z * z);
    if (fabs(l) > T(1e-8))
    {
      T k = T(1.0) / l;
      x *= k;
      y *= k;
      z *= k;
    }
  }

  inline Vector3 GetInverse()
  {
    return Vector3<T>(T(1.0) / x, T(1.0) / y, T(1.0) / z);
  }

  void  Rotate(const Vector3 &axis, const T sinAng, const T cosAng)
  {
    Vector3<T> self = *this;
    Vector3<T> x = self - axis * (axis * self);
    Vector3<T> y = x ^ axis;
    Vector3<T> delta = x * cosAng + y * sinAng - x;
    self += delta;
    *this = self;
  }

  void  Rotate(const Vector3 &axis, const T angle)
  {
    Rotate(axis, sin(angle), cos(angle));
  }

  void Rotate(const Vector3 &initialAxis, const Vector3 &dstAxis)
  {
    Vector3<T> axis = dstAxis ^ initialAxis;
    T sinAng = axis.Len();
    if(sinAng != 0) axis /= sinAng; //normalize
    T cosAng = initialAxis * dstAxis;
    Rotate(axis, sinAng, cosAng);
  }

  inline Vector3<T> GetNorm() const
  {
    T L = sqrt(x * x + y * y + z * z);
    if (fabs(L) > T(1e-8))
    {
      T k = T(1.0) / L;
      return Vector3(x * k, y * k, z * k);
    }else
    {
      return Vector3(0, 0, 0);
    }
  }

  inline Vector3<T> operator-() const
  {
    return Vector3(-x, -y, -z);
  }

  void Decrease(T val)
  {
    if(SquareLen() > val * val)
    {
      T len = Len();
      T scale = (len - val) / len;
      x *= scale;
      y *= scale;
      z *= scale;
    }else
    {
      x = 0.0f;
      y = 0.0f;
      z = 0.0f;
    }
  }

  inline Vector3<T> &operator *=(const T &i)			//mul number
  {
    x *= i;
    y *= i;
    z *= i;
    return *this;
  }

  inline Vector3<T> &operator /=(const T &i)			//div number
  {
    T inv = T(1.0) / i;
    x *= inv;
    y *= inv;
    z *= inv;
    return *this;
  }

  inline Vector3<T> &operator +=(const Vector3<T> &V)		//plus Vector3<T>
  {
    x += V.x;
    y += V.y;
    z += V.z;
    return *this;
  }

  inline Vector3<T> &operator-=(const Vector3<T> &V)		//plus Vector3<T>
  {
    x -= V.x;
    y -= V.y;
    z -= V.z;
    return *this;
  }

  inline Vector3<T> &operator--()
  {
    x = -x;
    y = -y;
    z = -z;
    return *this;
  }

  inline Vector3<T> operator+(const Vector3<T> &V) const
  {
    //return *((Vector3<T>*)&_mm_add_ps(_mm_load_ps(&x), _mm_load_ps(&V.x)));
    return Vector3(x + V.x,  y + V.y, z + V.z);
  }

  inline Vector3<T> operator-(const Vector3<T> &V) const
  {
    return Vector3(x - V.x,  y - V.y, z - V.z);
  }

  inline T operator*(const Vector3 &V) const
  {
    return x * V.x + y * V.y + z * V.z;
  }

  inline Vector3<T> operator &(const Vector3 &V) const
  {
	  return Vector3<T>(x * V.x, y * V.y, z * V.z);
  }

  inline T Dot(const Vector3 &v) const
  {
    return *this * v;
  }

  inline Vector3<T> operator^(const Vector3<T> &v) const
  {
      return Vector3<T>(this->y * v.z - v.y * this->z,
                        this->z * v.x - v.z * this->x, 
                        this->x * v.y - v.x * this->y);
  }

  inline Vector3<T> Cross(const Vector3<T> &v) const
  {
      return *this ^ v;
  }

  inline Vector3<T> operator*(const T &d) const
  {
    return Vector3<T>(x * d, y * d, z * d);
  }

  Vector3<T> GetPerpendicular() const
  {
    Vector3<T> leastPerpendicular;
    T bestProjection;
    T xProjection = fabs(x);
    T yProjection = fabs(y);
    T zProjection = fabs(z);
    if((xProjection <= yProjection + 1e-5) && (xProjection <= zProjection + 1e-5))
    {
      leastPerpendicular = Vector3<T>(1, 0, 0);
      bestProjection = xProjection;
    }
      else
    if((yProjection < xProjection + 1e-5) && (yProjection <= zProjection + 1e-5))
    {
      leastPerpendicular = Vector3<T>(0, 1, 0);
      bestProjection = yProjection;
    }
    else
    {
      leastPerpendicular = Vector3<T>(0, 0, 1);
      bestProjection = zProjection;
    }

    return ((*this) ^ leastPerpendicular).GetNorm();
  }

  static const Vector3<T> xAxis()
  {
    return Vector3<T>(T(1.0), 0, 0);
  }

  static const Vector3<T> yAxis()
  {
    return Vector3<T>(0, T(1.0), 0);
  }

  static const Vector3<T> zAxis()
  {
    return Vector3<T>(0, 0, T(1.0));
  }

  static const Vector3<T> zeroVector()
  {
    return Vector3<T>(0, 0, 0);
  }

  static const Vector3<T> zero()
  {
    return zeroVector();
  }

  static const Vector3<T> one()
  {
    return Vector3<T>(T(1.0), T(1.0), T(1.0));
  }
};

template<typename T>
inline Vector3<T> operator*(const T &d, const Vector3<T> &V)
{
  return Vector3<T>(V.x * d, V.y * d, V.z * d);
}

template<typename T>
inline Vector3<T> operator/(const Vector3<T> &V, const T &d)
{
  T invd = 0;
  if(fabs(d) > T(1e-8)) invd = 1.0f / d;
  return Vector3<T>(V.x * invd, V.y * invd, V.z * invd);
}

typedef Vector3<float>	Vector3f;
//typedef Vector3<double> Vector3d;

/*const static   Vector3d zeroVector3d  = Vector3d(0, 0, 0);
const static   Vector3d xAxis3d       = Vector3d(1, 0, 0);
const static   Vector3d yAxis3d       = Vector3d(0, 1, 0);
const static   Vector3d zAxis3d       = Vector3d(0, 0, 1);*/

const static   Vector3f zeroVector3f  = Vector3f(0, 0, 0);
const static   Vector3f xAxis3f       = Vector3f(1, 0, 0);
const static   Vector3f yAxis3f       = Vector3f(0, 1, 0);
const static   Vector3f zAxis3f       = Vector3f(0, 0, 1);

template<typename T>
bool ProjectPointToLine(const Vector3<T> &t1, const Vector3<T> &t2, const Vector3<T> &p, Vector3<T> &p0)
{
  Vector3<T> v1 = p - t1;
  Vector3<T> v2 = t2 - t1;
  p0 = t1 + v2 * ((v1 * v2) / v2.SquareLen());
  if((v1 * v2 >= 0.0) && ((v1 * v2) / (v2.SquareLen()) <= T(1.0)))
  {
    return 1;
  }else
  {
    return 0;
  }
}

template<typename T>
bool ProjectPointToTriangle(const Vector3<T> &p1, const Vector3<T> &p2, const Vector3<T> &p3, const Vector3<T> &point, Vector3<T> &projectionPoint)
{
  Vector3<T> n = (p2 - p1) ^ (p3 - p1);
  T mult = (p1 * n - point * n) / (n * n);
  projectionPoint = point + n * mult;

  return PointProjectionInsideTriangle(p1, p2, p3, projectionPoint);
};

template<typename T>
void ProjectPointToPlane(const Vector3<T> &point, const Vector3<T> &planePoint, const Vector3<T> &planeNormal, const Vector3<T> &projectionDirection, 
                                 Vector3<T> &projectedPoint)
{
  T mult = T(1.0) / (projectionDirection * planeNormal);
  projectedPoint = point + projectionDirection * ((planePoint * planeNormal) - (point * planeNormal)) * mult;
}

template<typename T>
bool TwoLinesDist(const Vector3<T> &p1,const Vector3<T> &p2,const Vector3<T> &t1,const Vector3<T> &t2, Vector3<T> &v, Vector3<T> &p)
{
  Vector3<T> v1 = p2 - p1;
  Vector3<T> v2 = t2 - t1;
  Vector3<T> v3 = v1 ^ v2;
  if(v3.SquareLen() > 1e-5f)
  {
    Vector3<T> norm = v3.GetNorm();
    T crossLen = norm * (p1 - t1);
    v = norm * crossLen;

    Vector3<T> crossPlaneNorm = (v2 ^ v3).GetNorm();

    Vector3<T> crosspoint;

    T height1 = (p1 - t1) * crossPlaneNorm;
    T height2 = (p2 - t1) * crossPlaneNorm;
    if(fabs(v1 * crossPlaneNorm) < 1e-4)
    {
      crosspoint = p1;
    }else
    {
      T scale = -(height1) / ((v1 * crossPlaneNorm));
      crosspoint = p1 + v1 * scale;
    }

    p = crosspoint - v;

    Vector3<T> test1 = (crosspoint - p1) ^ (p2 - p1);
    Vector3<T> test2 = (p - t1) ^ (t2 - t1);

    T scale1 = (crosspoint - p1) * v1 / v1.SquareLen();
    T scale2 = (p			- t1) * v2 / v2.SquareLen();
    return ((scale1 >= 0.0) && (scale1 <= 1.0) && (scale2 >= 0.0) && (scale2 <= 1.0));
  }else
  {
    return 0;
  }
}

template<typename T>
bool PointProjectionInsideTriangle(const Vector3<T> &p1, const Vector3<T> &p2, const Vector3<T> &p3, const Vector3<T> &point)
{
  Vector3<T> n = (p2 - p1) ^ (p3 - p1);

  Vector3<T> n1 = (p2 - p1) ^ n;
  Vector3<T> n2 = (p3 - p2) ^ n;
  Vector3<T> n3 = (p1 - p3) ^ n;

  T proj1 = (point - p2) * n1;
  T proj2 = (point - p3) * n2;
  T proj3 = (point - p1) * n3;

  if(proj1 > T(0.0))
    return 0;
  if(proj2 > T(0.0))
    return 0;
  if(proj3 > T(0.0))
    return 0;
  return 1;
}

template<typename T>
bool PointProjectionInsideTriangle(const Vector3<T> &p1, const Vector3<T> &p2, const Vector3<T> &p3, const Vector3<T> &point, const Vector3<T> &projectionDir, const T eps)
{
  T side1 = MixedProduct(p1 - point, p2 - point, projectionDir);
  T side2 = MixedProduct(p2 - point, p3 - point, projectionDir);
  T side3 = MixedProduct(p3 - point, p1 - point, projectionDir);

  if(side1 >  eps && side2 >  eps && side3 >  eps) return 1;
  if(side1 < -eps && side2 < -eps && side3 < -eps) return 1;
  return 0;
}

template<typename T>
T MixedProduct(const Vector3<T> &v0, const Vector3<T> &v1, const Vector3<T> &v2)
{
  return v0 * (v1 ^ v2);
}

template<typename GeomSpace>
bool PointInCell(const typename GeomSpace::Vector3 points[4], typename GeomSpace::Vector3 testPoint)
{
  typedef typename GeomSpace::Vector3 Vector3;
  typedef typename GeomSpace::Scalar  Scalar;
  Scalar eps = std::numeric_limits<typename GeomSpace::Scalar>::epsilon();//Scalar(1e-9);

  Scalar side0 = MixedProduct(points[1] - points[0], points[2] - points[0], testPoint - points[0]);
  Scalar side1 = MixedProduct(points[2] - points[0], points[3] - points[0], testPoint - points[0]);
  Scalar side2 = MixedProduct(points[3] - points[0], points[1] - points[0], testPoint - points[0]);
  Scalar side3 = MixedProduct(points[3] - points[1], points[2] - points[1], testPoint - points[1]);

  //printf("s0: %f s1: %f s2: %f s3: %f\n", side0, side1, side2, side3);

  if (side0 >= -eps && side1 >= -eps && side2 >= -eps && side3 >= -eps) return 1;
  if (side0 <=  eps && side1 <=  eps && side2 <=  eps && side3 <=  eps) return 1;
  return 0;

  /*Scalar eps = std::numeric_limits<float>::epsilon();//Scalar(1e-9);
  Scalar side012 =  mixed_product(points[1] - points[0], points[2] - points[0], testPoint - points[0]) *
                    mixed_product(points[1] - points[0], points[2] - points[0], points[3] - points[0]);
  if(side012 < -eps) return 0;

  Scalar side123 =  mixed_product(points[1] - points[2], points[3] - points[2], testPoint - points[2]) *
                    mixed_product(points[1] - points[2], points[3] - points[2], points[0] - points[2]);
  if(side123 < -eps) return 0;

  Scalar side230 =  mixed_product(points[2] - points[3], points[0] - points[3], testPoint - points[3]) *
                    mixed_product(points[2] - points[3], points[0] - points[3], points[1] - points[3]);
  if(side230 < -eps) return 0;

  Scalar side013 =  mixed_product(points[0] - points[1], points[3] - points[1], testPoint - points[1]) *
                    mixed_product(points[0] - points[1], points[3] - points[1], points[2] - points[1]);
  if(side013 < -eps) return 0;

  return 1;*/


  /*Scalar side1 = mixed_product(points[2] - points[0], points[3] - points[0], testPoint - points[0]);
  Scalar side2 = mixed_product(points[3] - points[0], points[1] - points[0], testPoint - points[0]);
  Scalar side3 = mixed_product(points[3] - points[1], points[2] - points[1], testPoint - points[1]);
  if (side0 * side1 < 0) return 0;
  if (side1 * side2 < 0) return 0;
  if (side2 * side3 < 0) return 0;

  return 1;*/
}

template<typename GeomSpace>
bool PointInCellEx(const typename GeomSpace::Vector3 points[4], typename GeomSpace::Vector3 testPoint, typename GeomSpace::Scalar eps = 0)
{
  typedef typename GeomSpace::Vector3 Vector3;
  typedef typename GeomSpace::Scalar  Scalar;

  Scalar side0 = MixedProduct(points[1] - points[0], points[2] - points[0], testPoint - points[0]);
  Scalar side1 = MixedProduct(points[2] - points[0], points[3] - points[0], testPoint - points[0]);
  Scalar side2 = MixedProduct(points[3] - points[0], points[1] - points[0], testPoint - points[0]);
  Scalar side3 = MixedProduct(points[3] - points[1], points[2] - points[1], testPoint - points[1]);

  //printf("s0: %f s1: %f s2: %f s3: %f\n", side0, side1, side2, side3);

  if (side0 > -eps && side1 > -eps && side2 > -eps && side3 > -eps) return 1;
  if (side0 <  eps && side1 <  eps && side2 <  eps && side3 <  eps) return 1;
  return 0;

  /*Scalar eps = std::numeric_limits<float>::epsilon();//Scalar(1e-9);
  Scalar side012 =  mixed_product(points[1] - points[0], points[2] - points[0], testPoint - points[0]) *
                    mixed_product(points[1] - points[0], points[2] - points[0], points[3] - points[0]);
  if(side012 < -eps) return 0;

  Scalar side123 =  mixed_product(points[1] - points[2], points[3] - points[2], testPoint - points[2]) *
                    mixed_product(points[1] - points[2], points[3] - points[2], points[0] - points[2]);
  if(side123 < -eps) return 0;

  Scalar side230 =  mixed_product(points[2] - points[3], points[0] - points[3], testPoint - points[3]) *
                    mixed_product(points[2] - points[3], points[0] - points[3], points[1] - points[3]);
  if(side230 < -eps) return 0;

  Scalar side013 =  mixed_product(points[0] - points[1], points[3] - points[1], testPoint - points[1]) *
                    mixed_product(points[0] - points[1], points[3] - points[1], points[2] - points[1]);
  if(side013 < -eps) return 0;

  return 1;*/


  /*Scalar side1 = mixed_product(points[2] - points[0], points[3] - points[0], testPoint - points[0]);
  Scalar side2 = mixed_product(points[3] - points[0], points[1] - points[0], testPoint - points[0]);
  Scalar side3 = mixed_product(points[3] - points[1], points[2] - points[1], testPoint - points[1]);
  if (side0 * side1 < 0) return 0;
  if (side1 * side2 < 0) return 0;
  if (side2 * side3 < 0) return 0;

  return 1;*/
}

