/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkRenderer.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkAssimpImporter
 * @brief   Import FBX Skinned Mesh
 *
 * @sa
*/
#ifndef __vtkAssimpImporter_h
#define __vtkAssimpImporter_h

#include "vtkObject.h"

#include <assimp/scene.h>

#include <map>

class vtkSkeletonAnimationStack;
class vtkSkeletonHierarchy;
class vtkSkeletonPose;

class vtkSkeletonPolyDataMapper;
class vtkActor;

class vtkMatrix4x4;
class vtkPolyData;
class vtkStringArray;

struct aiNode;

class VTK_EXPORT vtkAssimpImporter : public vtkObject  // WARNING : Consider use of vtkImporter
{
public:
  static vtkAssimpImporter *New();
  vtkTypeMacro(vtkAssimpImporter, vtkObject);

  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  void Update();

  vtkPolyData* GetOutput();
  vtkSkeletonAnimationStack* GetOutputSkeletonAnimationStack();
  vtkSkeletonHierarchy* GetOutputSkeletonHierarchy();
  vtkSkeletonPose* GetOutputSkeletonBindPose();

  vtkActor* GetActor();
  vtkSkeletonPolyDataMapper* GetMapper();

protected:
  vtkAssimpImporter();
  ~vtkAssimpImporter();

private:
  vtkAssimpImporter(const vtkAssimpImporter&);  // Not implemented.
  void operator=(const vtkAssimpImporter&);  // Not implemented.

  void ProcessMesh(const aiScene* pScene);
  void ProcessHierarchyRecursive(const aiNode* pNode);
  void ProcessAnimations(const aiScene* pScene);
  void ProcessMaterials(const aiScene* pScene);

  char* FileName;
  vtkPolyData* Output;
  vtkSkeletonAnimationStack* SkeletonAnimationStack;
  vtkSkeletonHierarchy* SkeletonHierarchy;
  vtkSkeletonPose* SkeletonBindPose;

  std::map<vtkStdString, int> BoneMap;

  vtkSkeletonPolyDataMapper* Mapper;
  vtkActor* Actor;

};

#endif
