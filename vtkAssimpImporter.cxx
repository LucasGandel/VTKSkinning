#include "vtkAssimpImporter.h"

#include "vtkSkeletonAnimation.h"
#include "vtkSkeletonAnimationStack.h"
#include "vtkSkeletonAnimationKeys.h"
#include "vtkSkeletonHierarchy.h"
#include "vtkSkeletonPose.h"

#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolygon.h>
#include <vtkQuaternion.h>
#include <vtkStringArray.h>

// Materials (WIP)
#include "vtkMaterial.h"
#include "vtkSkeletonPolyDataMapper.h"
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkTexture.h>
#include <vtkImageReader2.h>
#include <vtkImageReader2Factory.h>
#include <vtkJPEGReader.h>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <set>
#include <sstream>

vtkObjectFactoryNewMacro(vtkAssimpImporter)

//----------------------------------------------------------------------------
typedef std::pair<int, double> BoneWeightPair;

bool GreaterThanBonePair(BoneWeightPair a, BoneWeightPair b)
{
  return a.second > b.second;
}

//----------------------------------------------------------------------------
vtkAssimpImporter::vtkAssimpImporter()
{
  this->Output = nullptr;
  this->FileName = nullptr;

  this->Actor = vtkActor::New();
  this->Mapper = vtkSkeletonPolyDataMapper::New();
}

//----------------------------------------------------------------------------
vtkAssimpImporter::~vtkAssimpImporter()
{
  this->Output->Delete();
  this->SkeletonAnimationStack->Delete();
  this->SkeletonHierarchy->Delete();
  this->SkeletonBindPose->Delete();

  this->BoneMap.clear();
 
  delete[] this->FileName;

  this->Actor->Delete();
  this->Mapper->Delete();
}

//----------------------------------------------------------------------------
void vtkAssimpImporter::Update()
{
  this->Output = vtkPolyData::New();
  this->SkeletonAnimationStack = vtkSkeletonAnimationStack::New();
  this->SkeletonHierarchy = vtkSkeletonHierarchy::New();
  this->SkeletonBindPose = vtkSkeletonPose::New();

  Assimp::Importer importer;
  const aiScene* pScene = importer.ReadFile(this->FileName,
    aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale);

  if (pScene == nullptr)
  {
    vtkErrorMacro(<< "Error parsing " << this->FileName << ": " << importer.GetErrorString());
    return;
  }

  this->ProcessMesh(pScene);
  this->ProcessHierarchyRecursive(pScene->mRootNode);
  this->ProcessAnimations(pScene);
  this->ProcessMaterials(pScene);
}

//----------------------------------------------------------------------------
vtkPolyData* vtkAssimpImporter::GetOutput()
{
  return this->Output;
}

//----------------------------------------------------------------------------
vtkActor* vtkAssimpImporter::GetActor()
{
  return this->Actor;
}

//----------------------------------------------------------------------------
vtkSkeletonPolyDataMapper* vtkAssimpImporter::GetMapper()
{
  return this->Mapper;
}

//----------------------------------------------------------------------------
vtkSkeletonAnimationStack* vtkAssimpImporter::GetOutputSkeletonAnimationStack()
{
  return this->SkeletonAnimationStack;
}

//----------------------------------------------------------------------------
vtkSkeletonHierarchy* vtkAssimpImporter::GetOutputSkeletonHierarchy()
{
  return this->SkeletonHierarchy;
}

//----------------------------------------------------------------------------
vtkSkeletonPose* vtkAssimpImporter::GetOutputSkeletonBindPose()
{
  return this->SkeletonBindPose;
}

