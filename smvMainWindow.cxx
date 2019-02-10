#include "smvMainWindow.h"

#include "ui_smvMainWindow.h"
#include "vtkSkeletonAnimation.h"
#include "vtkSkeletonAnimationStack.h"
#include "vtkSkeletonHierarchy.h"
#include "vtkSkeletonPose.h"

#include <vtkAbstractArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkStringArray.h>

#include <QFileDialog>
#include <QItemSelection>
#include <QStandardItem>

#include <sstream>

/** Constructor */
smvMainWindow::smvMainWindow(QWidget *parent, Qt::WindowFlags flags) :QMainWindow(parent, flags)
{
   this->ui = new Ui::MainWindow();
   this->ui->setupUi(this);

   QObject::connect(this->ui->meshDataListWidget, SIGNAL(currentRowChanged(int)),
     this, SIGNAL(dataListRowChanged(int)));

   QObject::connect(this->ui->animationListWidget, SIGNAL(currentRowChanged(int)),
     this, SIGNAL(animationListRowChanged(int)));

   QObject::connect(this->ui->actionOpen3DModel, SIGNAL(triggered()), this, SLOT(onActionOpen3DModel()));
}

/** Destructor */
smvMainWindow::~smvMainWindow()
{
   delete this->ui;
}

/** Render Widget accessor */
QVTKOpenGLWidget* smvMainWindow::getRenderWidget()
{
  return this->ui->QVTKWidget;
}

/** Open 3D model. Supported file format are handled by the assimp library.
  SLot called on menu File action : Open 3D Model */
void smvMainWindow::onActionOpen3DModel()
{
  QString modelFilename = QFileDialog::getOpenFileName(this,
    tr("Open 3D model ..."), "", ""); // TODO: Filter file format.

  if (modelFilename.isEmpty())
  {
    return;
  }

  emit modelFileOpened(modelFilename);
}

/** Fill Mesh information tab from polydata
  Slot called on smvRenderManager::skinnedMeshLoaded(vtkPolyData*) */
void smvMainWindow::setMeshInformation(vtkPolyData* polydata)
{
  if (polydata == nullptr)
  {
    return;
  }

  // Mesh GroupBox information
  this->ui->nbPoints->setText(QString::number(polydata->GetNumberOfPoints()));
  this->ui->nbCells->setText(QString::number(polydata->GetNumberOfCells()));

  // Data GroupBox information
  for (int k = 0; k < polydata->GetPointData()->GetNumberOfArrays(); k++)
  {
    vtkAbstractArray* array = polydata->GetPointData()->GetAbstractArray(k);
    if (array == nullptr)
    {
      continue;
    }
    this->ui->meshDataListWidget->addItem(array->GetName());
  }
}

/** Fill Hierarchy information tab from vtkSkeletonHierarchy
  Slot called on smvRenderManager::skinnedMeshLoaded(vtkSkeletonHierarchy*) */
void smvMainWindow::setHierarchyInformation(vtkSkeletonHierarchy* hierarchy)
{
  if (hierarchy == nullptr)
  {
    return;
  }

  this->ui->hierarchyTreeNbBones->setText(QString::number(hierarchy->GetNumberOfNodes()));
  QStandardItemModel* model = new QStandardItemModel(this);

  for (int i = 0; i < hierarchy->GetNumberOfNodes(); i++)
  {
    std::string boneName = hierarchy->GetNodeNames()->GetValue(i);

    std::string parentBoneName = "";
    vtkIdType parentId = hierarchy->GetParentId(i);

    if (parentId != -1)
    {
      parentBoneName = hierarchy->GetNodeNames()->GetValue(parentId);
    }

    // Insert in hierarchy
    if (parentBoneName == "")
    {
      QTreeWidgetItem* item = new QTreeWidgetItem(this->ui->hierarchyTreeWidget);
      item->setText(0, boneName.c_str());
      this->ui->hierarchyTreeWidget->insertTopLevelItem(0, item);
      continue;
    }

    QList<QTreeWidgetItem*> itemList = this->ui->hierarchyTreeWidget->findItems(parentBoneName.c_str(), Qt::MatchExactly | Qt::MatchRecursive);
    QTreeWidgetItem* parenItem = nullptr;
    if (itemList.size() > 0)
    {
      parenItem = itemList[0];
      QTreeWidgetItem* item = new QTreeWidgetItem(parenItem);
      item->setText(0, boneName.c_str());
    }
  }
}

/** Fill Skeleton information tab from vtkSkeletonPose
  Slot called on smvRenderManager::skeletonPoseLoaded(vtkSkeletonPose*) */
void smvMainWindow::setSkeletonPoseInformation(vtkSkeletonPose* pose)
{
  if (pose == nullptr)
  {
    return;
  }

  this->ui->skeletonNbBones->setText(QString::number(pose->GetNumberOfTransforms()));
}

/** Fill Animation information tab from vtkSkeletonAnimationStack
  Slot called on smvRenderManager::skeletonAnimationLoaded(vtkSkeletonAnimationStack*) */
void smvMainWindow::setSkeletonAnimationInformation(vtkSkeletonAnimationStack* animStack)
{
  if (animStack == nullptr)
  {
    return;
  }

  for (int k = 0; k < animStack->GetNumberOfAnimations(); k++)
  {
    vtkSkeletonAnimation* anim = animStack->GetAnimation(k);
    vtkStdString animName = anim->GetAnimationName();
    double tickPerSecond = anim->GetTickPerSecond();
    int duration = anim->GetDuration();


    std::stringstream animLabel;
    animLabel << animName << std::endl <<
      " Duration (tick/s): " << duration << std::endl <<
      " Ticks/second: " << tickPerSecond;
    this->ui->animationListWidget->addItem(animLabel.str().c_str());
  }
}
