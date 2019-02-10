#include "smvRenderManager.h"

#include "vtkAssimpImporter.h"
#include "vtkSkeletonAnimation.h"
#include "vtkSkeletonAnimationStack.h"
#include "vtkSkeletonHierarchy.h"
#include "vtkSkeletonPose.h"
#include "vtkSkeletonPolyDataMapper.h"

#include <QVTKOpenGLWidget.h>
#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCallbackCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLight.h>
#include <vtkNew.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarsToColors.h>

/** Constructor */
smvRenderManager::smvRenderManager(QObject *parent)
  : QObject(parent)
{
  this->RenderWidget = nullptr;
  this->OrientationAxesWidget = nullptr;
  this->Mesh = nullptr;
}

/** Destructor */
smvRenderManager::~smvRenderManager()
{
  this->OrientationAxesWidget->Delete();
}

/** Set QVTKOpenGLWidget used for rendering and initialize the scene resources. */
void smvRenderManager::setRenderWidget(QVTKOpenGLWidget* widget)
{
  if (!widget)
  {
    return;
  }

  this->RenderWidget = widget;

  vtkNew<vtkRenderer> renderer;
  renderer->SetBackground(0.4, 0.4, 0.4);
  renderer->SetBackground2(0.6, 0.6, 0.6);
  renderer->GradientBackgroundOn();
  //renderer->LightFollowCameraOff();

  vtkNew<vtkLight> light;
  light->SetLightTypeToSceneLight();
  light->SetPosition(10, 500, 200);
  light->SetPositional(true);
  light->SetConeAngle(180);
  light->SetFocalPoint(0, 0, 0);
  //light->SetDiffuseColor(1, 0, 0);
  //light->SetAmbientColor(0, 1, 0);
  //light->SetSpecularColor(0, 0, 1);
  renderer->AddLight(light);

  vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
  renderWindow->AddRenderer(renderer);
  this->RenderWidget->SetRenderWindow(renderWindow);

  // Add the marker widget
  vtkNew<vtkAxesActor> axes;
  this->OrientationAxesWidget = vtkOrientationMarkerWidget::New();
  this->OrientationAxesWidget->SetOutlineColor(0.9300, 0.5700, 0.1300);
  this->OrientationAxesWidget->SetOrientationMarker(axes);
  this->OrientationAxesWidget->SetInteractor(this->RenderWidget->GetInteractor());
  this->OrientationAxesWidget->SetViewport(0.0, 0.0, 0.2, 0.2);
  this->OrientationAxesWidget->SetEnabled(1);
  this->OrientationAxesWidget->InteractiveOn();
}

/** Load skinned mesh from file.
  Slot called on action Open 3D Model */
void smvRenderManager::open3DModel(QString fileName)
{
  // Import model
  vtkNew<vtkAssimpImporter> assimpImporter;
  assimpImporter->SetFileName(fileName.toStdString().c_str());
  assimpImporter->Update();

  this->Mesh = assimpImporter->GetOutput();
  if (this->Mesh == nullptr ||
    this->Mesh->GetNumberOfPoints() == 0)
  {
    return;
  }

  // Inform main window
  emit skinnedMeshLoaded(this->Mesh);
  emit hierarchyLoaded(assimpImporter->GetOutputSkeletonHierarchy());
  emit skeletonPoseLoaded(assimpImporter->GetOutputSkeletonBindPose());
  emit skeletonAnimationLoaded(assimpImporter->GetOutputSkeletonAnimationStack());

  // Create resources;
  this->Mapper = assimpImporter->GetMapper();
  this->Mapper->SetInputData(this->Mesh);
  this->Mapper->SetSkeletonHierarchy(assimpImporter->GetOutputSkeletonHierarchy());
  this->Mapper->SetSkeletonBindPose(assimpImporter->GetOutputSkeletonBindPose());
  this->Mapper->SetSkeletonAnimationStack(assimpImporter->GetOutputSkeletonAnimationStack());

  vtkActor* actor = assimpImporter->GetActor();
  actor->SetMapper(this->Mapper);

  // Create resources
  vtkNew<vtkPolyData> skeletonPolyData;
  if (assimpImporter->GetOutputSkeletonAnimationStack()->GetNumberOfAnimations() > 0)
  {
    vtkNew<vtkSkeletonPose> pose;
    assimpImporter->GetOutputSkeletonAnimationStack()->GetAnimation(0)->ComputeInterpolatedPose(0, pose);
    vtkSkeletonPose::ExtractBones(pose, assimpImporter->GetOutputSkeletonHierarchy(), skeletonPolyData);
  }
  vtkNew<vtkPolyDataMapper> skeletonMapper;
  vtkNew<vtkActor> skeletonActor;
  skeletonMapper->SetInputData(skeletonPolyData);

  skeletonActor->SetMapper(skeletonMapper);

  vtkRenderer* renderer =
    this->RenderWidget->GetRenderWindow()->GetRenderers()->GetFirstRenderer();
  renderer->AddActor(actor);
  //renderer->AddActor(skeletonActor);

  // Reset view
  renderer->ResetCamera();

  vtkRenderWindowInteractor* iren =
    this->RenderWidget->GetRenderWindow()->GetInteractor();
  iren->CreateRepeatingTimer(15);
  iren->AddObserver(vtkCommand::TimerEvent, this->Mapper->GetAnimationCallbackCommand());
}

/** Update scalars to use current array */
void smvRenderManager::onDataListRowChanged(int index)
{
  if (index == -1 || index > this->Mesh->GetPointData()->GetNumberOfArrays())
  {
    this->Mesh->GetPointData()->SetActiveScalars("");
  }
  else
  {
    this->Mesh->GetPointData()->SetActiveScalars(this->Mesh->GetPointData()->GetArrayName(index));

    double* range = this->Mesh->GetPointData()->GetArray(index)->GetRange();
    this->Mapper->SetScalarRange(range[0], range[1]);
  }

  this->RenderWidget->GetRenderWindow()->Render();
}

/** Update animation to use current index */
void smvRenderManager::onAnimationListRowChanged(int index)
{
  if (index < 0 || index > this->Mapper->GetSkeletonAnimationStack()->GetNumberOfAnimations() - 1)
  {
    return;
  }

  this->Mapper->SetFrame(0);
  this->Mapper->SetAlpha(0);
  this->Mapper->SetCurrentAnimationIndex(index);
}
