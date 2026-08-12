#include <iostream>
#include <string>
#include <vector>
#include <cmath>

// HDF5 Header (Using HighFive library for clean C++ syntax)
#include <highfive/H5File.hpp>

// VTK Headers
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkImageImport.h>
#include <vtkImageFlip.h>
#include <vtkImageData.h>
#include <vtkMatrixToHomogeneousTransform.h>
#include<vtkGPUVolumeRayCastMapper.h>

#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkVolumeProperty.h>
#include <vtkVolume.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCubeAxesActor.h>
#include <vtkVolumeMapper.h>
#include<vtkTextProperty.h>

using namespace std;

// Helper to invert a rigid 4x4 matrix using VTK
vtkSmartPointer<vtkMatrix4x4> InvertRigid(vtkSmartPointer<vtkMatrix4x4> mat) {
    vtkSmartPointer<vtkMatrix4x4> inverted = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Invert(mat, inverted);
    return inverted;
}

// Helper to multiply two vtkMatrix4x4 matrices
vtkSmartPointer<vtkMatrix4x4> MultiplyMatrices(vtkSmartPointer<vtkMatrix4x4> m1, vtkSmartPointer<vtkMatrix4x4> m2) {
    vtkSmartPointer<vtkMatrix4x4> result = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Multiply4x4(m1, m2, result);
    return result;
}

// Updated helper to load 2D vector data into a vtkMatrix4x4
void Vector2DToVtkMatrix(const vector<vector<double>>& matrix_2d, vtkSmartPointer<vtkMatrix4x4> mat) {
    for (size_t r = 0; r < matrix_2d.size() && r < 4; ++r) {
        for (size_t c = 0; c < matrix_2d[r].size() && c < 4; ++c) {
            mat->SetElement(r, c, matrix_2d[r][c]);
        }
    }
}


vtkSmartPointer<vtkMatrix4x4> GetTransMatrixIndex2PhysicalCoordinates(const vector<vector<double>> vol_dir_mat, 
    const vector<vector<double>> vol_spacing,
    const vector<vector<double>> vol_origin,
    const vector<size_t> dim){
    
    vtkSmartPointer<vtkMatrix4x4> vol_idx_to_phys_pt = vtkSmartPointer<vtkMatrix4x4>::New();
    vol_idx_to_phys_pt->Identity();

    for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                double val = vol_dir_mat[r][c] * vol_spacing[c][0];
                vol_idx_to_phys_pt->SetElement(r, c, val);
            }
            vol_idx_to_phys_pt->SetElement(r, 3, vol_origin[r][0]);
    }

    
    vtkSmartPointer<vtkMatrix4x4> yflip_xform = vtkSmartPointer<vtkMatrix4x4>::New();
    yflip_xform->Identity();
    yflip_xform->SetElement(1, 1, -1.0);
    yflip_xform->SetElement(1, 3, dim[1] + 1);
    vtkSmartPointer<vtkMatrix4x4> Mat = MultiplyMatrices(vol_idx_to_phys_pt, yflip_xform);

    return Mat;
}

template <typename T>
vector<T> flatten3DVector(const vector<vector<vector<T>>>& vec3D) {
    // 1. Calculate total number of elements
    size_t totalSize = 0;
    for (const auto& matrix : vec3D) {
        for (const auto& row : matrix) {
            totalSize += row.size();
        }
    }

    // 2. Pre-allocate flat vector memory
    vector<T> flatVec;
    flatVec.reserve(totalSize);

    // 3. Copy elements sequentially
    for (const auto& matrix : vec3D) {
        for (const auto& row : matrix) {
            flatVec.insert(flatVec.end(), row.begin(), row.end());
        }
    }

    return flatVec;
}

