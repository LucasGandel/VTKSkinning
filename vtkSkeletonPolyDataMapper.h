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
* @class  vtkSkeletonPolyDataMapper
* @brief   PolyDataMapper using OpenGL to render.
*
* PolyDataMapper that uses a OpenGL to do the actual rendering.
*/

#ifndef vtkSkeletonPolyDataMapper_h
#define vtkSkeletonPolyDataMapper_h

#include "vtkOpenGLPolyDataMapper.h"

class vtkCallbackCommand;

class vtkMaterial;
class vtkSkeletonAnimationStack;
class vtkSkeletonHierarchy;
class vtkSkeletonPose;

class vtkSkeletonPolyDataMapper : public vtkOpenGLPolyDataMapper
{
public:

  static vtkSkeletonPolyDataMapper* New();
  vtkTypeMacro(vtkSkeletonPolyDataMapper, vtkOpenGLPolyDataMapper)

  void SetSkeletonBindPose(vtkSkeletonPose* pose);
  vtkGetMacro(SkeletonBindPose, vtkSkeletonPose*);

  void SetSkeletonAnimationStack(vtkSkeletonAnimationStack* animationStack);
  vtkGetMacro(SkeletonAnimationStack, vtkSkeletonAnimationStack*);

  static void UpdateAnimationCallback(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData);
  vtkGetMacro(AnimationCallbackCommand, vtkCallbackCommand*);

  void SetSkeletonHierarchy(vtkSkeletonHierarchy* hierarchy);
  vtkGetMacro(SkeletonHierarchy, vtkSkeletonHierarchy*);

  vtkGetMacro(CurrentAnimationIndex, vtkIdType);
  vtkSetMacro(CurrentAnimationIndex, vtkIdType);

  vtkGetMacro(Frame, int);
  vtkSetMacro(Frame, int);

  vtkGetMacro(Alpha, double);
  vtkSetMacro(Alpha, double);

  void InsertNextMaterial(vtkMaterial*);

protected:
  vtkSkeletonPolyDataMapper();
  ~vtkSkeletonPolyDataMapper() override;

  /** Build vertex attributes before calling the superclass. */
  void BuildBufferObjects(vtkRenderer *ren, vtkActor *act) override;

  /** Override vtkOpenGLPolyDataMapper::UpdateShaders() to call SetSkinningShaderParameters(). */
  void UpdateShaders(vtkOpenGLHelper &cellBO, vtkRenderer *ren, vtkActor *act) override;

  /** Set the shader parameters related to Skinning, called by UpdateShader */
  virtual void SetSkinningShaderParameters(vtkOpenGLHelper &cellBO, vtkRenderer *ren, vtkActor *act);

  /** Update animation frame before calling the superclass.
    WARNING: Unused. Might be needed to update animation */
  void RenderPieceStart(vtkRenderer *ren, vtkActor *act) override;

  /** Handle multi material texturing */
  void AddShaderTCoordReplacement(vtkActor* actor);

  /** Handle mesh skinning */
  void AddShaderPositionVCReplacement();

  /** Handle normal skinning */
  void AddShaderNormalReplacement();

private:
 vtkSkeletonPolyDataMapper(const vtkSkeletonPolyDataMapper&) = delete;
  void operator=(const vtkSkeletonPolyDataMapper&) = delete;

  vtkSkeletonHierarchy* SkeletonHierarchy; // Bone hierarchy
  vtkSkeletonPose* SkeletonBindPose; // Bone coordinates to world coordiantes transform
  vtkSkeletonAnimationStack* SkeletonAnimationStack; // Bone animations

  double Alpha; //interpolation between current and next frames [0.0; 1.0]
  int Frame; // current frame pose
  vtkIdType CurrentAnimationIndex;
  vtkCallbackCommand* AnimationCallbackCommand;

  bool IsSkinnable; // Indicates wether or not the required parameters are set to perform skinning.

  std::vector<vtkMaterial*> Materials;

};

#endif