//----------------------------------------------------------------------------
void vtkAssimpImporter::ProcessMesh(const aiScene* pScene)
{
  vtkIdType VERTEX_ID_OFFSET = 0;
  vtkIdType BONE_ID_OFFSET = 0;

  vtkNew<vtkCellArray> polys;
  vtkNew<vtkPoints> points;

  vtkNew<vtkIntArray> boneIds;
  boneIds->SetName("BoneIDs");
  boneIds->SetNumberOfComponents(4);

  vtkNew<vtkDoubleArray> weights;
  weights->SetName("Weights");
  weights->SetNumberOfComponents(4);

  vtkNew<vtkIntArray> materialIds;
  materialIds->SetName("MaterialIds");
  materialIds->SetNumberOfComponents(1);

  this->Output->GetPointData()->AddArray(boneIds);
  this->Output->GetPointData()->AddArray(weights);
  this->Output->GetPointData()->AddArray(materialIds);
  this->Output->SetPoints(points);
  this->Output->SetPolys(polys);

  vtkNew<vtkDoubleArray> normals;
  normals->SetNumberOfComponents(3);
  normals->SetName("Normals");
  for (unsigned int meshId = 0; meshId < pScene->mNumMeshes; meshId++)
  {
    const aiMesh* pMesh = pScene->mMeshes[meshId];

    // Initialize TCoords array
    for (unsigned int uvChannelId = 0; uvChannelId < pMesh->GetNumUVChannels(); uvChannelId++)
    {
      vtkNew<vtkDoubleArray> tcoords;
      tcoords->SetNumberOfComponents(2); // TODO: support 3D Tcoords // support cube map
      std::stringstream tCoordsName;
      tCoordsName << "TCoords_" << uvChannelId;
      tcoords->SetName(tCoordsName.str().c_str());

      if (uvChannelId == 0 && this->Output->GetPointData()->GetTCoords() == nullptr)
      {
        this->Output->GetPointData()->SetTCoords(tcoords);
      }

      if (uvChannelId > 0 && this->Output->GetPointData()->GetArray(tCoordsName.str().c_str()) == nullptr)
      {
        this->Output->GetPointData()->AddArray(tcoords);
      }
    }

    unsigned int vertexCount = pMesh->mNumVertices;

    // Bone Ids - Weights
    std::vector<std::vector<BoneWeightPair>> vertexBoneWeight;
    vertexBoneWeight.resize(vertexCount);
    int uniqueBoneCount = 0;
    for (unsigned int boneId = 0; boneId < pMesh->mNumBones; boneId++)
    {
      vtkStdString boneName(pMesh->mBones[boneId]->mName.C_Str());

      aiMatrix4x4 boneMatrix = pMesh->mBones[boneId]->mOffsetMatrix;

      aiQuaternion RotationQ = aiQuaternion();
      aiVector3D PositionV = aiVector3D();
      aiVector3D ScalingV = aiVector3D();

      boneMatrix.Decompose(ScalingV, RotationQ, PositionV);

      vtkQuaternion<double> orientation;
      orientation.Set(RotationQ.w, RotationQ.x, RotationQ.y, RotationQ.z);

      double position[3] = { PositionV.x * ScalingV.x, PositionV.y * ScalingV.y, PositionV.z * ScalingV.z };

      if (this->BoneMap.find(boneName) == this->BoneMap.end())
      {
        this->SkeletonBindPose->InsertNextTransform(position, orientation.GetData());
        this->BoneMap[boneName] = this->SkeletonBindPose->GetNumberOfTransforms() - 1;
        uniqueBoneCount++;
      }

      for (unsigned int j = 0; j < pMesh->mBones[boneId]->mNumWeights; j++)
      {
        int vertexID = pMesh->mBones[boneId]->mWeights[j].mVertexId;
        float weight = pMesh->mBones[boneId]->mWeights[j].mWeight;

        BoneWeightPair pair;
        pair.first = this->BoneMap[boneName];
        pair.second = weight;

        vertexBoneWeight[vertexID].push_back(pair);
      }
    }

    BONE_ID_OFFSET += uniqueBoneCount;

    for (unsigned int i = 0; i < vertexCount; i++)
    {
      // Vertex positions
      const aiVector3D* pPos = &(pMesh->mVertices[i]);
      points->InsertNextPoint(pPos->x, pPos->y, pPos->z); //WARNING: Need to rotate around X by 90° if no anim ?(https://github.com/assimp/assimp/issues/849)

      // Normals
      if (pMesh->HasNormals())
      {
        const aiVector3D* pNormal = &(pMesh->mNormals[i]);
        normals->InsertNextTuple3(pNormal->x, pNormal->y, pNormal->z);
      }

      // TCoords
      for (unsigned int uvChannelId = 0; uvChannelId < pMesh->GetNumUVChannels(); uvChannelId++)
      {
        if (!pMesh->HasTextureCoords(uvChannelId))
        {
          continue;
        }

        vtkDataArray* tcoords = nullptr;

        if (uvChannelId == 0)
        {
          tcoords = this->Output->GetPointData()->GetTCoords();
        }
        else
        {
          std::stringstream tCoordsName;
          tCoordsName << "TCoords_" << uvChannelId;
          tcoords = this->Output->GetPointData()->GetArray(tCoordsName.str().c_str());
        }

        const aiVector3D* pTCoords = &(pMesh->mTextureCoords[uvChannelId][i]);
        tcoords->InsertNextTuple2(pTCoords->x, pTCoords->y);
      }

      // Material Id (Assimp splits input mesh into one-material meshes)
      materialIds->InsertNextTuple1(pMesh->mMaterialIndex);

      // Reorder weights and boneIDs for storage
      double boneId[4] = { 0, 0, 0, 0 };
      double weight[4] = { 0.0, 0.0, 0.0, 0.0 };

      std::sort(vertexBoneWeight[i].begin(), vertexBoneWeight[i].end(), GreaterThanBonePair);

      for (int j = 0; j < vertexBoneWeight[i].size(); j++)
      {
        if (j > 3)
        {
          break;
        }
        boneId[j] = vertexBoneWeight[i][j].first;
        weight[j] = vertexBoneWeight[i][j].second;
      }
      boneIds->InsertNextTuple4(boneId[0], boneId[1], boneId[2], boneId[3]);
      weights->InsertNextTuple4(weight[0], weight[1], weight[2], weight[3]);
    }

    // Cells
    vtkIdType polygonCount = pMesh->mNumFaces;
    for (int i = 0; i < polygonCount; i++)
    {
      vtkNew<vtkPolygon> polygon;
      const aiFace& Face = pMesh->mFaces[i];

      polygon->GetPointIds()->SetNumberOfIds(Face.mNumIndices);
      for (unsigned int k = 0; k < Face.mNumIndices; k++)
      {
        polygon->GetPointIds()->SetId(k, VERTEX_ID_OFFSET + Face.mIndices[k]);
      }
      polys->InsertNextCell(polygon);
    }

    VERTEX_ID_OFFSET = points->GetNumberOfPoints();
  }

  if (normals->GetNumberOfTuples() > 0)
  {
    this->Output->GetPointData()->SetNormals(normals);
  }

}