vtkSmartPointer<vtkImageData> ImportVtkFromVector(
    vector<uint8_t>& data_vector,
    // const vector<vector<vector<int>>>& vector_3d,
    const vector<size_t>& shape,
    bool y_flip = true) 
{
    if(shape.size()<3) return nullptr;

    int dim_z = static_cast<int>(shape[0]);
    int dim_y = static_cast<int>(shape[1]);
    int dim_x = static_cast<int>(shape[2]);

    auto vtk_import = vtkSmartPointer<vtkImageImport>::New();

    vtk_import->SetImportVoidPointer(data_vector.data(), false);

    vtk_import->SetDataScalarType(VTK_UNSIGNED_CHAR);
    vtk_import->SetNumberOfScalarComponents(1);

    vtk_import->SetDataExtent(0, dim_x-1, 0, dim_y-1, 0, dim_z-1);
    vtk_import->SetWholeExtent(0, dim_x-1, 0, dim_y-1, 0, dim_z-1);
    vtk_import->Update();

    if (y_flip) {
        auto flipper = vtkSmartPointer<vtkImageFlip>::New();
        flipper->SetInputData(vtk_import->GetOutput());
        flipper->SetFilteredAxis(1); // Y-axis
        flipper->FlipAboutOriginOff();
        flipper->Update();
        return flipper->GetOutput();
    }

    return vtk_import->GetOutput();

}




vtkSmartPointer<vtkImageData> XformVolume(const vtkSmartPointer<vtkImageData> VolumeData, const vtkSmartPointer<vtkMatrix4x4> Xform){


    vtkSmartPointer<vtkMatrixToHomogeneousTransform> transform = 
        vtkSmartPointer<vtkMatrixToHomogeneousTransform>::New();
    transform->SetInput(Xform);

    vtkSmartPointer<vtkImageReslice> reslice =
        vtkSmartPointer<vtkImageReslice>::New();

    reslice->SetInputData(VolumeData);
    reslice->SetResliceTransform(transform);
    reslice->AutoCropOutputOn();
    reslice->SetInterpolationModeToLinear();
    reslice->Update();

    return reslice->GetOutput();
}





void VolumeRendering(vtkVolumeMapper* volume_mapper, vtkRenderer* renderer, vtkRenderWindow* render_window, vtkRenderWindowInteractor* interactor) {

    // 3. Define color transfer function (Ensure 0-255 range mapping)
    vtkSmartPointer<vtkColorTransferFunction> color_func = vtkSmartPointer<vtkColorTransferFunction>::New();
    color_func->AddRGBPoint(0, 0.0, 0.0, 0.0);      
    color_func->AddRGBPoint(1, 0.0, 1.0, 0.0);
    color_func->AddRGBPoint(2, 1.0, 0.0, 0.0);
    color_func->AddRGBPoint(5, 0.0, 1.0, 1.0);    
    color_func->AddRGBPoint(6, 1.0, 0.5, 0.0);    

    // 4. Define broader opacity transfer function (Catching all scalar fields)
    vtkSmartPointer<vtkPiecewiseFunction> opacity_func = vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacity_func->AddPoint(0, 0.0);                 
    opacity_func->AddPoint(1, 0.2);   // Set to 0.2 instead of 0 to see low values

    // 5. Set Volume Properties
    vtkSmartPointer<vtkVolumeProperty> volume_property = vtkSmartPointer<vtkVolumeProperty>::New();
    volume_property->SetColor(color_func);
    volume_property->SetScalarOpacity(opacity_func);
    volume_property->SetInterpolationTypeToLinear();
    volume_property->ShadeOff();

    // 6. Create the Volume Actor
    vtkSmartPointer<vtkVolume> volume = vtkSmartPointer<vtkVolume>::New();
    volume->SetMapper(volume_mapper);
    volume->SetProperty(volume_property);

    // 7. Set up Rendering Pipeline
    renderer->AddViewProp(volume);


    vtkSmartPointer<vtkCubeAxesActor> cube_axes_actor = vtkSmartPointer<vtkCubeAxesActor>::New();
    renderer->AddViewProp(cube_axes_actor);
    
    // Use a bright background to verify if the window is rendering at all
    renderer->SetBackground(0.2, 0.4, 0.6); 

    
    render_window->AddRenderer(renderer);
    render_window->SetSize(1200, 1200);
    render_window->SetWindowName("VTK Smart Volume Renderer");

    // 8. Set up Interactor
    
    interactor->SetRenderWindow(render_window);

    cube_axes_actor->VisibilityOn();
    
    // Complete the incomplete snippet by setting bounds and camera
    renderer->ResetCamera();
    cube_axes_actor->SetBounds(renderer->ComputeVisiblePropBounds());
    cube_axes_actor->SetCamera(renderer->GetActiveCamera());

    cube_axes_actor->GetTitleTextProperty(0)->SetColor(1.0, 0.0, 0.0);
    cube_axes_actor->GetLabelTextProperty(0)->SetColor(1.0, 0.0, 0.0);
    cube_axes_actor->GetTitleTextProperty(1)
        ->SetColor(0.0, 1.0, 0.0);
    cube_axes_actor->GetLabelTextProperty(1)
        ->SetColor(0.0, 1.0, 0.0);
    cube_axes_actor->GetTitleTextProperty(2)
        ->SetColor(0.0, 0.0, 1.0);
    cube_axes_actor->GetLabelTextProperty(2)
        ->SetColor(0.0, 0.0, 1.0);

    cube_axes_actor->DrawXGridlinesOn();
    cube_axes_actor->DrawYGridlinesOn();
    cube_axes_actor->DrawZGridlinesOn();

    // cube_axes_actor->SetGridLineLocation(vtkCubeAxesActor->VTK_GRID_LINES_FURTHEST)

    render_window->Render();
    interactor->Start();
}




