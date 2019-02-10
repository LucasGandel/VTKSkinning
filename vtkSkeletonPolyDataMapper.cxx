#include "vtkSkeletonPolyDataMapper.h"

#include "vtkDoubleArray.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h" // For New macro
#include "vtkOpenGLHelper.h"
#include "vtkOpenGLTexture.h"
#include "vtkOpenGLVertexBufferObject.h"
#include "vtkOpenGLVertexBufferObjectGroup.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkShaderProgram.h"
#include "vtkStringArray.h"
#include "vtkTextureObject.h"

#include "vtkCallbackCommand.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderWindow.h"

#include "vtkMaterial.h"
#include "vtkSkeletonAnimation.h"
#include "vtkSkeletonAnimationStack.h"
#include "vtkSkeletonHierarchy.h"
#include "vtkSkeletonPose.h"

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

  // Perform shader replacement
  if (this->IsSkinnable)
  {
    this->AddShaderPositionVCReplacement();
    this->AddShaderNormalReplacement();
  }
  this->AddShaderTCoordReplacement(act);

  Superclass::BuildBufferObjects(ren, act);
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::UpdateShaders(
  vtkOpenGLHelper &cellBO, vtkRenderer* ren, vtkActor *actor)
{
  Superclass::UpdateShaders(cellBO, ren, actor);

  /*
  * Send skinning-related uniforms to shaders.
  * Here we are right after the Superclass call to
  * "InvokeEvent(vtkCommand::UpdateShaderEvent, cellBO.Program);"
  */
  if (cellBO.Program)
  {
    this->SetSkinningShaderParameters(cellBO, ren, actor);
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
void vtkSkeletonPolyDataMapper::RenderPieceStart(vtkRenderer* ren, vtkActor *actor)
{
  Superclass::RenderPieceStart(ren, actor);
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

typedef std::pair<vtkTexture *, std::string> texinfo;

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
  // WARNING: WIP - Experimental multi-material support

  this->AddShaderReplacement(vtkShader::Vertex, "//VTK::TCoord::Dec\n", true,
    "//VTK::TCoord::Dec\n"
    "in float materialId;\n"
    "flat out float materialIdVCVSOutput;",
    false);
  this->AddShaderReplacement(vtkShader::Vertex, "//VTK::TCoord::Impl\n", true,
    "//VTK::TCoord::Impl\n"
    "materialIdVCVSOutput = materialId;\n",
    false);
  this->AddShaderReplacement(vtkShader::Geometry, "//VTK::TCoord::Dec\n", true,
    "//VTK::TCoord::Dec"
    "in float materialIdVCVSOutput[];"
    "out float materialIdVCGSOutput;",
    false);

  this->AddShaderReplacement(vtkShader::Geometry, "//VTK::TCoord::Impl\n", true,
    "//VTK::TCoord::Impl\n"
    "materialIdVCGSOutput = materialIdVCVSOutput[i];\n",
    false);

  this->AddShaderReplacement(vtkShader::Fragment, "//VTK::TCoord::Dec", true,
    "//VTK::TCoord::Dec\n"
    "flat in float materialIdVCVSOutput;\n",
    false);

  ///////////
  std::vector<texinfo> textures = this->GetTextures(actor);
  if (textures.empty())
  {
    return;
  }
  ///////////
  std::string tCoordImpFS;
  tCoordImpFS +=  "vec4 tcolor = vec4(1,1,1,1);\n";

  std::map<std::string, vtkTexture*> textureMap = actor->GetProperty()->GetAllTextures();

  for (size_t materialId = 0; materialId < this->Materials.size(); materialId++)
  {
    vtkMaterial* material = this->Materials[materialId];
    if (material->GetTextureNames()->GetNumberOfTuples() == 0)
    {
      continue;
    }

    int i = 0;

    for (int textureId = 0; textureId <  material->GetTextureNames()->GetNumberOfTuples(); textureId++)
    {

      vtkStdString textureName = material->GetTextureNames()->GetValue(textureId);
      vtkTexture *texture = nullptr;
      auto mapIt = textureMap.find(textureName);
      if (mapIt != textureMap.end())
      {
        texture = mapIt->second;
      }
      if (texture == nullptr)
        continue;

      std::stringstream ss;

      // do we have special tcoords for this texture?
      std::string tcoordname = this->GetTextureCoordinateName(textureName);
      int tcoordComps = this->VBOs->GetNumberOfComponents(tcoordname.c_str());

      std::string tCoordImpFSPre;
      std::string tCoordImpFSPost;
      if (tcoordComps == 1)
      {
        tCoordImpFSPre = "vec2(";
        tCoordImpFSPost = ", 0.0)";
      }
      else
      {
        tCoordImpFSPre = "";
        tCoordImpFSPost = "";
      }

      // Read texture color
      ss << "if (materialIdVCVSOutput == " << materialId << ")\n" <<
        "{\n";
      ss << "vec4 tcolor_" << i << " = texture(" << textureName << ", "
        << tCoordImpFSPre << tcoordname << "VCVSOutput" << tCoordImpFSPost << "); // Read texture color\n";

      // Update color based on texture number of components
      int tNumComp = vtkOpenGLTexture::SafeDownCast(texture)->GetTextureObject()->GetComponents();
      switch (tNumComp)
      {
      case 1:
        ss << "tcolor_" << i << " = vec4(tcolor_" << i << ".r,tcolor_" << i << ".r,tcolor_" << i << ".r,1.0)";
        break;
      case 2:
        ss << "tcolor_" << i << " = vec4(tcolor_" << i << ".r,tcolor_" << i << ".r,tcolor_" << i << ".r,tcolor_" << i << ".g)";
        break;
      case 3:
        ss << "tcolor_" << i << " = vec4(tcolor_" << i << ".r,tcolor_" << i << ".g,tcolor_" << i << ".b,1.0)";
      }
      ss << "; // Update color based on texture nbr of components \n";
      // Define final color based on texture blending
      if (i == 0)
      {
        ss << "tcolor = tcolor_" << i << "; // BLENDING: None (first texture) \n";
      }
      else
      {
        int tBlending = vtkOpenGLTexture::SafeDownCast(texture)->GetBlendingMode();
        switch (tBlending)
        {
        case vtkTexture::VTK_TEXTURE_BLENDING_MODE_REPLACE:
          ss << "tcolor.rgb = tcolor_" << i << ".rgb * tcolor_" << i << ".a + "
            << "tcolor.rgb * (1 - tcolor_" << i << " .a); // BLENDING: Replace\n"
            << "tcolor.a = tcolor_" << i << ".a + tcolor.a * (1 - tcolor_" << i << " .a); // BLENDING: Replace\n\n";
          break;
        case vtkTexture::VTK_TEXTURE_BLENDING_MODE_MODULATE:
          ss << "tcolor *= tcolor_" << i << "; // BLENDING: Modulate\n\n";
          break;
        case vtkTexture::VTK_TEXTURE_BLENDING_MODE_ADD:
          ss << "tcolor.rgb = tcolor_" << i << ".rgb * tcolor_" << i << ".a + "
            << "tcolor.rgb * tcolor.a; // BLENDING: Add\n"
            << "tcolor.a += tcolor_" << i << ".a; // BLENDING: Add\n\n";
          break;
        case vtkTexture::VTK_TEXTURE_BLENDING_MODE_ADD_SIGNED:
          ss << "tcolor.rgb = tcolor_" << i << ".rgb * tcolor_" << i << ".a + "
            << "tcolor.rgb * tcolor.a - 0.5; // BLENDING: Add signed\n"
            << "tcolor.a += tcolor_" << i << ".a - 0.5; // BLENDING: Add signed\n\n";
          break;
        case vtkTexture::VTK_TEXTURE_BLENDING_MODE_INTERPOLATE:
          vtkDebugMacro(<< "Interpolate blending mode not supported for OpenGL2 backend.");
          break;
        case vtkTexture::VTK_TEXTURE_BLENDING_MODE_SUBTRACT:
          ss << "tcolor.rgb -= tcolor_" << i << ".rgb * tcolor_" << i << ".a; // BLENDING: Subtract\n\n";
          break;
        default:
          vtkDebugMacro(<< "No blending mode given, ignoring this texture colors.");
          ss << "// NO BLENDING MODE: ignoring this texture colors\n";
        }
      }
      ss << "}\n";
      i++;
      tCoordImpFS += ss.str();
    }
  }

  /////////// WARNING: Remove if statements and use material id array instead.
  // Prvide 1x1 dummy texture and sample it if material does not provide texture map.

  this->AddShaderReplacement(
    vtkShader::Fragment,
    "//VTK::TCoord::Impl", // Override texture mapping.
    true,
    tCoordImpFS + "gl_FragData[0] = gl_FragData[0] * tcolor;",
    false
  );
 
}

//-----------------------------------------------------------------------------
void vtkSkeletonPolyDataMapper::InsertNextMaterial(vtkMaterial* material)
{
  material->Register(this);
  this->Materials.push_back(material);
}


