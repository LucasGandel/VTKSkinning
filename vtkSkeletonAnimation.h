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
* @class   vtkSkeletonAnimation
* @brief   vtkSkeletonAnimation.
*
* Class storing skeleton animation keys per bone.
* The size of each animation keys vector must be equal to the number of bones
* in the skeleton.
*/

#ifndef vtkSkeletonAnimation_h
#define vtkSkeletonAnimation_h

#include <vtkObject.h>
#include <vtkStdString.h>

#include <vector>

class vtkSkeletonPose;
class vtkSkeletonAnimationKeys;

class vtkSkeletonAnimation : public vtkObject
{
public:
  static vtkSkeletonAnimation* New();
  vtkTypeMacro(vtkSkeletonAnimation, vtkObject)

  /** Access to an interpolated skeleton at time given by (keyframe, value)
  * The alpha value is supposed to be between [0,1]  */
  void ComputeInterpolatedPose(float const animationTime, vtkSkeletonPose* outputPose);

  /** Empty the structure */
  void Clear();

  void InsertNextPositionKeys(vtkSkeletonAnimationKeys* positionKeys);
  void InsertNextRotationKeys(vtkSkeletonAnimationKeys* rotationKeys);
  void InsertNextScalingKeys(vtkSkeletonAnimationKeys* scalingKeys);

  void SetNumberOfNodes(vtkIdType nbNodes);
  vtkSkeletonAnimationKeys* GetNodePositionKeys(vtkIdType nodeId);
  vtkSkeletonAnimationKeys* GetNodeRotationKeys(vtkIdType nodeId);
  vtkSkeletonAnimationKeys* GetNodeScalingKeys(vtkIdType nodeId);

  vtkGetMacro(AnimationName, vtkStdString);
  vtkSetMacro(AnimationName, vtkStdString);

  vtkGetMacro(TickPerSecond, double);
  vtkSetMacro(TickPerSecond, double);

  vtkGetMacro(Duration, double);
  vtkSetMacro(Duration, double);

protected:
  vtkSkeletonAnimation();
  ~vtkSkeletonAnimation() override;

private:
  vtkSkeletonAnimation(const vtkSkeletonAnimation&) = delete;
  void operator=(const vtkSkeletonAnimation&) = delete;

  vtkStdString AnimationName;
  double TickPerSecond;
  double Duration;

  std::vector<vtkSkeletonAnimationKeys*> PositionKeys;
  std::vector<vtkSkeletonAnimationKeys*> RotationKeys;
  std::vector<vtkSkeletonAnimationKeys*> ScalingKeys;
};

#endif
