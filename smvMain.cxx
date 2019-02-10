#include "smvMainWindow.h"
#include "smvRenderManager.h"

#include <QApplication>

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  smvMainWindow mainWindow;
  smvRenderManager renderManager;

  //Inform the main window about loaded data
  QObject::connect(&renderManager, SIGNAL(skinnedMeshLoaded(vtkPolyData*)), &mainWindow, SLOT(setMeshInformation(vtkPolyData*)));
  QObject::connect(&renderManager, SIGNAL(hierarchyLoaded(vtkSkeletonHierarchy*)), &mainWindow, SLOT(setHierarchyInformation(vtkSkeletonHierarchy*)));
  QObject::connect(&renderManager, SIGNAL(skeletonPoseLoaded(vtkSkeletonPose*)), &mainWindow, SLOT(setSkeletonPoseInformation(vtkSkeletonPose*)));
  QObject::connect(&renderManager, SIGNAL(skeletonAnimationLoaded(vtkSkeletonAnimationStack*)), &mainWindow, SLOT(setSkeletonAnimationInformation(vtkSkeletonAnimationStack*)));

  // Inform the rendering manager about option change
  QObject::connect(&mainWindow, SIGNAL(dataListRowChanged(int)), &renderManager, SLOT(onDataListRowChanged(int)));
  QObject::connect(&mainWindow, SIGNAL(animationListRowChanged(int)), &renderManager, SLOT(onAnimationListRowChanged(int)));

  // Menu File actions
  QObject::connect(&mainWindow, SIGNAL(modelFileOpened(QString)), &renderManager, SLOT(open3DModel(QString)));

  QString title = "Skinned Mesh Viewer";
  mainWindow.setWindowTitle(title);

  mainWindow.show();
  renderManager.setRenderWidget(mainWindow.getRenderWidget());

  return(app.exec());
}
