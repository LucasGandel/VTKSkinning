/*=========================================================================

Program:   Visualization Toolkit

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
* @class   vtkSkeletonPose
* @brief   vtkSkeletonPose.
*
* Array of transforms internally stored as chunks of 7 float in a vtkFloatArray.
* The first 3 values of the 7-tuples transform correspond to an xyz position
* whereas the last 4 values correspond to a wxyz quaternion.
*/

#ifndef vtkSkeletonPose_h
#define vtkSkeletonPose_h

#include "vtkObject.h"

class vtkSkeletonHierarchy;

class vtkFloatArray;
class vtkPolyData;

class vtkSkeletonPose : public vtkObject
{
public:
  static vtkSkeletonPose* New();
  vtkTypeMacro(vtkSkeletonPose, vtkObject)

  double* GetTransform(vtkIdType index);
  void SetTransform(vtkIdType index, double* transform);
  void SetTransform(vtkIdType index, double* position, double* orientation);

  void InsertNextTransform(double* transform);
  void InsertNextTransform(double* position, double* orientation);

  void SetNumberOfTransforms(vtkIdType nbBones);
  vtkIdType GetNumberOfTransforms() const;

  void GetTransformMatrix(vtkIdType index, double* transformMatrix);

  static void ComputeGlobalPose(vtkSkeletonPose* localPose, vtkSkeletonHierarchy* structure, vtkSkeletonPose* globalPose);

  /** Take the inverse of the frame (used to compute the inversed bind pose frames). */
  //static vtkSkeletonPose* Inverse(vtkSkeletonPose* skeleton);

  /** Multiply each frames of the skeleton each other.
  *  The two skeleton must have the same number of bones.  */
  static void Multiply(vtkSkeletonPose* skeleton_1, vtkSkeletonPose* skeleton_2, vtkSkeletonPose* outputPose);

  /** Interpolate each bone frames of the input skeletons between the two poses given an alpha interpolated value.
  * alpha is supposed to be between [0,1]. */
  static void Interpolate(vtkSkeletonPose* skeleton_1, vtkSkeletonPose* skeleton_2, float alpha, vtkSkeletonPose* outputPose);

  static void ExtractBones(vtkSkeletonPose*, vtkSkeletonHierarchy*, vtkPolyData*);

protected:
  vtkSkeletonPose();
  ~vtkSkeletonPose() override;

private:
  vtkSkeletonPose(const vtkSkeletonPose&) = delete;
  void operator=(const vtkSkeletonPose&) = delete;

  vtkFloatArray* Transforms;
};

#endif