//----------------------------------------------------------------------------
void vtkAssimpImporter::ProcessHierarchyRecursive(const aiNode* pNode)
{
  //// spacer for tree logging
  //aiNode* pParentNode = pNode->mParent;
  //std::string spacer = " ";
  //while (pParentNode != nullptr)
  //{
  //  spacer += " | ";
  //  pParentNode = pParentNode->mParent;
  //}
  vtkStdString nodeName(pNode->mName.C_Str());
  //std::cout << spacer << "Node: " << nodeName << std::endl;

  vtkStdString parentNodeName;
  if (pNode->mParent != nullptr)
  {
    parentNodeName = vtkStdString(pNode->mParent->mName.C_Str());
  }

  // Name and type
  this->SkeletonHierarchy->GetNodeNames()->InsertNextValue(nodeName);

  vtkIdType boneId = -1;
  std::map<vtkStdString, int>::iterator it = this->BoneMap.find(nodeName);
  if (it != this->BoneMap.end())
  {
    boneId = it->second;
  }

  this->SkeletonHierarchy->GetNodeTypes()->InsertNextTuple1(boneId);

  // Parenting
  vtkIdType parentId = this->SkeletonHierarchy->GetNodeNames()->LookupValue(parentNodeName);
  this->SkeletonHierarchy->InsertNextParentId(parentId);

  // Transforms
  aiQuaternion RotationQ = aiQuaternion();
  aiVector3D PositionV = aiVector3D();
  aiVector3D ScalingV = aiVector3D();

  pNode->mTransformation.Decompose(ScalingV, RotationQ, PositionV);

  vtkQuaternion<double> orientation;
  orientation.Set(RotationQ.w, RotationQ.x, RotationQ.y, RotationQ.z);

  double position[3] = { PositionV.x * ScalingV.x, PositionV.y * ScalingV.y, PositionV.z * ScalingV.z };

  this->SkeletonHierarchy->GetNodeTransforms()->InsertNextTransform(position, orientation.GetData());

  // Recursively process children
  for (unsigned int i = 0; i < pNode->mNumChildren; i++)
  {
    this->ProcessHierarchyRecursive(pNode->mChildren[i]);
  }
}

