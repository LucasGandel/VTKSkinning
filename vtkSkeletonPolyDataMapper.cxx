#include "vtkSkeletonPolyDataMapper.h"

#include "vtkDoubleArray.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h" // For New macro
#include "vtkOpenGLTexture.h"
#include "vtkOpenGLVertexArrayObject.h"
#include "vtkOpenGLVertexBufferObject.h"
#include "vtkOpenGLVertexBufferObjectGroup.h"
#include "vtkOpenGLIndexBufferObject.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkShaderProgram.h"

#include "vtkCallbackCommand.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderWindow.h"

#include "vtkMaterial.h"
#include "vtkSkeletonAnimation.h"
#include "vtkSkeletonAnimationStack.h"
#include "vtkSkeletonHierarchy.h"
#include "vtkSkeletonPose.h"

#include <set>
#include <sstream>

////-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSkeletonPolyDataMapper)

//-----------------------------------------------------------------------------
vtkSkeletonPolyDataMapper::vtkSkeletonPolyDataMapper()
{
  this->SkeletonBindPose = vtkSkeletonPose::New();
  this->SkeletonAnimationStack = vtkSkeletonAnimationStack::New();
  this->SkeletonHierarchy = vtkSkeletonHierarchy::New();

  this->Frame = 0;
  this->CurrentAnimationIndex = 0;

  this->AnimationCallbackCommand = vtkCallbackCommand::New();
  this->AnimationCallbackCommand->SetCallback(vtkSkeletonPolyDataMapper::UpdateAnimationCallback);
  this->AnimationCallbackCommand->SetClientData(this);

  this->IsSkinnable = true;
}

//-----------------------------------------------------------------------------
vtkSkeletonPolyDataMapper::~vtkSkeletonPolyDataMapper()
{
  this->SkeletonAnimationStack->Delete();
  this->SkeletonBindPose->Delete();
  this->SkeletonHierarchy->Delete();
  this->AnimationCallbackCommand->Delete();

  for (size_t i = 0; i < this->Materials.size(); i++)
  {
    this->Materials[i]->Delete();
  }
  this->Materials.clear();

  if (this->VBOTCoords != nullptr)
  {
    this->VBOTCoords->Delete();
  }
}

//-------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::SetSkeletonBindPose(vtkSkeletonPose* pose)
{
  if (this->SkeletonBindPose == pose)
  {
    return;
  }
  if (this->SkeletonBindPose != nullptr)
  {
    this->SkeletonBindPose->Delete();
  }
  pose->Register(this);
  this->SkeletonBindPose = pose;
}

//-------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::SetSkeletonAnimationStack(vtkSkeletonAnimationStack* animationStack)
{
  if (this->SkeletonAnimationStack == animationStack)
  {
    return;
  }

  if (this->SkeletonAnimationStack != nullptr)
  {
    this->SkeletonAnimationStack->Delete();
  }
  animationStack->Register(this);
  this->SkeletonAnimationStack = animationStack;
}

//-------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::SetSkeletonHierarchy(vtkSkeletonHierarchy* hierarchy)
{
  if (this->SkeletonHierarchy == hierarchy)
  {
    return;
  }

  if (this->SkeletonHierarchy != nullptr)
  {
    this->SkeletonHierarchy->Delete();
  }
  hierarchy->Register(this);
  this->SkeletonHierarchy = hierarchy;
}

