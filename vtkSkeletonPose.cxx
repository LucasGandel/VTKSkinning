#include "vtkSkeletonPose.h"

#include "vtkSkeletonHierarchy.h"

#include <vtkFloatArray.h>
#include <vtkLine.h>     // For ExtractBones
#include <vtkMatrix4x4.h>
#include <vtkMatrix3x3.h>
#include <vtkObjectFactory.h> // For New macro
#include <vtkPoints.h>
#include <vtkPolyData.h> // For ExtractBones
#include <vtkQuaternion.h>

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSkeletonPose)

//-----------------------------------------------------------------------------
vtkSkeletonPose::vtkSkeletonPose()
{
  this->Transforms = vtkFloatArray::New();
  this->Transforms->SetNumberOfComponents(7);
}

//-----------------------------------------------------------------------------
vtkSkeletonPose::~vtkSkeletonPose()
{
  this->Transforms->Delete();
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::SetNumberOfTransforms(vtkIdType nbBones)
{
  this->Transforms->Initialize();
  this->Transforms->SetNumberOfComponents(7);
  this->Transforms->SetNumberOfTuples(nbBones);
}

//-----------------------------------------------------------------------------
vtkIdType vtkSkeletonPose::GetNumberOfTransforms() const
{
  return this->Transforms->GetNumberOfTuples();
}

//-----------------------------------------------------------------------------
double* vtkSkeletonPose::GetTransform(vtkIdType index)
{
  return this->Transforms->GetTuple(index);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::SetTransform(vtkIdType index, double* transform)
{
  this->Transforms->SetTuple(index, transform);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::SetTransform(vtkIdType index, double* position, double* orientation)
{
  double transform[7] = { position[0], position[1], position[2],
    orientation[0], orientation[1], orientation[2], orientation[3] };

  this->Transforms->SetTuple(index, transform);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::InsertNextTransform(double* transform)
{
  this->Transforms->InsertNextTuple(transform);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::InsertNextTransform(double* position, double* orientation)
{
  double transform[7] = { position[0], position[1], position[2],
    orientation[0], orientation[1], orientation[2], orientation[3] };

  this->Transforms->InsertNextTuple(transform);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::GetTransformMatrix(vtkIdType index, double* transformMatrix)
{
  double* boneTransform = this->Transforms->GetTuple(index);

  vtkQuaternion<double> boneTransformQ;
  boneTransformQ.Set(boneTransform[3], boneTransform[4], boneTransform[5], boneTransform[6]);

  double orientation[3][3];
  boneTransformQ.ToMatrix3x3(orientation);

  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      transformMatrix[4*i + j] = orientation[i][j];
    }
  }

  transformMatrix[3] = boneTransform[0];
  transformMatrix[7] = boneTransform[1];
  transformMatrix[11] = boneTransform[2];

  transformMatrix[12] = 0.0;
  transformMatrix[13] = 0.0;
  transformMatrix[14] = 0.0;
  transformMatrix[15] = 1.0;
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::ComputeGlobalPose(vtkSkeletonPose* localPose, vtkSkeletonHierarchy* hierarchy, vtkSkeletonPose* globalPose)
{
  if (hierarchy->GetNumberOfNodes() <= 0 || localPose->GetNumberOfTransforms() <= 0)
  {
    return;
  }

  globalPose->SetNumberOfTransforms(localPose->GetNumberOfTransforms());

  vtkNew<vtkSkeletonPose> hierarchyPoseGC;

  for (int i = 0; i < hierarchy->GetNumberOfNodes(); i++)
  {
    double nodeGlobalPosition[3] = { 0, 0, 0 };
    double nodeGlobalOrientation[4] = { 1.0, 0, 0, 0 };

    int parentId = hierarchy->GetParentId(i);
    vtkIdType boneId = hierarchy->GetNodeTypes()->GetTuple1(i);

    if (parentId == -1)
    {
      // No parent, this is a global transform
      hierarchyPoseGC->InsertNextTransform(hierarchy->GetNodeTransforms()->GetTransform(i));

      if (boneId != -1)
      {
        globalPose->SetTransform(boneId, hierarchy->GetNodeTransforms()->GetTransform(i));
      }
      continue;
    }

    double* nodeLocalTransform = hierarchy->GetNodeTransforms()->GetTransform(i);

    if (boneId != -1)
    {
      nodeLocalTransform = localPose->GetTransform(boneId);
    }

    double nodeLocalOrientation[4] = { nodeLocalTransform[3], nodeLocalTransform[4], nodeLocalTransform[5], nodeLocalTransform[6] };
    double nodeLocalPosition[3] = { nodeLocalTransform[0], nodeLocalTransform[1], nodeLocalTransform[2] };

    double* nodeGlobalParentTransform = hierarchyPoseGC->GetTransform(parentId);
    double nodeGlobalParentOrientation[4] = { nodeGlobalParentTransform[3], nodeGlobalParentTransform[4], nodeGlobalParentTransform[5], nodeGlobalParentTransform[6] };
    double nodeGlobalParentPosition[3] = { nodeGlobalParentTransform[0], nodeGlobalParentTransform[1], nodeGlobalParentTransform[2] };

    // Compute node global position
    vtkMath::RotateVectorByNormalizedQuaternion(nodeLocalPosition, vtkQuaternion<double>(nodeGlobalParentOrientation).Normalized().GetData(), nodeGlobalPosition);
    nodeGlobalPosition[0] += nodeGlobalParentPosition[0];
    nodeGlobalPosition[1] += nodeGlobalParentPosition[1];
    nodeGlobalPosition[2] += nodeGlobalParentPosition[2];

    // Compute node global orientation
    vtkMath::MultiplyQuaternion(nodeGlobalParentOrientation, nodeLocalOrientation, nodeGlobalOrientation);

    // Store global transform
    hierarchyPoseGC->InsertNextTransform(nodeGlobalPosition, nodeGlobalOrientation);

    if (boneId != -1)
    {
      globalPose->SetTransform(boneId, nodeGlobalPosition, nodeGlobalOrientation);
    }
  }
}

//-----------------------------------------------------------------------------
//TODO (Might be needed to invert bind pose)
//vtkSkeletonPose* vtkSkeletonPose::Inverse(vtkSkeletonPose* skeleton)
//{
//  position = -position
//  orientation = conjugated(orientation)
//}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::Multiply(vtkSkeletonPose* skeleton_1, vtkSkeletonPose* skeleton_2, vtkSkeletonPose* outputPose)
{
  if (skeleton_1->GetNumberOfTransforms() != skeleton_2->GetNumberOfTransforms())
  {
    return;
  }

  for (int k = 0; k < skeleton_1->GetNumberOfTransforms(); ++k)
  {
    double bonePosition[3] = { 0, 0, 0 };
    double boneOrientation[4] = { 1, 0, 0, 0 };

    double* boneTransform_1 = skeleton_1->GetTransform(k);
    double boneOrientation_1[4] = { boneTransform_1[3], boneTransform_1[4], boneTransform_1[5], boneTransform_1[6] };
    double bonePosition_1[3] = { boneTransform_1[0], boneTransform_1[1], boneTransform_1[2] };

    double* boneTransform_2 = skeleton_2->GetTransform(k);
    double boneOrientation_2[4] = { boneTransform_2[3], boneTransform_2[4], boneTransform_2[5], boneTransform_2[6] };
    double bonePosition_2[3] = { boneTransform_2[0], boneTransform_2[1], boneTransform_2[2] };

    // Compute bone position
    vtkMath::RotateVectorByNormalizedQuaternion(bonePosition_2, boneOrientation_1, bonePosition);
    bonePosition[0] += bonePosition_1[0];
    bonePosition[1] += bonePosition_1[1];
    bonePosition[2] += bonePosition_1[2];

    // Compute bone orientation
    vtkMath::MultiplyQuaternion(boneOrientation_1, boneOrientation_2, boneOrientation);

    outputPose->InsertNextTransform(bonePosition, boneOrientation);
  }
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::Interpolate(vtkSkeletonPose* skeleton_1, vtkSkeletonPose* skeleton_2, float const alpha, vtkSkeletonPose* outputPose)
{
  if (skeleton_1->GetNumberOfTransforms() != skeleton_2->GetNumberOfTransforms())
  {
    return;
  }

  for (int k = 0; k < skeleton_1->GetNumberOfTransforms(); ++k)
  {
    double* boneTransform_1 = skeleton_1->GetTransform(k);
    double boneOrientation_1[4] = { boneTransform_1[3], boneTransform_1[4], boneTransform_1[5], boneTransform_1[6] };
    double bonePosition_1[3] = { boneTransform_1[0], boneTransform_1[1], boneTransform_1[2] };

    double* boneTransform_2 = skeleton_2->GetTransform(k);
    double boneOrientation_2[4] = { boneTransform_2[3], boneTransform_2[4], boneTransform_2[5], boneTransform_2[6] };
    double bonePosition_2[3] = { boneTransform_2[0], boneTransform_2[1], boneTransform_2[2] };

    // Interpolate positions
    double bonePosition[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++)
    {
      bonePosition[i] = (1 - alpha) * bonePosition_1[i] + alpha * bonePosition_2[i];
    }

    // Interpolate orientation
    vtkQuaternion<double> orientationQ_1(boneOrientation_1);
    vtkQuaternion<double> orientationQ_2(boneOrientation_2);
    vtkQuaternion<double> boneOrientation = orientationQ_1.Slerp(alpha, orientationQ_2);

    outputPose->InsertNextTransform(bonePosition, boneOrientation.GetData());
  }
}

//-----------------------------------------------------------------------------
void vtkSkeletonPose::ExtractBones(vtkSkeletonPose* skeleton, vtkSkeletonHierarchy* hierarchy,  vtkPolyData* output)
{
  if (skeleton == nullptr || skeleton->GetNumberOfTransforms() <= 0)
  {
    return;
  }
  vtkNew<vtkSkeletonPose> globalPose;
  vtkSkeletonPose::ComputeGlobalPose(skeleton, hierarchy, globalPose);

  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> lines;
  vtkIdType lastBonePointId = -1;
  for (int k = 0; k < hierarchy->GetNumberOfNodes(); k++)
  {
    vtkIdType parent = hierarchy->GetParentId(k);

    vtkIdType boneId = hierarchy->GetNodeTypes()->GetTuple1(k);
    vtkIdType pointId = -1;
    vtkIdType parentId = -1;

    lastBonePointId = pointId;
    if (boneId == -1)
    {
      double* transform = hierarchy->GetNodeTransforms()->GetTransform(k);
      pointId = points->InsertNextPoint(transform[0], transform[1], transform[2]);

      continue;
    }

    double* transform = globalPose->GetTransform(boneId);
    pointId = points->InsertNextPoint(transform[0], transform[1], transform[2]);

    if (parent == -1 || hierarchy->GetNodeTypes()->GetTuple1(parent) == -1)
    {
      continue;
    }

    vtkNew<vtkLine> bone;
    bone->GetPointIds()->SetId(0, parent);
    bone->GetPointIds()->SetId(1, pointId);
    lines->InsertNextCell(bone);
  }

  output->SetPoints(points);
  output->SetLines(lines);
}