int main() {
    string file_path = "/home/sang/projects/data/ipcai_2020_full_res_data/ipcai_2020_full_res_data.h5";
    string spec_id = "17-1882";
    int proj_idx = 3;

    try {
        // Open dataset file for reading
        HighFive::File f(file_path, HighFive::File::ReadOnly);

        cout << "reading projection parameters...\n";
        auto proj_params_g = f.getGroup("proj-params");

        // Read intrinsic and extrinsic matrices (flattened vectors)
        vector<vector<double>> ext_data, int_data;
        proj_params_g.getDataSet("extrinsic").read(ext_data);
        proj_params_g.getDataSet("intrinsic").read(int_data);

        auto extrinsic = vtkSmartPointer<vtkMatrix4x4>::New();
        auto intrinsic = vtkSmartPointer<vtkMatrix4x4>::New();
        Vector2DToVtkMatrix(ext_data, extrinsic);
        Vector2DToVtkMatrix(int_data, intrinsic);

        double proj_num_cols = 0, proj_num_rows = 0, proj_col_spacing = 0, proj_row_spacing = 0;
        proj_params_g.getDataSet("num-cols").read(proj_num_cols);
        proj_params_g.getDataSet("num-rows").read(proj_num_rows);
        proj_params_g.getDataSet("pixel-col-spacing").read(proj_col_spacing);
        proj_params_g.getDataSet("pixel-row-spacing").read(proj_row_spacing);

        double focal_len = abs((intrinsic->GetElement(0,0) * proj_col_spacing) + 
                                    (intrinsic->GetElement(1,1) * proj_row_spacing)) / 2.0;

        cout << "reading GT poses...\n";
        auto spec_g = f.getGroup(spec_id);
        
        // Format the projection path string (mimics Python's format)
        char proj_path[64];
        snprintf(proj_path, sizeof(proj_path), "projections/%03d", proj_idx);
        auto proj_g = spec_g.getGroup(proj_path);
        auto gt_poses_g = proj_g.getGroup("gt-poses");

        vector<vector<double>> p_data, lf_data, rf_data;
        gt_poses_g.getDataSet("cam-to-pelvis-vol").read(p_data);
        gt_poses_g.getDataSet("cam-to-left-femur-vol").read(lf_data);
        gt_poses_g.getDataSet("cam-to-right-femur-vol").read(rf_data);

        auto cam_to_pelvis_vol = vtkSmartPointer<vtkMatrix4x4>::New();
        auto cam_to_left_femur_vol = vtkSmartPointer<vtkMatrix4x4>::New();
        auto cam_to_right_femur_vol = vtkSmartPointer<vtkMatrix4x4>::New();
        Vector2DToVtkMatrix(p_data, cam_to_pelvis_vol);
        Vector2DToVtkMatrix(lf_data, cam_to_left_femur_vol);
        Vector2DToVtkMatrix(rf_data, cam_to_right_femur_vol);

        // Invert and multiply transforms
        auto pelvis_vol_to_cam_proj = MultiplyMatrices(extrinsic, InvertRigid(cam_to_pelvis_vol));
        auto left_femur_vol_to_cam_proj = MultiplyMatrices(extrinsic, InvertRigid(cam_to_left_femur_vol));
        auto right_femur_vol_to_cam_proj = MultiplyMatrices(extrinsic, InvertRigid(cam_to_right_femur_vol));

        cout << "reading 3D segmentation...\n";
        auto vol_seg_g = spec_g.getGroup("vol-seg");
        auto vol_seg_img_g = vol_seg_g.getGroup("image");

        // Read 3D volume pixel data
        vector<vector<vector<uint8_t>>> vol_seg_3d; 
        vol_seg_img_g.getDataSet("pixels").read(vol_seg_3d);
        vector<size_t> volume_shape = {vol_seg_3d.size(), vol_seg_3d[0].size(), vol_seg_3d[0][0].size()};
        vector<uint8_t> vol_seg_pix = flatten3DVector(vol_seg_3d);

        vtkSmartPointer<vtkImageData> vtk_volume;
        vtk_volume = ImportVtkFromVector(vol_seg_pix, volume_shape, true);

        // Read metadata for transformation matrix
        vector<vector<double>> vol_seg_spacing, vol_seg_origin, vol_seg_dir_mat;
        vol_seg_img_g.getDataSet("spacing").read(vol_seg_spacing); // 3x1
        vol_seg_img_g.getDataSet("dir-mat").read(vol_seg_dir_mat); // 3x3
        vol_seg_img_g.getDataSet("origin").read(vol_seg_origin);  //3x1

        auto pixels_dataset = vol_seg_img_g.getDataSet("pixels");
        vector<size_t> dims = pixels_dataset.getSpace().getDimensions(); // [Z, Y, X]
  

        auto vol_idx_to_phys = GetTransMatrixIndex2PhysicalCoordinates(vol_seg_dir_mat, vol_seg_spacing, vol_seg_origin, dims);

        vtkSmartPointer<vtkMatrix4x4> PhysToVolIdx = vtkSmartPointer<vtkMatrix4x4>::New();
        vtkMatrix4x4::Invert(vol_idx_to_phys, PhysToVolIdx);
        vtkSmartPointer<vtkMatrix4x4> InvMat1 = vtkSmartPointer<vtkMatrix4x4>::New();
        vtkMatrix4x4::Invert(pelvis_vol_to_cam_proj, InvMat1);

        vtk_volume = XformVolume(vtk_volume, PhysToVolIdx);
        vtk_volume = XformVolume(vtk_volume, InvMat1);



        // 3D landmarks
        map<string, vector<double>> lands_3d;
        auto lands_3d_g = spec_g.getGroup("vol-landmarks");
        vector<string> land_names = lands_3d_g.listObjectNames();
        for (string land_name: land_names){
            vector<vector<double>> raw_pt;
            lands_3d_g.getDataSet(land_name).read(raw_pt);
            // lands_3d[land_name] = pelvis_vol_to_cam_proj @ (np.append(lands_3d_g[land_name][:],1)).T
            if (raw_pt.size() >= 3){
                double in[4] = {raw_pt[0][0], raw_pt[1][0], raw_pt[2][0], 1.0};
                double out[4] = {0.0, 0.0, 0.0, 0.0};
                pelvis_vol_to_cam_proj->MultiplyPoint(in, out);
                lands_3d[land_name] = vector<double>{out[0], out[1], out[2]};
            }
        }

        vtkSmartPointer<vtkGPUVolumeRayCastMapper> VolumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
        VolumeMapper->SetInputData(vtk_volume);

        vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
        vtkSmartPointer<vtkRenderWindow> render_window = vtkSmartPointer<vtkRenderWindow>::New();
        vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
        VolumeRendering(VolumeMapper, renderer, render_window, interactor);

        int a=1;






    } catch (const HighFive::Exception& err) {
        cerr << "HDF5 Error: " << err.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
