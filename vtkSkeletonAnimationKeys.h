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
* @class   vtkSkeletonAnimationKeys
* @brief   vtkSkeletonAnimationKeys.
*
* Store position, rotation or scaling animation keys.
* A key is the association of a position, rotation or scaling tuple with an
* animation time value.
*/

#ifndef vtkSkeletonAnimationKeys_h
#define vtkSkeletonAnimationKeys_h

#include <vtkObject.h>

class vtkFloatArray;

class vtkSkeletonAnimationKeys : public vtkObject
{
public:

  enum KeyType {POSITION, ROTATION, SCALING};

  static vtkSkeletonAnimationKeys* New();
  vtkTypeMacro(vtkSkeletonAnimationKeys, vtkObject);
 
  void SetNumberOfKeys(int);

  void SetType(KeyType type);

  void InsertNextKey(double* data, double time);

  void SetKey(vtkIdType keyId, double* data, double time);

  vtkGetMacro(Data, vtkFloatArray*);
  vtkSetMacro(Data, vtkFloatArray*);

  vtkGetMacro(TimeData, vtkFloatArray*);
  vtkSetMacro(TimeData, vtkFloatArray*);

  vtkIdType GetKeyTimeIndex(double time);

protected:
  vtkSkeletonAnimationKeys();
  ~vtkSkeletonAnimationKeys() override;

private:
  vtkSkeletonAnimationKeys(const vtkSkeletonAnimationKeys&) = delete;
  void operator=(const vtkSkeletonAnimationKeys&) = delete;

  vtkFloatArray* Data;
  vtkFloatArray* TimeData;
};

#endif
