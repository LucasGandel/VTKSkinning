#ifndef __smvRenderManager_h
#define __smvRenderManager_h

#include <QObject>

class QVTKOpenGLWidget;
class vtkOrientationMarkerWidget;
class vtkPolyData;

class vtkSkeletonAnimationStack;
class vtkSkeletonHierarchy;
class vtkSkeletonPolyDataMapper;
class vtkSkeletonPose;

class smvRenderManager : public QObject
{
  Q_OBJECT;

public:
  smvRenderManager(QObject *parent = 0);
  ~smvRenderManager();

  void setRenderWidget(QVTKOpenGLWidget*);

public slots:
  void open3DModel(QString fileName);

  void onDataListRowChanged(int);
  void onAnimationListRowChanged(int);

signals:
  void skinnedMeshLoaded(vtkPolyData*);
  void hierarchyLoaded(vtkSkeletonHierarchy*);
  void skeletonPoseLoaded(vtkSkeletonPose*);
  void skeletonAnimationLoaded(vtkSkeletonAnimationStack*);

private:
  QVTKOpenGLWidget* RenderWidget;
  vtkOrientationMarkerWidget* OrientationAxesWidget;

  vtkPolyData* Mesh;
  vtkSkeletonPolyDataMapper* Mapper;
};

#endif //__smvRenderManager_h