//----------------------------------------------------------------------------
void vtkAssimpImporter::ProcessAnimations(const aiScene* pScene)
{
  if (pScene == nullptr || pScene->mNumAnimations == 0)
  {
    return;
  }

  for (unsigned int animId = 0; animId < pScene->mNumAnimations; animId++)
  {
    // Animation
    const aiAnimation* pAnimation = pScene->mAnimations[animId];

    vtkNew<vtkSkeletonAnimation> animation;
    animation->SetTickPerSecond(pAnimation->mTicksPerSecond);
    animation->SetAnimationName(pAnimation->mName.C_Str());
    animation->SetDuration(pAnimation->mDuration);
    animation->SetNumberOfNodes(this->SkeletonBindPose->GetNumberOfTransforms());

    this->SkeletonAnimationStack->InsertNextAnimation(animation);

    // Iterate over the animated nodes
    for (unsigned int channelId = 0; channelId < pAnimation->mNumChannels; channelId++)
    {
      vtkStdString currentNodeName = vtkStdString(pAnimation->mChannels[channelId]->mNodeName.C_Str());
      vtkIdType nodeId = this->SkeletonHierarchy->GetNodeNames()->LookupValue(currentNodeName);
      vtkIdType boneId = this->SkeletonHierarchy->GetNodeTypes()->GetTuple1(nodeId);

      if (boneId == -1)
      {
        continue;
      }

      aiNodeAnim* pNodeAnim = pAnimation->mChannels[channelId];

      // Position keys
      vtkSkeletonAnimationKeys* positionKeys = animation->GetNodePositionKeys(boneId);
      positionKeys->SetNumberOfKeys(pNodeAnim->mNumPositionKeys);
      for (vtkIdType pKeyId = 0; pKeyId < pNodeAnim->mNumPositionKeys; pKeyId++)
      {
        const aiVector3D& PositionV = pNodeAnim->mPositionKeys[pKeyId].mValue;
        double position[3] = { PositionV.x, PositionV.y, PositionV.z };
        positionKeys->SetKey(pKeyId, position, pNodeAnim->mPositionKeys[pKeyId].mTime);
      }

      // Rotation keys
      vtkSkeletonAnimationKeys* rotationKeys = animation->GetNodeRotationKeys(boneId);
      rotationKeys->SetNumberOfKeys(pNodeAnim->mNumRotationKeys);
      for (vtkIdType rKeyId = 0; rKeyId < pNodeAnim->mNumRotationKeys; rKeyId++)
      {
        const aiQuaternion& RotationQ = pNodeAnim->mRotationKeys[rKeyId].mValue;
        double rotation[4] = { RotationQ.w, RotationQ.x, RotationQ.y, RotationQ.z };
        rotationKeys->SetKey(rKeyId, rotation, pNodeAnim->mRotationKeys[rKeyId].mTime);
      }

      // Scaling keys
      vtkSkeletonAnimationKeys* scalingKeys = animation->GetNodeScalingKeys(boneId);
      scalingKeys->SetNumberOfKeys(pNodeAnim->mNumScalingKeys);
      for (vtkIdType sKeyId = 0; sKeyId < pNodeAnim->mNumScalingKeys; sKeyId++)
      {
        const aiVector3D& ScalingV = pNodeAnim->mScalingKeys[sKeyId].mValue;
        double scaling[3] = { ScalingV.x, ScalingV.y, ScalingV.z };
        scalingKeys->SetKey(sKeyId, scaling, pNodeAnim->mScalingKeys[sKeyId].mTime);
      }
    }
  }
}

