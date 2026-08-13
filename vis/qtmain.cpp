#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <vtkImageReader2Factory.h>
#include <vtkImageReader2.h>
#include <vtkCamera.h>

#include "include/qtmain.h"


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();

    // Initialize VTK components
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    renderer = vtkSmartPointer<vtkRenderer>::New();
    imageActor = vtkSmartPointer<vtkImageActor>::New();

    // Link VTK to Qt Native Widget
    vtkWidget->setRenderWindow(renderWindow);
    renderWindow->AddRenderer(renderer);
    renderer->AddActor(imageActor);
    renderer->SetBackground(0.1, 0.1, 0.1); // Dark background
}

void MainWindow::setupUI() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    layout = new QVBoxLayout(centralWidget);

    btnSelectPath = new QPushButton("Select Image Data Path", this);
    vtkWidget = new QVTKOpenGLNativeWidget(this);

    layout->addWidget(btnSelectPath);
    layout->addWidget(vtkWidget);

    // Make the VTK widget expand to fill available space
    vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(btnSelectPath, &QPushButton::clicked, this, &MainWindow::onSelectPath);
    resize(800, 600);
}

void MainWindow::onSelectPath() {
    QString filePath = QFileDialog::getOpenFileName(this, "Select Image File", "", "All Files (*.*)");
    
    if (filePath.isEmpty()) return;

    // A generic factory to read various image formats (TIFF, PNG, JPEG, etc.)
    vtkSmartPointer<vtkImageReader2Factory> readerFactory = 
        vtkSmartPointer<vtkImageReader2Factory>::New();
    vtkSmartPointer<vtkImageReader2> reader;
    reader.TakeReference(readerFactory->CreateImageReader2(filePath.toUtf8().constData()));

    if (reader) {
        reader->SetFileName(filePath.toUtf8().constData());
        reader->Update();
        
        // Pass the resulting vtkImageData to the render function
        renderImageData(reader->GetOutput());
    } else {
        QMessageBox::warning(this, "Error", "Could not read the selected image format.");
    }
}

void MainWindow::renderImageData(vtkSmartPointer<vtkImageData> imageData) {
    // Connect the image actor to the loaded data
    imageActor->SetInputData(imageData);

    // Reset camera to properly focus on the image dimensions
    renderer->ResetCamera();
    
    // Orthographic projection is usually preferred for 2D images
    renderer->GetActiveCamera()->ParallelProjectionOn();
    
    // Trigger rendering window update
    renderWindow->Render();
}



int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}