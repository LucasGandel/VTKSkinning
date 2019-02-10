#include "vtkSkeletonHierarchy.h"

#include "vtkSkeletonPose.h"

#include <vtkObjectFactory.h> // For New macro
#include <vtkIdTypeArray.h>
#include <vtkIntArray.h>
#include <vtkStringArray.h>

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSkeletonHierarchy)

//-----------------------------------------------------------------------------
vtkSkeletonHierarchy::vtkSkeletonHierarchy()
{
  this->NodeNames = vtkStringArray::New();
  this->NodeNames->SetNumberOfComponents(1);

  this->NodeTypes = vtkIdTypeArray::New();
  this->NodeTypes->SetNumberOfComponents(1);

  this->NodeHierarchy = vtkIdTypeArray::New();
  this->NodeHierarchy->SetNumberOfComponents(1);

  this->NodeTransforms = vtkSkeletonPose::New();
}

//-----------------------------------------------------------------------------
vtkSkeletonHierarchy::~vtkSkeletonHierarchy()
{
  this->NodeHierarchy->Delete();
  this->NodeNames->Delete();
  this->NodeTypes->Delete();

  this->NodeTransforms->Delete();
}

//-----------------------------------------------------------------------------
void vtkSkeletonHierarchy::InsertNextParentId(int const parent_id)
{
	this->NodeHierarchy->InsertNextTuple1(parent_id);
}

//-----------------------------------------------------------------------------
int vtkSkeletonHierarchy::GetNumberOfNodes() const
{
	return this->NodeHierarchy->GetNumberOfTuples();
}

//-----------------------------------------------------------------------------
int vtkSkeletonHierarchy::GetParentId(int const index) const
{
	return this->NodeHierarchy->GetTuple1(index);
}

//-----------------------------------------------------------------------------
void vtkSkeletonHierarchy::SetParentId(vtkIdType index, vtkIdType parentId)
{
  this->NodeHierarchy->SetTuple1(index, parentId);
}
