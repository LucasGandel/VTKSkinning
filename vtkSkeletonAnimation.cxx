#include "vtkSkeletonAnimation.h"

#include "vtkSkeletonPose.h"
#include "vtkSkeletonAnimationKeys.h"

#include <vtkFloatArray.h>
#include <vtkObjectFactory.h> // For New macro
#include <vtkQuaternion.h>

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSkeletonAnimation)

//-----------------------------------------------------------------------------
vtkSkeletonAnimation::vtkSkeletonAnimation()
{
}

//-----------------------------------------------------------------------------
vtkSkeletonAnimation::~vtkSkeletonAnimation()
{
  this->Clear();
}

void vtkSkeletonAnimation::Clear()
{
  for (int i = 0; i < this->PositionKeys.size(); i++)
  {
    this->PositionKeys[i]->Delete();
  }
  this->PositionKeys.clear();

  for (int i = 0; i < this->RotationKeys.size(); i++)
  {
    this->RotationKeys[i]->Delete();
  }
  this->RotationKeys.clear();

  for (int i = 0; i < this->ScalingKeys.size(); i++)
  {
    this->ScalingKeys[i]->Delete();
  }
  this->ScalingKeys.clear();
}

// Insert rotation keys for a node
void vtkSkeletonAnimation::InsertNextPositionKeys(vtkSkeletonAnimationKeys* positionKeys)
{
  positionKeys->Register(this);
  this->PositionKeys.push_back(positionKeys);
}

void vtkSkeletonAnimation::InsertNextRotationKeys(vtkSkeletonAnimationKeys* rotationKeys)
{
  rotationKeys->Register(this);
  this->RotationKeys.push_back(rotationKeys);
}

void vtkSkeletonAnimation::InsertNextScalingKeys(vtkSkeletonAnimationKeys* scalingKeys)
{
  scalingKeys->Register(this);
  this->ScalingKeys.push_back(scalingKeys);
}

void vtkSkeletonAnimation::SetNumberOfNodes(vtkIdType nbNodes)
{
  for (int i = 0; i < nbNodes; i++)
  {
    vtkNew<vtkSkeletonAnimationKeys> positionKey;
    positionKey->SetType(vtkSkeletonAnimationKeys::POSITION);
    this->InsertNextPositionKeys(positionKey);

    vtkNew<vtkSkeletonAnimationKeys> rotationKey;
    rotationKey->SetType(vtkSkeletonAnimationKeys::ROTATION);
    this->InsertNextRotationKeys(rotationKey);

    vtkNew<vtkSkeletonAnimationKeys> scalingKey;
    scalingKey->SetType(vtkSkeletonAnimationKeys::SCALING);
    this->InsertNextScalingKeys(scalingKey);
  }
}

vtkSkeletonAnimationKeys* vtkSkeletonAnimation::GetNodePositionKeys(vtkIdType nodeId)
{
  return this->PositionKeys[nodeId];
}

vtkSkeletonAnimationKeys* vtkSkeletonAnimation::GetNodeRotationKeys(vtkIdType nodeId)
{
  return this->RotationKeys[nodeId];
}

vtkSkeletonAnimationKeys* vtkSkeletonAnimation::GetNodeScalingKeys(vtkIdType nodeId)
{
  return this->ScalingKeys[nodeId];
}

void vtkSkeletonAnimation::ComputeInterpolatedPose(float const animationTime, vtkSkeletonPose* outputPose)
{
  outputPose->SetNumberOfTransforms(this->RotationKeys.size());

  for (int k = 0; k < outputPose->GetNumberOfTransforms(); k++)
  {
    double animTime = fmod(animationTime, this->Duration);
    vtkIdType pKeyId = this->PositionKeys[k]->GetKeyTimeIndex(animTime);// xyz position
    vtkIdType rKeyId = this->RotationKeys[k]->GetKeyTimeIndex(animTime);// wxyz quaternion
    vtkIdType sKeyId = this->ScalingKeys[k]->GetKeyTimeIndex(animTime);  // xyz scaling

    double* position = this->PositionKeys[k]->GetData()->GetTuple3(pKeyId);// xyz position
    double* rotation = this->RotationKeys[k]->GetData()->GetTuple4(rKeyId);// wxyz quaternion
    double* scaling = this->ScalingKeys[k]->GetData()->GetTuple3(sKeyId);  // xyz scaling

    double* nextPosition = this->PositionKeys[k]->GetData()->GetTuple3((pKeyId + 1 )% this->PositionKeys[k]->GetData()->GetNumberOfTuples());// xyz position
    double* nextRotation = this->RotationKeys[k]->GetData()->GetTuple4((rKeyId + 1) % this->RotationKeys[k]->GetData()->GetNumberOfTuples());// wxyz quaternion
    double* nextScaling = this->ScalingKeys[k]->GetData()->GetTuple3((sKeyId + 1) % this->ScalingKeys[k]->GetData()->GetNumberOfTuples());  // xyz scaling

    double currentPTime = this->PositionKeys[k]->GetTimeData()->GetTuple1(pKeyId);
    double nextPTime = this->PositionKeys[k]->GetTimeData()->GetTuple1((pKeyId + 1) % this->PositionKeys[k]->GetData()->GetNumberOfTuples());
    double alphaP = (animTime - currentPTime) / (nextPTime - currentPTime);

    double currentRTime = this->RotationKeys[k]->GetTimeData()->GetTuple1(rKeyId);
    double nextRTime = this->RotationKeys[k]->GetTimeData()->GetTuple1((rKeyId + 1) % this->RotationKeys[k]->GetData()->GetNumberOfTuples());
    double alphaR = (animTime - currentRTime) / (nextRTime - currentRTime);

    double currentSTime = this->ScalingKeys[k]->GetTimeData()->GetTuple1(sKeyId);
    double nextSTime = this->ScalingKeys[k]->GetTimeData()->GetTuple1((sKeyId + 1) % this->ScalingKeys[k]->GetData()->GetNumberOfTuples());
    double alphaS = (nextSTime - currentSTime) >0?(animTime - currentSTime) / (nextSTime - currentSTime) : 0;

    double boneScaling[3] = { 0, 0, 0 };
    double bonePosition[3] = { 0, 0, 0 };

    for (int i = 0; i < 3; i++)
    {
      bonePosition[i] = (1 - alphaP) * position[i] + alphaP * nextPosition[i];
      boneScaling[i] = (1 - alphaS) * scaling[i] + alphaS * nextScaling[i];

      bonePosition[i] *= boneScaling[i];
    }

    // Interpolate orientation
    vtkQuaternion<double> orientationQ_1(rotation[0], rotation[1], rotation[2], rotation[3]);
    vtkQuaternion<double> orientationQ_2(nextRotation[0], nextRotation[1], nextRotation[2], nextRotation[3]);
    vtkQuaternion<double> boneOrientation = orientationQ_1.Slerp(alphaR, orientationQ_2);

    outputPose->SetTransform(k, bonePosition, boneOrientation.GetData());
  }
}
