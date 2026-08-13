#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageActor.h>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onSelectPath();

private:
    void setupUI();
    void renderImageData(vtkSmartPointer<vtkImageData> imageData);

    // UI Widgets
    QWidget *centralWidget;
    QVBoxLayout *layout;
    QPushButton *btnSelectPath;
    QVTKOpenGLNativeWidget *vtkWidget;

    // VTK Pipeline components
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkImageActor> imageActor;
};
