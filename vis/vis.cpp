#include <iostream>
#include <string>
#include <vector>
#include <cmath>

// HDF5 Header (Using HighFive library for clean C++ syntax)
#include <highfive/H5File.hpp>

// VTK Headers
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>

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
        std::vector<std::vector<std::vector<int>>> vol_seg_pix; 
        vol_seg_img_g.getDataSet("pixels").read(vol_seg_pix);

        // Read metadata for transformation matrix
        std::vector<double> vol_seg_spacing, vol_seg_origin;
        std::vector<std::vector<double>> vol_seg_dir_mat;
        vol_seg_img_g.getDataSet("spacing").read(vol_seg_spacing);
        vol_seg_img_g.getDataSet("dir-mat").read(vol_seg_dir_mat);
        vol_seg_img_g.getDataSet("origin").read(vol_seg_origin);

        // Form transform matrix from voxel index to physical point
        auto vol_seg_idx_to_phys_pt = vtkSmartPointer<vtkMatrix4x4>::New();
        vol_seg_idx_to_phys_pt->Identity();

        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                double val = vol_seg_dir_mat[r][c] * vol_seg_spacing[c];
                vol_seg_idx_to_phys_pt->SetElement(r, c, val);
            }
            vol_seg_idx_to_phys_pt->SetElement(r, 3, vol_seg_origin[r]);
        }

    } catch (const HighFive::Exception& err) {
        std::cerr << "HDF5 Error: " << err.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
