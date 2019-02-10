#include "vtkMaterial.h"

#include <vtkObjectFactory.h> // For New macro
#include <vtkStringArray.h>

////-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMaterial)

//-----------------------------------------------------------------------------
vtkMaterial::vtkMaterial()
{
  this->TextureNames = vtkStringArray::New();
  this->TextureNames->SetNumberOfComponents(1);

  this->Name = "";
}

//-----------------------------------------------------------------------------
vtkMaterial::~vtkMaterial()
{
  this->TextureNames->Delete();
}
