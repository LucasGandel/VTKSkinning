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
* @class   vtkSkeletonHierarchy
* @brief   vtkSkeletonHierarchy.
*
* A class storing for each entry the index of its parent.
* This class should be associated to a vtkSkeletonPose storing the geometrical
* data of the frame.
* For instance NodeHierarchy->GetTuple1(5) = 2 means that node 5 is the child of node 2.
* By convention, the parent of root node is chosen to be -1.
*
* A node is not necessarily a bone. Bones can be distingued from nodes using
* the internal NodeTypes array. A value different from -1 corresponds to the id
* of the bone in the skeleton poses.
*
* Node transforms are needed when computing the model global pose.
*/

#ifndef vtkSkeletonHierarchy_h
#define vtkSkeletonHierarchy_h

#include "vtkObject.h"

#include <map>
#include <vector>

class vtkIdTypeArray;
class vtkIntArray;
class vtkStringArray;

class vtkSkeletonPose;

class vtkSkeletonHierarchy : public vtkObject
{
public:
  static vtkSkeletonHierarchy* New();
	vtkTypeMacro(vtkSkeletonHierarchy, vtkObject)

	/** Add a parent id into the structure */
	void InsertNextParentId(int parent_id);

	/** Get the parent id*/
	int GetParentId(int index) const;

  /** Set the parent id*/
  void SetParentId(vtkIdType index, vtkIdType parentId);

	/** Number of bones in the structure */
	int GetNumberOfNodes() const;

  vtkGetMacro(NodeNames, vtkStringArray*);
  vtkGetMacro(NodeHierarchy, vtkIdTypeArray*);
  vtkGetMacro(NodeTypes, vtkIdTypeArray*);
  vtkGetMacro(NodeTransforms, vtkSkeletonPose*);

protected:
	vtkSkeletonHierarchy();
	~vtkSkeletonHierarchy() override;

private:
	vtkSkeletonHierarchy(const vtkSkeletonHierarchy&) = delete;
	void operator=(const vtkSkeletonHierarchy&) = delete;

	/** Internal storage of the parent id */
  vtkStringArray* NodeNames;
  vtkIdTypeArray* NodeTypes;
  vtkIdTypeArray* NodeHierarchy;

  vtkSkeletonPose* NodeTransforms;
};

#endif
