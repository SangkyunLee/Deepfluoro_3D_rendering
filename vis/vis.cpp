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

// Updated helper to load 2D std::vector data into a vtkMatrix4x4
void Vector2DToVtkMatrix(const std::vector<std::vector<double>>& matrix_2d, vtkSmartPointer<vtkMatrix4x4> mat) {
    for (size_t r = 0; r < matrix_2d.size() && r < 4; ++r) {
        for (size_t c = 0; c < matrix_2d[r].size() && c < 4; ++c) {
            mat->SetElement(r, c, matrix_2d[r][c]);
        }
    }
}


template <typename T>
std::vector<T> flatten3DVector(const std::vector<std::vector<std::vector<T>>>& vec3D) {
    // 1. Calculate total number of elements
    size_t totalSize = 0;
    for (const auto& matrix : vec3D) {
        for (const auto& row : matrix) {
            totalSize += row.size();
        }
    }

    // 2. Pre-allocate flat vector memory
    std::vector<T> flatVec;
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
    std::vector<uint8_t>& data_vector,
    // const std::vector<std::vector<std::vector<int>>>& vector_3d,
    const std::vector<size_t>& shape,
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




int main() {
    std::string file_path = "/home/sang/projects/data/ipcai_2020_full_res_data/ipcai_2020_full_res_data.h5";
    std::string spec_id = "17-1882";
    int proj_idx = 3;

    try {
        // Open dataset file for reading
        HighFive::File f(file_path, HighFive::File::ReadOnly);

        std::cout << "reading projection parameters...\n";
        auto proj_params_g = f.getGroup("proj-params");

        // Read intrinsic and extrinsic matrices (flattened vectors)
        std::vector<std::vector<double>> ext_data, int_data;
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

        double focal_len = std::abs((intrinsic->GetElement(0,0) * proj_col_spacing) + 
                                    (intrinsic->GetElement(1,1) * proj_row_spacing)) / 2.0;

        std::cout << "reading GT poses...\n";
        auto spec_g = f.getGroup(spec_id);
        
        // Format the projection path string (mimics Python's format)
        char proj_path[64];
        std::snprintf(proj_path, sizeof(proj_path), "projections/%03d", proj_idx);
        auto proj_g = spec_g.getGroup(proj_path);
        auto gt_poses_g = proj_g.getGroup("gt-poses");

        std::vector<std::vector<double>> p_data, lf_data, rf_data;
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

        std::cout << "reading 3D segmentation...\n";
        auto vol_seg_g = spec_g.getGroup("vol-seg");
        auto vol_seg_img_g = vol_seg_g.getGroup("image");

        // Read 3D volume pixel data
        std::vector<std::vector<std::vector<uint8_t>>> vol_seg_3d; 
        vol_seg_img_g.getDataSet("pixels").read(vol_seg_3d);
        std::vector<size_t> volume_shape = {vol_seg_3d.size(), vol_seg_3d[0].size(), vol_seg_3d[0][0].size()};
        std::vector<uint8_t> vol_seg_pix = flatten3DVector(vol_seg_3d);
        auto vtk_volume = ImportVtkFromVector(vol_seg_pix, volume_shape, true);

        



        // auto pixels_dataset = vol_seg_img_g.getDataSet("pixels");
        // std::vector<size_t> dims = pixels_dataset.getSpace().getDimensions(); // [Z, Y, X]
        // size_t total_elements = dims[0] * dims[1] * dims[2];
        // std::vector<int> vol_seg_pix(total_elements);
        // // Read directly into the flat memory buffer via pointer
        // pixels_dataset.read(vol_seg_pix);


        // auto vtk_volume = ImportVtkFromVector(vol_seg_pix, dims, true);






        // Read metadata for transformation matrix
        // std::vector<double> vol_seg_spacing, vol_seg_origin;
        std::vector<std::vector<double>> vol_seg_spacing, vol_seg_origin, vol_seg_dir_mat;
        vol_seg_img_g.getDataSet("spacing").read(vol_seg_spacing); // 3x1
        vol_seg_img_g.getDataSet("dir-mat").read(vol_seg_dir_mat); // 3x3
        vol_seg_img_g.getDataSet("origin").read(vol_seg_origin);  //3x1

        // Form transform matrix from voxel index to physical point
        auto vol_seg_idx_to_phys_pt = vtkSmartPointer<vtkMatrix4x4>::New();
        vol_seg_idx_to_phys_pt->Identity();

        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                double val = vol_seg_dir_mat[r][c] * vol_seg_spacing[c][0];
                vol_seg_idx_to_phys_pt->SetElement(r, c, val);
            }
            vol_seg_idx_to_phys_pt->SetElement(r, 3, vol_seg_origin[r][0]);
        }

        // 3D landmarks
        std::map<std::string, std::vector<double>> lands_3d;
        auto lands_3d_g = spec_g.getGroup("vol-landmarks");
        std::vector<std::string> land_names = lands_3d_g.listObjectNames();
        for (std::string land_name: land_names){
            std::vector<std::vector<double>> raw_pt;
            lands_3d_g.getDataSet(land_name).read(raw_pt);
            // lands_3d[land_name] = pelvis_vol_to_cam_proj @ (np.append(lands_3d_g[land_name][:],1)).T
            if (raw_pt.size() >= 3){
                double in[4] = {raw_pt[0][0], raw_pt[1][0], raw_pt[2][0], 1.0};
                double out[4] = {0.0, 0.0, 0.0, 0.0};
                pelvis_vol_to_cam_proj->MultiplyPoint(in, out);
                lands_3d[land_name] = std::vector<double>{out[0], out[1], out[2]};
            }
        }

        
        int a=1;






    } catch (const HighFive::Exception& err) {
        std::cerr << "HDF5 Error: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
