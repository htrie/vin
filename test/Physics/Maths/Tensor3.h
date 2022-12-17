#pragma once

//#include "Matrix3x3.h"
template<typename T>
class Tensor3
{
public:
  Tensor3()
  {
    xx = xy = xz = yy = yz = zz = 0;
  }
  Tensor3(T xx, T xy, T xz, T yy, T yz, T zz)
  {
    this->xx = xx;
    this->xy = xy;
    this->xz = xz;
    this->yy = yy;
    this->yz = yz;
    this->zz = zz;
  }
  Tensor3(T diag)
  {
    xx = yy = zz = diag;
    xy = xz = yz = 0;
  }
  Vector3<T> operator ()(const Vector3<T> &v) const
  {
    return Vector3<T>(
      xx * v.x + xy * v.y + xz * v.z,
      xy * v.x + yy * v.y + yz * v.z,
      xz * v.x + yz * v.y + zz * v.z);
  }

  Vector3<T> BuildEigenvalues()
  {
    Vector3<T> eigenvalues;
    //https://en.wikipedia.org/wiki/Eigenvalue_algorithm#3.C3.973_matrices
    Matrix3x3<T> matrix(xx, xy, xz, xy, yy, yz, xz, yz, zz);

    T p1 = sqr(matrix.data[0][1]) + sqr(matrix.data[0][2]) +sqr (matrix.data[1][2]);
    if(p1 < T(1e-7))
    {
      eigenvalues.x = matrix.data[0][0];
      eigenvalues.y = matrix.data[1][1];
      eigenvalues.z = matrix.data[2][2];
    }else
    {
      T q = (matrix.data[0][0] + matrix.data[1][1] + matrix.data[2][2]) / T(3.0);
      T p2 = sqr(matrix.data[0][0] - q) + sqr(matrix.data[1][1] - q) + sqr(matrix.data[2][2] - q) + T(2.0) * p1;
      T p = sqrt(p2 / T(6.0));
      Matrix3x3<T> B = (matrix - Matrix3x3<T>::identity() * q) * (T(1.0) / p);
      T r = B.GetDeterminant() / T(2.0);
      T phi;
      if(r <= T(-1.0))
      {
        phi = T(pi) / T(3.0);
      }else
      if(r >= T(1.0))
      {
        phi = 0;
      }else
      {
        phi = acos(r) / T(3.0);
      }

      eigenvalues.x = q + T(2.0) * p * cos(phi);
      eigenvalues.y = q + T(2.0) * p * cos(phi + T(2.0 / 3.0) * T(pi));
      eigenvalues.z = T(3.0) * q - eigenvalues.x - eigenvalues.y;
    }
    return eigenvalues;
  }
  void BuildEigenvaluesAndEigenvectors(Vector3<T> &eigenvalues, Vector3<T> *eigenvectors)
  {
    Matrix3x3<T> matrix(xx, xy, xz, xy, yy, yz, xz, yz, zz);

    eigenvalues = BuildEigenvalues();
    Matrix3x3<T> diff0 = matrix - Matrix3x3<T>::identity() * eigenvalues.x;
    Matrix3x3<T> diff1 = matrix - Matrix3x3<T>::identity() * eigenvalues.y;
    Matrix3x3<T> diff2 = matrix - Matrix3x3<T>::identity() * eigenvalues.z;

    Matrix3x3<T> products[3];
    
    products[0] = (diff1 * diff2);
    products[1] = (diff0 * diff2);
    products[2] = (diff0 * diff1);

    for(int valueIndex = 0; valueIndex < 3; valueIndex++)
    {
      eigenvectors[valueIndex] = products[valueIndex].GetColumn(0);
      for(int columnIndex = 1; columnIndex < 3; columnIndex++)
      {
        if(eigenvectors[valueIndex].SquareLen() < products[valueIndex].GetColumn(columnIndex).SquareLen())
        {
          eigenvectors[valueIndex] = products[valueIndex].GetColumn(columnIndex);
        }
      }
      eigenvectors[valueIndex].Normalize();
    }
  }

    
  T GetTrace()
  {
    return xx + yy + zz;
  }

  Matrix3x3<T> GetMatrix() const
  {
    return Matrix3x3<T>(xx, xy, xz, xy, yy, yz, xz, yz, zz);
  }

  T xx, xy, xz, yy, yz, zz;
};

template <typename T>
Tensor3<T> TensorProduct(const Vector3<T> &v)
{
  return Tensor3<T>(
    v.x * v.x, //xx
    v.x * v.y, //xy
    v.x * v.z, //xz
    v.y * v.y, //yy
    v.y * v.z, //yz
    v.z * v.z  //zz
  );
}

template <typename T>
Tensor3<T> SymmetricTensorProduct(const Vector3<T> &v0, const Vector3<T> &v1) {
  return Tensor3<T>(
    T(2.0) * v0.x * v1.x, //xx
    v0.x * v1.y + v0.y * v1.x, //xy
    v0.x * v1.z + v0.z * v1.x, //xz
    T(2.0) * v0.y * v1.y, //yy
    v0.y * v1.z + v0.z * v1.y, //yz
    T(2.0) * v0.z * v1.z  //zz
  );
}


template<typename T>
inline const Tensor3<T> operator +(const Tensor3<T> &t0, const Tensor3<T> &t1)
{
  return Tensor3<T>(
    t0.xx + t1.xx,
    t0.xy + t1.xy,
    t0.xz + t1.xz,
    t0.yy + t1.yy,
    t0.yz + t1.yz,
    t0.zz + t1.zz);
}

template<typename T>
inline const Tensor3<T> operator -(const Tensor3<T> &t0, const Tensor3<T> &t1)
{
  return Tensor3<T>(
    t0.xx - t1.xx,
    t0.xy - t1.xy,
    t0.xz - t1.xz,
    t0.yy - t1.yy,
    t0.yz - t1.yz,
    t0.zz - t1.zz);
}

template<typename T>
inline const Tensor3<T> operator *(const Tensor3<T> &t, const T &s)
{
  return Tensor3<T>(
    t.xx * s,
    t.xy * s,
    t.xz * s,
    t.yy * s,
    t.yz * s,
    t.zz * s);
}

template <class T>
inline T DoubleConvolution(const Tensor3<T> &v0, const Tensor3<T> &v1)
{
  return 
    v0.xx * v1.xx + 
    v0.yy * v1.yy + 
    v0.zz * v1.zz + 
    T(2.0) * v0.xy * v1.xy +
    T(2.0) * v0.xz * v1.xz +
    T(2.0) * v0.yz * v1.yz;
}

typedef Tensor3<float>	Tensor3f;
typedef Tensor3<double> Tensor3d;