//----------------------------------------------------------------------------
void vtkAssimpImporter::ProcessMaterials(const aiScene* pScene)
{
  for (unsigned int i = 0; i < pScene->mNumMaterials; i++)
  {
    vtkNew<vtkMaterial> material;
    this->Mapper->InsertNextMaterial(material);

    aiMaterial* pMaterial = pScene->mMaterials[i];
    aiString materialName;
    pMaterial->Get(AI_MATKEY_NAME, materialName);
    material->SetName(materialName.C_Str());

    //WARNING: WIP - Only supports diffuse textures
    unsigned int numTextures = pMaterial->GetTextureCount(aiTextureType_DIFFUSE);
    for (unsigned int dTextureId = 0; dTextureId < numTextures; dTextureId++)
    {
      // WARNING: Only one albedo map is supported per material.

      aiString texturePath;
      if (pMaterial->GetTexture(aiTextureType_DIFFUSE, dTextureId, &texturePath) != AI_SUCCESS)
      {
        continue;
      }

      //WARNING: TODO - Improve texture filename and name extraction
      vtkStdString fileName = this->FileName;
      vtkStdString textureFullPath = "";
      size_t i = fileName.rfind("/", fileName.length());
      if (i != vtkStdString::npos)
      {
        textureFullPath = fileName.substr(0, i + 1);
      }

      vtkStdString relativeTexturePath = texturePath.C_Str();
      i = relativeTexturePath.rfind("\\", relativeTexturePath.length());
      if (i != vtkStdString::npos)
      {
        relativeTexturePath.replace(i, 1, "/");
      }

      textureFullPath.append(relativeTexturePath);

      i = textureFullPath.rfind('\ ', textureFullPath.length());
      if (i != vtkStdString::npos)
      {
        textureFullPath.replace(i, 1, "\ ");
      }

      vtkStdString textureFileName = "";
      i = textureFullPath.rfind("/", textureFullPath.length());
      if (i != vtkStdString::npos)
      {
        textureFileName = textureFullPath.substr(i + 1, textureFullPath.length());
      }

      vtkStdString textureName = "";
      i = textureFileName.rfind(".", textureFileName.length());
      if (i != vtkStdString::npos) {
        textureName = textureFileName.substr(0, i);
      }

      // Material albedo map name
      material->SetAlbedoTextureName(textureName);

      // Material texture Id
      int textureIndex;
      if (pMaterial->Get(AI_MATKEY_UVWSRC(aiTextureType_DIFFUSE, dTextureId), textureIndex) != AI_SUCCESS)
      {
        textureIndex = 0;
      }
      material->SetTCoordsId(textureIndex);

      // Check if the texture already exists
      if (this->Actor->GetProperty()->GetTexture(textureName) != nullptr)
      {
        continue;
      }

      // Read texture file
      vtkSmartPointer<vtkImageReader2Factory> readerFactory =
        vtkSmartPointer<vtkImageReader2Factory>::New();
      vtkImageReader2 *imageReader =
        readerFactory->CreateImageReader2(textureFullPath);

      if (imageReader == nullptr || imageReader->CanReadFile(textureFullPath) == 0)
      {
        vtkWarningMacro(<< "Could not load texture:  " << textureFullPath);
        continue;
      }

      imageReader->SetFileName(textureFullPath);

      vtkNew<vtkTexture> texture;
      texture->SetInputConnection(imageReader->GetOutputPort());
      texture->InterpolateOn();
      texture->MipmapOn();
      this->Actor->GetProperty()->SetTexture(textureName, texture);

      imageReader->Delete();
    }

    unsigned int numOpacityTextures = pMaterial->GetTextureCount(aiTextureType_OPACITY);

    unsigned int numAmbientTextures = pMaterial->GetTextureCount(aiTextureType_AMBIENT);

    unsigned int numNormalTextures = pMaterial->GetTextureCount(aiTextureType_NORMALS);
  }
}
