#include "vtkSkeletonAnimationStack.h"

#include "vtkSkeletonAnimation.h"

#include <vtkObjectFactory.h> // For New macro

////-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSkeletonAnimationStack)

//-----------------------------------------------------------------------------
vtkSkeletonAnimationStack::vtkSkeletonAnimationStack()
{
}

//-----------------------------------------------------------------------------
vtkSkeletonAnimationStack::~vtkSkeletonAnimationStack()
{
  this->Clear();
}

vtkIdType vtkSkeletonAnimationStack::GetNumberOfAnimations() const
{
  return static_cast<vtkIdType>(this->Animations.size());
}

vtkSkeletonAnimation* vtkSkeletonAnimationStack::GetAnimation(int const frame)
{
  return this->Animations[frame];
}

void vtkSkeletonAnimationStack::InsertNextAnimation(vtkSkeletonAnimation* animation)
{
  animation->Register(this);

  this->Animations.push_back(animation);
}

void vtkSkeletonAnimationStack::Clear()
{
  for (int i = 0; i < this->Animations.size(); i++)
  {
    this->Animations[i]->Delete();
  }
  this->Animations.clear();
}
