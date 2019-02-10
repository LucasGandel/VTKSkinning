#include "vtkSkeletonAnimationKeys.h"

#include <vtkFloatArray.h>
#include <vtkObjectFactory.h> // For New macro

////-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSkeletonAnimationKeys)

//-----------------------------------------------------------------------------
vtkSkeletonAnimationKeys::vtkSkeletonAnimationKeys()
{
  this->Data = vtkFloatArray::New();
  this->Data->SetNumberOfComponents(3); // Default type to POSITION/SCALING

  this->TimeData = vtkFloatArray::New();
  this->TimeData->SetNumberOfComponents(1);
}

//-----------------------------------------------------------------------------
vtkSkeletonAnimationKeys::~vtkSkeletonAnimationKeys()
{
  this->Data->Delete();
  this->TimeData->Delete();
}

//-----------------------------------------------------------------------------
void vtkSkeletonAnimationKeys::SetType(vtkSkeletonAnimationKeys::KeyType type)
{
  switch (type)
  {
  case vtkSkeletonAnimationKeys::POSITION:
  case vtkSkeletonAnimationKeys::SCALING:
    this->Data->SetNumberOfComponents(3);
    break;
  case vtkSkeletonAnimationKeys::ROTATION:
    this->Data->SetNumberOfComponents(4);
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------
void vtkSkeletonAnimationKeys::SetNumberOfKeys(int nbKeys)
{
  this->Data->SetNumberOfTuples(nbKeys);
  this->TimeData->SetNumberOfTuples(nbKeys);
}

//-----------------------------------------------------------------------------
void vtkSkeletonAnimationKeys::InsertNextKey(double* data, double time)
{
  this->Data->InsertNextTuple(data);
  this->TimeData->InsertNextTuple1(time);
}

//-----------------------------------------------------------------------------
void vtkSkeletonAnimationKeys::SetKey(vtkIdType keyId, double* data, double time)
{
  this->Data->SetTuple(keyId, data);
  this->TimeData->SetTuple1(keyId, time);
}

//-----------------------------------------------------------------------------
vtkIdType vtkSkeletonAnimationKeys::GetKeyTimeIndex(double time)
{
  for (vtkIdType k = 0; k < this->TimeData->GetNumberOfTuples(); k++)
  {
    if (this->TimeData->GetTuple1(k) > time)
    {
      return k-1; //WARNING: k == 0
    }
  }
  return 0;
}
