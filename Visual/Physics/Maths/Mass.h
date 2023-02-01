#pragma once

template<typename Space>
struct Mass;

template<>
struct Mass<Space3>
{
  typedef Space3::Scalar Scalar;
  typedef Space3::Vector Vector;
  typedef Space3::Tensor Tensor;
  typedef Space3::Coords Coords;
  typedef Matrix3x3<Scalar> Matrix3x3;

  Scalar mass;
  Tensor inertiaTensor;
  Coords coords;

  Mass()
  {
    mass = 0;
    inertiaTensor = Tensor(0);
    coords = Coords::defCoords();
  }

  Mass(Vector tetrahedraVertices[4], Scalar density)
  {
    //http://docsdrive.com/pdfs/sciencepublications/jmssp/2005/8-11.pdf
    typedef Polynomial<float, int, 3> P;
    Vector *v = tetrahedraVertices;

    Matrix3x3 J(v[1] - v[0], v[2] - v[0], v[3] - v[0]);
    Scalar detJ = J.GetDeterminant();
    detJ = abs(detJ);

    this->mass = detJ / Scalar(6.0) * density;
    this->inertiaTensor = Tensor(0);
    coords = Coords::defCoords();
    if(abs(this->mass) < std::numeric_limits<Scalar>::epsilon())
    {
      return;
    }



    this->coords = Coords::defCoords();
    this->coords.pos = (v[0] + v[1] + v[2] + v[3]) * Scalar(0.25);

    /*v[0] -= this->coords.pos;
    v[1] -= this->coords.pos;
    v[2] -= this->coords.pos;
    v[3] -= this->coords.pos;*/

    P xiEtaZeta[3];
    xiEtaZeta[0] = P(v[0].x - this->coords.pos.x) + P(v[1].x - v[0].x) * P("x") + P(v[2].x - v[0].x) * P("y") + P(v[3].x - v[0].x) * P("z");
    xiEtaZeta[1] = P(v[0].y - this->coords.pos.y) + P(v[1].y - v[0].y) * P("x") + P(v[2].y - v[0].y) * P("y") + P(v[3].y - v[0].y) * P("z");
    xiEtaZeta[2] = P(v[0].z - this->coords.pos.z) + P(v[1].z - v[0].z) * P("x") + P(v[2].z - v[0].z) * P("y") + P(v[3].z - v[0].z) * P("z");

//    this->inertiaTensor.xx = density * detJ * P("(y * y) + (z * z)").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);
    this->inertiaTensor.xx = density * detJ * P("(y * y) + (z * z)").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);
    this->inertiaTensor.yy = density * detJ * P("(x * x) + (z * z)").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);
    this->inertiaTensor.zz = density * detJ * P("(x * x) + (y * y)").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);

    this->inertiaTensor.xy = -density * detJ * P("x * y").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);
    this->inertiaTensor.xz = -density * detJ * P("x * z").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);
    this->inertiaTensor.yz = -density * detJ * P("y * z").Substitute(xiEtaZeta).ComputeSubspaceIntegral(3);

    
    /*float xy = -density * detJ * (
    //this->inertiaTensor.xy = -density * detJ * (
      2 * v[0].x * v[0].y + v[1].x * v[0].y + v[2].x * v[0].y + v[3].x * v[0].y + v[0].x * v[1].y +
      2 * v[1].x * v[1].y + v[2].x * v[1].y + v[3].x * v[1].y + v[0].x * v[2].y + v[1].x * v[2].y + 2 * v[2].x * v[2].y +
      v[3].x * v[2].y + v[0].x * v[3].y + v[1].x * v[3].y + v[2].x * v[3].y + 2 * v[3].x * v[3].y) / 120.0f;
    float xz = -density * detJ * (
    //this->inertiaTensor.xz = -density * detJ * (
      2 * v[0].x * v[0].z + v[1].x * v[0].z + v[2].x * v[0].z + v[3].x * v[0].z + v[0].x * v[1].z +
      2 * v[1].x * v[1].z + v[2].x * v[1].z + v[3].x * v[1].z + v[0].x * v[2].z + v[1].x * v[2].z + 2 * v[2].x * v[2].z +
      v[3].x * v[2].z + v[0].x * v[3].z + v[1].x * v[3].z + v[2].x * v[3].z + 2 * v[3].x * v[3].z) / 120.0f;
    float yz = -density * detJ * (
    //this->inertiaTensor.yz = -density * detJ * (
      2 * v[0].y * v[0].z + v[1].y * v[0].z + v[2].y * v[0].z + v[3].y * v[0].z + v[0].y * v[1].z +
      2 * v[1].y * v[1].z + v[2].y * v[1].z + v[3].y * v[1].z + v[0].y * v[2].z + v[1].y * v[2].z + 2 * v[2].y * v[2].z +
      v[3].y * v[2].z + v[0].y * v[3].z + v[1].y * v[3].z + v[2].y * v[3].z + 2 * v[3].y * v[3].z) / 120.0f;*/

  }

  Mass(Vector *vertices, unsigned int *indices, int indicesCount);


  Mass ComputePrincipal()
  {
    Mass<Space3> result = *this;
    Vector eigenvalues;
    Vector eigenvectors[3];
    inertiaTensor.BuildEigenvaluesAndEigenvectors(eigenvalues, eigenvectors);
    result.coords.xVector = coords.GetWorldVector(eigenvectors[0]);
    result.coords.yVector = coords.GetWorldVector(eigenvectors[1]);
    result.coords.zVector = coords.GetWorldVector(eigenvectors[2]);
    if(MixedProduct(result.coords.xVector, result.coords.yVector, result.coords.zVector) < 0)
    {
      result.coords.xVector = -result.coords.xVector;
    }
    result.coords.Normalize(); //DEBUG, should not do anything
    result.inertiaTensor = Tensor(0);
    result.inertiaTensor.xx = eigenvalues[0];
    result.inertiaTensor.yy = eigenvalues[1];
    result.inertiaTensor.zz = eigenvalues[2];
    return result;
  }

  const Mass<Space3> operator + (const Mass<Space3> &other)
  {
    Mass<Space3> result;
    result.mass = this->mass + other.mass;
    result.coords = Coords::defCoords();
    result.coords.pos = (this->coords.pos * this->mass + other.coords.pos * other.mass) / (this->mass + other.mass);

    Matrix3x3 rotation0(this->coords.xVector, this->coords.yVector, this->coords.zVector);
    Matrix3x3 rotation1(other.coords.xVector, other.coords.yVector, other.coords.zVector);

    Matrix3x3 tensor0 = this->inertiaTensor.GetMatrix();
    Matrix3x3 tensor1 = other.inertiaTensor.GetMatrix();

//    Matrix3x3 resultInertiaMatrix = rotation0 * tensor0 * rotation0.GetTransposed() + rotation1 * tensor1 * rotation1.GetTransposed();
    Matrix3x3 resultInertiaMatrix = rotation0.GetTransposed() * tensor0 * rotation0 + rotation1.GetTransposed() * tensor1 * rotation1;
    result.inertiaTensor = Tensor(resultInertiaMatrix.data[0][0], resultInertiaMatrix.data[0][1], resultInertiaMatrix.data[0][2], resultInertiaMatrix.data[1][1], resultInertiaMatrix.data[1][2], resultInertiaMatrix.data[2][2]);

    //https://en.wikipedia.org/wiki/Parallel_axis_theorem
    Vector deltas[2];
    Scalar masses[2];
    deltas[0] = this->coords.pos - result.coords.pos;
    deltas[1] = other.coords.pos - result.coords.pos;
    masses[0] = this->mass;
    masses[1] = other.mass;
    for(int partIndex = 0; partIndex < 2; partIndex++)
    {
      result.inertiaTensor.xx += masses[partIndex] * (deltas[partIndex].SquareLen() - deltas[partIndex].x * deltas[partIndex].x);
      result.inertiaTensor.yy += masses[partIndex] * (deltas[partIndex].SquareLen() - deltas[partIndex].y * deltas[partIndex].y);
      result.inertiaTensor.zz += masses[partIndex] * (deltas[partIndex].SquareLen() - deltas[partIndex].z * deltas[partIndex].z);
      result.inertiaTensor.xy += masses[partIndex] * (- deltas[partIndex].x * deltas[partIndex].y);
      result.inertiaTensor.xz += masses[partIndex] * (- deltas[partIndex].x * deltas[partIndex].z);
      result.inertiaTensor.yz += masses[partIndex] * (- deltas[partIndex].y * deltas[partIndex].z);
    }

    return result;
  }
  Mass(Vector *vertices, unsigned int *indices, int indicesCount, float density)
  {
    Mass result;

    Vector tetraVertices[4];
    tetraVertices[0] = Vector::zero();
    for(int triangleIndex = 0; triangleIndex < indicesCount / 3; triangleIndex++)
    {
      for(int vertexNumber = 0; vertexNumber < 3; vertexNumber++)
      {
        tetraVertices[vertexNumber + 1] = vertices[indices[triangleIndex * 3 + vertexNumber]];
      }
      result = result + Mass(tetraVertices, density);
    }
    *this = result;
  }
  template<typename VertexType>
  Mass(VertexType *vertices, unsigned int *indices, int indicesCount, float density)
  {
    Mass result;

    Vector tetraVertices[4];
    tetraVertices[0] = Vector::zero();
    for(int triangleIndex = 0; triangleIndex < indicesCount / 3; triangleIndex++)
    {
      for(int vertexNumber = 0; vertexNumber < 3; vertexNumber++)
      {
        tetraVertices[vertexNumber + 1] = vertices[indices[triangleIndex * 3 + vertexNumber]].pos;
      }
      Mass<Space3> tetraMass = Mass(tetraVertices, density);
      if(tetraMass.mass < 0)
      {
        int pp = 1;
      }
      //Mass addition = Mass(tetraVertices, density);
      result = result + Mass(tetraVertices, density);
//      printf("mass %f  xx %f", result.)
    }
    *this = result;
  }
};


