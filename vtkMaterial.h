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
* @class   vtkMaterial
* @brief   vtkMaterial.
*
* WIP: vtkMaterial
*/

#ifndef vtkMaterial_h
#define vtkMaterial_h

#include <vtkObject.h>
#include <vtkStdString.h>

class vtkMaterial : public vtkObject
{
public:
  static vtkMaterial* New();
  vtkTypeMacro(vtkMaterial, vtkObject);

  vtkGetMacro(Name, vtkStdString);
  vtkSetMacro(Name, vtkStdString);

  vtkGetMacro(AlbedoTextureName, vtkStdString);
  vtkSetMacro(AlbedoTextureName, vtkStdString);

  vtkGetMacro(TCoordsId, int);
  vtkSetMacro(TCoordsId, int);

protected:
  vtkMaterial();
  ~vtkMaterial() override;

private:
  vtkMaterial(const vtkMaterial&) = delete;
  void operator=(const vtkMaterial&) = delete;

  vtkStdString Name;

  vtkStdString AlbedoTextureName;
  int TCoordsId;
};

#endif
