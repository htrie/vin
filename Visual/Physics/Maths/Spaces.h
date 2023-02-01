#pragma once
struct Space2
{
  typedef float Scalar;
  typedef Vector2f Vector;
  typedef Coords2f Coords;
  typedef AABB2f AABB;

  typedef float Angle;
  typedef float DiagTensor;
  typedef Tensor2<Scalar> Tensor;

  const static int dimsCount = 2;
};

struct Space3
{
  typedef float Scalar;
  typedef Vector3f Vector;
  typedef Coords3f Coords;
  typedef AABB3f AABB;
  typedef Vector3f Angle;
  typedef Vector3f DiagTensor;
  typedef Tensor3<Scalar> Tensor;

  const static int dimsCount = 3;
};

template<typename Space>
struct Overload{};

#define SPACE_TYPEDEFS  \
  typedef typename Space::Vector      Vector; \
  typedef typename Space::Coords      Coords; \
  typedef typename Space::AABB        AABB; \
  typedef typename Space::Angle       Angle; \
  typedef typename Space::DiagTensor  DiagTensor; \
  typedef typename Space::Tensor      Tensor; \
  typedef typename Space::Scalar      Scalar;
