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
* @class   vtkSkeletonAnimationStack
* @brief   vtkSkeletonAnimationStack.
*
* Vector of vtkSkeletonAnimation.
*
*/

#ifndef vtkSkeletonAnimationStack_h
#define vtkSkeletonAnimationStack_h

#include <vtkObject.h>
#include <vtkStdString.h>

#include <vector>

class vtkSkeletonAnimation;

class vtkSkeletonAnimationStack : public vtkObject
{
public:
  static vtkSkeletonAnimationStack* New();
  vtkTypeMacro(vtkSkeletonAnimationStack, vtkObject)

  /** The number of keyframes */
  vtkIdType GetNumberOfAnimations() const;

  vtkSkeletonAnimation* GetAnimation(int index);

  void InsertNextAnimation(vtkSkeletonAnimation* animation);

  /** Empty the structure */
  void Clear();

protected:
  vtkSkeletonAnimationStack();
  ~vtkSkeletonAnimationStack() override;

private:
  vtkSkeletonAnimationStack(const vtkSkeletonAnimationStack&) = delete;
  void operator=(const vtkSkeletonAnimationStack&) = delete;

  /** Internal data */
  std::vector<vtkSkeletonAnimation*> Animations;

};

#endif