//-------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::BuildBufferObjects(
  vtkRenderer *ren, vtkActor *act)
{
  vtkPolyData *poly = this->CurrentInput;

  if (poly == nullptr)
  {
    return;
  }

  // Look for weights attribute
  vtkDoubleArray* weights =
    vtkDoubleArray::SafeDownCast(poly->GetPointData()->GetAbstractArray("Weights"));
  if (weights == nullptr)
  {
    vtkWarningMacro(<< "Weights not found in vtkSkeletonPolyDataMapper input."
      "Skinning won't be performed.");
    this->IsSkinnable = false;
  }
  else
  {
    this->VBOs->CacheDataArray("weights", weights, ren, VTK_FLOAT);
  }

  // Look for bone IDs attribute
  vtkIntArray* boneIDs =
    vtkIntArray::SafeDownCast(poly->GetPointData()->GetAbstractArray("BoneIDs"));
  if (boneIDs == nullptr)
  {
    vtkWarningMacro(<< "BoneIDs not found in vtkSkeletonPolyDataMapper input."
      "Skinning won't be performed.");
    this->IsSkinnable = false;
  }
  else
  {
    this->VBOs->CacheDataArray("boneIDs", boneIDs, ren, VTK_INT);
  }

  // Look for the animation
  if (this->SkeletonAnimationStack->GetNumberOfAnimations() <= 0)
  {
    vtkWarningMacro(<< "vtkSkeletonPolyDataMapper animation has no pose ."
      "Skinning won't be performed.");
    this->IsSkinnable = false;
  }

  // Look for the bind pose
  if (this->SkeletonBindPose->GetNumberOfTransforms() <= 0)
  {
    vtkWarningMacro(<< "vtkSkeletonPolyDataMapper has no bind pose ."
      "Skinning won't be performed.");
    this->IsSkinnable = false;
  }

  // Look for material Ids attribute
  vtkIntArray* materialIds =
    vtkIntArray::SafeDownCast(poly->GetPointData()->GetAbstractArray("MaterialIds"));
  if (materialIds == nullptr)
  {
    vtkWarningMacro(<< "MaterialIds not found in vtkSkeletonPolyDataMapper input."
      "Shading won't be performed.")
  }
  else
  {
    this->VBOs->CacheDataArray("materialId", materialIds, ren, VTK_INT);
  }

  // Handle multiple TCoords arrays indexed by material id
  std::set<int> tCoordsIds;
  for (auto it = this->Materials.begin(); it != this->Materials.end(); it++)
  {
    int tCoordsId = (*it)->GetTCoordsId();
    if (tCoordsIds.find(tCoordsId) != tCoordsIds.end())
    {
      continue;
    }

    tCoordsIds.insert(tCoordsId);
  }

  // Append TCoords arrays in VBO
  if (tCoordsIds.size() > 0)
  {
    this->VBOTCoords = vtkOpenGLVertexBufferObject::New();
    this->VBOTCoords->SetDataType(VTK_FLOAT);

    for (auto it = tCoordsIds.begin(); it != tCoordsIds.end(); it++)
    {
      vtkStdString tCoordsName = "TCoords_" + std::to_string(*it);
      vtkDataArray* tCoords = poly->GetPointData()->GetArray(tCoordsName);

      if (tCoords == nullptr)
      {
        continue;
      }

      this->VBOTCoords->AppendDataArray(tCoords);
    }

    this->VBOTCoords->UploadVBO();
  }

  // WARNING: TODO Prevent Superclass to bind tcoords if already done here
  Superclass::BuildBufferObjects(ren, act);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::BuildShaders(
  std::map<vtkShader::Type, vtkShader *> shaders,
  vtkRenderer *ren, vtkActor *actor)
{
  // Perform shader replacement
  if (this->IsSkinnable)
  {
    this->AddShaderPositionVCReplacement();
    this->AddShaderNormalReplacement();
  }
  this->AddShaderTCoordReplacement(actor);

  Superclass::BuildShaders(shaders, ren, actor);
}

//-----------------------------------------------------------------------------
bool vtkSkeletonPolyDataMapper::HaveTextures(vtkActor *actor)
{
  // Overriden to prevent handling of actor textures when we index textures
  // using material ids.
  return (!this->HaveTexturedMaterials && this->GetNumberOfTextures(actor) > 0);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::SetMapperShaderParameters(vtkOpenGLHelper &cellBO,
  vtkRenderer* ren, vtkActor *actor)
{
  this->HaveTexturedMaterials = false;

  // Handle multiple textures indexed by material id.
  if (this->Materials.size() > 0)
  {
    std::vector<int> materialAlbedoSampler2DIds;
    materialAlbedoSampler2DIds.resize(this->Materials.size());

    std::vector<int> materialTCoordsIds;
    materialTCoordsIds.resize(this->Materials.size());

    for (size_t materialId = 0; materialId < this->Materials.size(); materialId++)
    {
      vtkMaterial* material = this->Materials[materialId];

      materialTCoordsIds[materialId] = material->GetTCoordsId();// TODO Handle default TCoordsId

      vtkStdString albedoTextureName = material->GetAlbedoTextureName();
      vtkTexture* albedoTexture = actor->GetProperty()->GetTexture(albedoTextureName);

      if (albedoTexture == nullptr)
      {
        //WARNING: Unsupported. TODO Handle default textureId and map using a 1x1 blank albedo texture.
        continue;
      }

      int tunit = vtkOpenGLTexture::SafeDownCast(albedoTexture)->GetTextureUnit();
      std::string textureShaderName = "albedoSampler2D[" + std::to_string(tunit) + "]";
      cellBO.Program->SetUniformi(textureShaderName.c_str(), tunit);

      materialAlbedoSampler2DIds[materialId] = tunit;

      // Prevent Superclass to handle actor textures
      // (see overriden vtkSkeletonPolyDataMapper::HaveTextures(vtkActor *actor))
      this->HaveTexturedMaterials = true;
    }

    cellBO.Program->SetUniform1iv("materialAlbedoSampler2DId",
      static_cast<int>(this->Materials.size()), &materialAlbedoSampler2DIds[0]);
    cellBO.Program->SetUniform1iv("materialTCoordsId",
      static_cast<int>(this->Materials.size()), &materialTCoordsIds[0]);
  }

  // Superclass call to SetMapperShaderParameters.
  // Needs to be done prior to adding the TCoords VBO to ensure that the VAO
  // is ready.
  Superclass::SetMapperShaderParameters(cellBO, ren, actor);

  // Send skinning-related uniforms to shaders.
  this->SetSkinningShaderParameters(cellBO, ren, actor);

  // Handle multiple TCoords array indexed by material id.
  if (this->VBOTCoords == nullptr)
  {
    return;
  }

  if (cellBO.IBO->IndexCount &&
    (this->VBOTCoords->GetMTime() > cellBO.AttributeUpdateTime ||
      cellBO.ShaderSourceTime > cellBO.AttributeUpdateTime))
  {
    return;
  }

  // Handle multiple TCoords array indexed by material id.
  // The VBO of texture coordinates is a buffer containing the appended
  // TCoords array. The total number of texture coordinates arrays is equal
  // to the number of tuples in the VBO, divided by the expected number of
  // tuples in each TCoords array.
  int numberOfTCoords =
    this->VBOTCoords->GetNumberOfTuples() / this->CurrentInput->GetPointData()->GetNumberOfTuples();

  int stride = this->VBOTCoords->GetStride();
  int elementType = this->VBOTCoords->GetDataType();
  int elementTupleSize = this->VBOTCoords->GetNumberOfComponents();
  bool normalize = false;

  for (int tCoordsId = 0; tCoordsId < numberOfTCoords; tCoordsId++)
  {
    int offset = tCoordsId * this->VBOTCoords->GetNumberOfTuples() / numberOfTCoords * stride;

    std::stringstream attribName;
    attribName << "TCoords[" << tCoordsId << "]";

    bool attributeAdded = cellBO.VAO->AddAttributeArray(cellBO.Program, this->VBOTCoords, attribName.str(),
      offset, stride, elementType, elementTupleSize, normalize);
  }
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::SetSkinningShaderParameters(vtkOpenGLHelper &cellBO,
  vtkRenderer* ren, vtkActor *actor)
{
  if (!this->IsSkinnable)
  {
    return;
  }

  vtkSkeletonAnimation* currentAnimation = this->SkeletonAnimationStack->GetAnimation(this->CurrentAnimationIndex);

  vtkNew<vtkSkeletonPose> animationPose;
  currentAnimation->ComputeInterpolatedPose(this->Frame, animationPose);

  vtkNew<vtkSkeletonPose> globalPose;
  vtkSkeletonPose::ComputeGlobalPose(animationPose, this->SkeletonHierarchy, globalPose);

  vtkNew<vtkSkeletonPose> pose;
  vtkSkeletonPose::Multiply(globalPose, this->SkeletonBindPose, pose);

  std::vector<float> boneTransforms;
  boneTransforms.resize(16 * pose->GetNumberOfTransforms());

  for (unsigned int k = 0; k < pose->GetNumberOfTransforms(); k++)
  {
    double boneMatrix[16];
    pose->GetTransformMatrix(k, boneMatrix);

    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        boneTransforms[k * 16 + 4 * i + j] = boneMatrix[4 * j + i];
      }
    }
  }

  cellBO.Program->SetUniformMatrix4x4v("SkeletonPose",
    static_cast<int>(boneTransforms.size()) / 16,
    &boneTransforms[0]);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::UpdateAnimationCallback(vtkObject* caller,
  long unsigned int vtkNotUsed(eventId), void* clientData, void* vtkNotUsed(callData))
{
  vtkSkeletonPolyDataMapper *mapper =
    static_cast<vtkSkeletonPolyDataMapper*>(clientData);

  if (mapper->GetSkeletonAnimationStack()->GetNumberOfAnimations() == 0)
  {
    return;
  }

  vtkSkeletonAnimation* currentAnimation = mapper->GetSkeletonAnimationStack()->GetAnimation(mapper->GetCurrentAnimationIndex());

  if (currentAnimation == nullptr)
  {
    return;
  }

  // Update animation frame
  double dA = 15 / (1.0e3 / currentAnimation->GetTickPerSecond());
  mapper->Alpha += dA;
  if (mapper->Alpha > 1.0)
  {
    mapper->Alpha = 0.0;
    mapper->Frame = (mapper->Frame + 1) % ((int)currentAnimation->GetDuration());
  }

  vtkRenderWindowInteractor *iren =
    static_cast<vtkRenderWindowInteractor*>(caller);

  iren->GetRenderWindow()->Render();
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::AddShaderPositionVCReplacement()
{
  std::stringstream vertexShaderDecl;
  vertexShaderDecl <<
    "//VTK::PositionVC::Dec\n" // we still want the default
    "attribute vec4 weights;\n"
    "attribute vec4 boneIDs;\n"
    "uniform mat4 SkeletonPose[" << this->SkeletonBindPose->GetNumberOfTransforms() << "];\n";

  this->AddShaderReplacement(
    vtkShader::Vertex,
    "//VTK::PositionVC::Dec",
    true, // before the standard replacements
    vertexShaderDecl.str(),
    false // only do it once
  );

  this->AddShaderReplacement(
    vtkShader::Vertex,
    "//VTK::PositionVC::Impl", // Override vertex output position.
    true,
    "vec4 p = vec4(0,0,0,0);\n"
    "p = p + weights.x * (SkeletonPose[int(boneIDs.x)] * vertexMC);\n"
    "p = p + weights.y * (SkeletonPose[int(boneIDs.y)] * vertexMC);\n"
    "p = p + weights.z * (SkeletonPose[int(boneIDs.z)] * vertexMC);\n"
    "p = p + weights.w * (SkeletonPose[int(boneIDs.w)] * vertexMC);\n"
    "vertexVCVSOutput = MCVCMatrix * p;\n"
    "gl_Position =  MCDCMatrix * p;\n",
    false
  );
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::AddShaderNormalReplacement()
{
  // WARNING: This assumes that we have point normals. See vtkOpenGLPolyDataMapper
  this->AddShaderReplacement(
    vtkShader::Vertex,
    "//VTK::Normal::Impl",
    true,
    "//VTK::Normal::Impl" // We still want the default.
    "vec4 n = vec4(0,0,0,0);\n"
    "n = n + weights.x * (SkeletonPose[int(boneIDs.x)] * vec4(normalMC, 0));\n"
    "n = n + weights.y * (SkeletonPose[int(boneIDs.y)] * vec4(normalMC, 0));\n"
    "n = n + weights.z * (SkeletonPose[int(boneIDs.z)] * vec4(normalMC, 0));\n"
    "n = n + weights.w * (SkeletonPose[int(boneIDs.w)] * vec4(normalMC, 0));\n"
    "normalVCVSOutput = normalMatrix * n.xyz;",
    false
  );
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::AddShaderTCoordReplacement(vtkActor* actor)
{
  // Handle multiple material arrays indexed by material id
  // Here we add texture map and texture coordinate arrays declarations.

  // Find the different textures and TCoords arrays
  std::set<int> tCoordsIds;
  std::set<std::string> albedoNames;

  for (auto it = this->Materials.begin(); it != this->Materials.end(); it++)
  {
    // TCoords id
    int tCoordsId = (*it)->GetTCoordsId();
    if (tCoordsIds.find(tCoordsId) == tCoordsIds.end())
    {
      tCoordsIds.insert(tCoordsId);
    }

    // Albedo name
    std::string albedoName = (*it)->GetAlbedoTextureName();
    if (albedoNames.find(albedoName) == albedoNames.end())
    {
      albedoNames.insert(albedoName);
    }
  }

  if (tCoordsIds.size() == 0 || albedoNames.size() == 0)
  {
    return;
  }

  // Add TCoords array declaration
  // WARNING: This will not prevent superclass to add its own declarations.
  //   TODO Prevent superclass from adding declarations if not needed
  std::string tCoordsIdsSizeStr = std::to_string(tCoordsIds.size());

  this->AddShaderReplacement(vtkShader::Vertex, "//VTK::TCoord::Dec\n", true,
    "//VTK::TCoord::Dec\n"
    "in float materialId;\n"
    "in vec2 TCoords[" + tCoordsIdsSizeStr + "];\n"
    "flat out float materialIdVCVSOutput;\n"
    "out vec2 TCoordsVCVSOutput[" + tCoordsIdsSizeStr + "];\n",
    false);

  this->AddShaderReplacement(vtkShader::Vertex, "//VTK::TCoord::Impl\n", true,
    "//VTK::TCoord::Impl\n"
    "materialIdVCVSOutput = materialId;\n"
    "for(int k = 0; k < " + tCoordsIdsSizeStr + "; k++)\n"
    "{\n"
    "  TCoordsVCVSOutput[k] = TCoords[k];\n"
    "}\n",
    false);

  this->AddShaderReplacement(vtkShader::Geometry, "//VTK::TCoord::Dec\n", true,
    "//VTK::TCoord::Dec\n"
    "in float materialIdVCVSOutput[];\n"
    "in vec2 TCoordsVCVSOutput[" + tCoordsIdsSizeStr + "][];\n " //WARNING: Untested
    "out float materialIdVCGSOutput;\n"
    "out vec2 TCoordsVCGSOutput[" + tCoordsIdsSizeStr + "];\n",
    false);

  this->AddShaderReplacement(vtkShader::Geometry, "//VTK::TCoord::Impl\n", true,
    "//VTK::TCoord::Impl\n"
    "materialIdVCGSOutput = materialIdVCVSOutput[i];\n"
    "for(int k = 0; k < " + tCoordsIdsSizeStr + "; k++)\n"
    "{\n"
    "  TCoordsVCGSOutput[k] =  TCoordsVCVSOutput[k][i];\n" //WARNING: Untested
    "}\n",
    false);

  this->AddShaderReplacement(vtkShader::Fragment, "//VTK::TCoord::Dec", true,
    "//VTK::TCoord::Dec\n"
    "flat in float materialIdVCVSOutput;\n"
    "in vec2 TCoordsVCVSOutput[" + tCoordsIdsSizeStr + "];\n",
    false);

  // Override texture mapping declaration.
  //   TODO : handle cube maps
  std::string tMapDecFS;

  // Add albedo sampler declaration
  std::string albedoNamesSizeStr = std::to_string(tCoordsIds.size());
  tMapDecFS += "uniform sampler2D albedoSampler2D["+ albedoNamesSizeStr +"];\n";

  // Add declaration to map material id to albedo sampler id (filled in SetMapperShaderParameters())
  tMapDecFS += "uniform int materialAlbedoSampler2DId[" + std::to_string(this->Materials.size()) + "];";

  // Add declaration to map material id to albedo sampler id (filled in SetMapperShaderParameters())
  tMapDecFS += "uniform int materialTCoordsId[" + std::to_string(this->Materials.size()) + "];";

  this->AddShaderReplacement(
    vtkShader::Fragment,
    "//VTK::TMap::Dec",
    true,
    tMapDecFS,
    false
  );

  // Override texture mapping.
  std::string tCoordImpFS;
  tCoordImpFS +=
    "vec4 tcolor = texture(albedoSampler2D[materialAlbedoSampler2DId[int(materialIdVCVSOutput)]],"
      "TCoordsVCVSOutput[materialTCoordsId[int(materialIdVCVSOutput)]]);\n";

  this->AddShaderReplacement(
    vtkShader::Fragment,
    "//VTK::TCoord::Impl",
    true,
    tCoordImpFS + "gl_FragData[0] = gl_FragData[0] * tcolor;\n",
    false
  );
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::InsertNextMaterial(vtkMaterial* material)
{
  material->Register(this);
  this->Materials.push_back(material);
}
