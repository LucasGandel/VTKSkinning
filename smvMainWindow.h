#ifndef __smvMainWindow_h
#define __smvMainWindow_h

#include <QMainWindow>

namespace Ui{
	class MainWindow;
}

class QItemSelection;

class QVTKOpenGLWidget;
class vtkPolyData;

class vtkSkeletonAnimationStack;
class vtkSkeletonHierarchy;
class vtkSkeletonPose;

class smvMainWindow : public QMainWindow
{
Q_OBJECT;
public:
	smvMainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);
	~smvMainWindow();

  QVTKOpenGLWidget* getRenderWidget();

public slots:
  void onActionOpen3DModel();

  void setMeshInformation(vtkPolyData*);
  void setHierarchyInformation(vtkSkeletonHierarchy*);
  void setSkeletonPoseInformation(vtkSkeletonPose*);
  void setSkeletonAnimationInformation(vtkSkeletonAnimationStack*);

signals:
  void modelFileOpened(QString);

  void dataListRowChanged(int);
  void animationListRowChanged(int);

private:
   Ui::MainWindow* ui;
};

#endif //__smvMainWindow_h
