#pragma once

template<typename T>
class Coords3
{
public:
  Vector3<T> xVector, yVector, zVector;
  Vector3<T> pos;
  Coords3(){}
  Coords3(const Vector3<T> &_pos, const Vector3<T> &_xVector, const Vector3<T> &_yVector, const Vector3<T> &_zVector)
  {
    xVector = _xVector;
    yVector = _yVector;
    zVector = _zVector;

    pos = _pos;
  }

  /*static const Coords Identity()
  {
    return Coords(zeroVector3<T>, xAxis, yAxis, zAxis);
  }*/

  const Vector3<T> GetLocalPoint(const Vector3<T> &globalPoint) const
  {
    Vector3<T> delta = globalPoint - pos;
    return Vector3<T>(delta * xVector, 
                      delta * yVector, 
                      delta * zVector);
  }

  const Vector3<T> GetLocalVector(const Vector3<T> &globalAxis) const 
  {
    Vector3<T> delta = globalAxis;
    return Vector3<T>(delta * xVector, 
                      delta * yVector, 
                      delta * zVector);
  }
  const Vector3<T> GetWorldPoint  (const Vector3<T> &relativePoint) const 
  {
    return pos + xVector * relativePoint.x + yVector * relativePoint.y + zVector * relativePoint.z;
  }
  const Vector3<T> GetWorldVector  (const Vector3<T> &relativeAxis) const 
  {
    return xVector * relativeAxis.x + yVector * relativeAxis.y + zVector * relativeAxis.z;
  }
  const Coords3<T> GetWorldCoords(const Coords3<T> &localCoords) const
  {
    return Coords3<T>( GetWorldPoint(localCoords.pos),
                            GetWorldVector(localCoords.xVector),
                            GetWorldVector(localCoords.yVector),
                            GetWorldVector(localCoords.zVector));
  }

  const Coords3<T> GetLocalCoords(const Coords3<T> &globalCoords) const
  {
    return Coords3<T>( GetLocalPoint(globalCoords.pos),
                            GetLocalVector(globalCoords.xVector),
                            GetLocalVector(globalCoords.yVector),
                            GetLocalVector(globalCoords.zVector));
  }

  void Rotate(Vector3<T> point, Vector3<T> axis)
  {
    T angle = axis.Len();
    if(angle == 0) return;
    Vector3f normAxis = axis / angle;
    Vector3f delta = point - this->pos;
    delta.Rotate(normAxis, angle);
    pos = point - delta;

    xVector.Rotate(normAxis, angle);
    yVector.Rotate(normAxis, angle);
    zVector.Rotate(normAxis, angle);
  }

  void Rotate(Vector3<T> point, Vector3<T> axis, float angle)
  {
    Vector3f delta = point - this->pos;
    delta.Rotate(axis, angle);
    pos = point - delta;

    xVector.Rotate(axis, angle);
    yVector.Rotate(axis, angle);
    zVector.Rotate(axis, angle);
  }

  void Rotate(Vector3<T> point, Vector3<T> initialAxis, Vector3<T> dstAxis)
  {
    Vector3f delta = point - this->pos;
    delta.Rotate(initialAxis, dstAxis);
    pos = point - delta;

    xVector.Rotate(initialAxis, dstAxis);
    yVector.Rotate(initialAxis, dstAxis);
    zVector.Rotate(initialAxis, dstAxis);
  }

  void Identity()
  {
    pos = Vector3<T>(0, 0, 0);
    xVector = Vector3<T>(1, 0, 0);
    yVector = Vector3<T>(0, 1, 0);
    zVector = Vector3<T>(0, 0, 1);
  }

  void Normalize()
  {
    Vector3f oldXVector = xVector;
    Vector3f oldYVector = yVector;
    Vector3f oldZVector = zVector;
    xVector = (yVector ^ zVector).GetNorm();
    yVector = (zVector ^ xVector).GetNorm();
    zVector = (xVector ^ yVector).GetNorm();

    if(oldXVector * xVector < 0) xVector = -xVector;
    if(oldYVector * yVector < 0) yVector = -yVector;
    if(oldZVector * zVector < 0) zVector = -zVector;
  }
  void CheckOrthogonality()
  {
    if(fabs(fabs(MixedProduct(xVector, yVector, zVector)) - 1.0f) > 1e-1f)
    {
        int pp = 1;
    }
  }

  Coords3<T> ExtractScale(Vector3f &scale) //drifts a little
  {
    scale.x = xVector.Len();
    scale.y = yVector.Len();
    scale.z = zVector.Len();
    Coords3<T> res = *this;
    res.xVector /= scale.x + T(1e-5);
    res.yVector /= scale.y + T(1e-5);
    res.zVector /= scale.z + T(1e-5);
    return res;
  }

  const Vector3<T>& GetGlobalxVector(){return xVector;}
  const Vector3<T>& GetGlobalyVector(){return yVector;}
  const Vector3<T>& GetGlobalzVector(){return zVector;}
  const Vector3<T>& GetGlobalPos(){return pos;}

  static const Coords3<T> defCoords()
  {
    return Coords3(Vector3<T>::zeroVector(), Vector3<T>::xAxis(), Vector3<T>::yAxis(), Vector3<T>::zAxis());
  }
};

typedef Coords3<float>  Coords3f;
typedef Coords3<double> Coords3d;

typedef Coords3f Coords3f;
typedef Coords3d Coords3d;
