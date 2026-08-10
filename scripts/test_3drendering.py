
import numpy as np
import h5py as h5
from vtk import VTK_UNSIGNED_CHAR
import vtk


def invert_rigid(H):
    H_inv = np.eye(4)
    R_inv = H[0:3,0:3].T
    H_inv[0:3,0:3] = R_inv
    H_inv[0:3,3]   = R_inv @ -H[0:3,3]

    return H_inv




def import_vtk_from_numpy(nparray, yflip=True):
    vtk_import = vtk.vtkImageImport()
    vtk_import.SetImportVoidPointer(nparray, True)
    vtk_import.SetDataScalarType(VTK_UNSIGNED_CHAR)
    vtk_import.SetNumberOfScalarComponents(1)

    vtk_import.SetDataExtent(0, nparray.shape[2] - 1,
                                0, nparray.shape[1] - 1,
                                0, nparray.shape[0] - 1)

    vtk_import.SetWholeExtent(0, nparray.shape[2] - 1,
                                0, nparray.shape[1] - 1,
                                0, nparray.shape[0] - 1)

    vtk_import.Update()
    if yflip:
        flipper = vtk.vtkImageFlip()
        flipper.SetInputData(vtk_import.GetOutput())
        flipper.SetFilteredAxis(1)
        flipper.FlipAboutOriginOff()
        flipper.Update()
        vtk_import = flipper

    return vtk_import.GetOutput()


def xform_volume(volume_data, xform_4x4):
    xform_4x4_vtk = vtk.vtkMatrix4x4()
    for i in range(4):
        for j in range(4):
            xform_4x4_vtk.SetElement(i,j,xform_4x4[i,j])

    transform = vtk.vtkMatrixToHomogeneousTransform()
    transform.SetInput(xform_4x4_vtk)


    # Reslice the volume data
    reslice = vtk.vtkImageReslice()
    reslice.SetInputData(volume_data)
    reslice.SetResliceTransform(transform)
    # Automatically calculate new bounds so data isn't cut off
    reslice.AutoCropOutputOn() 
    reslice.SetInterpolationModeToLinear() # or NearestNeighbor
    reslice.Update()

    # Get the newly transformed volume data
    transformed_data = reslice.GetOutput()
    return transformed_data


def add_sphere(renderer, pt, r, g, b, radius):
    sphere_src = vtk.vtkSphereSource()
    sphere_src.SetCenter(pt[0], pt[1], pt[2])
    sphere_src.SetThetaResolution(20)
    sphere_src.SetPhiResolution(20)
    sphere_src.SetRadius(radius)
    sphere_src.Update()

    mapper = vtk.vtkPolyDataMapper()
    mapper.SetInputData(sphere_src.GetOutput())

    actor = vtk.vtkActor()
    actor.SetMapper(mapper)
    actor.GetProperty().SetColor(r,g,b)
    
    renderer.AddViewProp(actor)


def volume_ren(volume_mapper, lands_3d):


    # 3. Define color transfer function (Ensure 0-255 range mapping)
    color_func = vtk.vtkColorTransferFunction()
    color_func.AddRGBPoint(0, 0.0, 0.0, 0.0)      
    color_func.AddRGBPoint(1, 0.0, 1.0, 0.0)
    color_func.AddRGBPoint(2, 1.0, 0.0, 0.0)
    color_func.AddRGBPoint(5, 0.0, 1.0, 1.0)    
    color_func.AddRGBPoint(6, 1.0, 0.5, 0.0)    

    # 4. Define broader opacity transfer function (Catching all scalar fields)
    opacity_func = vtk.vtkPiecewiseFunction()
    opacity_func.AddPoint(0, 0.0)                 
    opacity_func.AddPoint(1, 0.2)   # Set to 0.2 instead of 0 to see low values
    # opacity_func.AddPoint(127, 0.5)               
    # opacity_func.AddPoint(255, 0.8)               

    # 5. Set Volume Properties
    volume_property = vtk.vtkVolumeProperty()
    volume_property.SetColor(color_func)
    volume_property.SetScalarOpacity(opacity_func)
    volume_property.SetInterpolationTypeToLinear()
    volume_property.ShadeOff()

    # 6. Create the Volume Actor
    volume = vtk.vtkVolume()


    volume.SetMapper(volume_mapper)
    volume.SetProperty(volume_property)

    # 7. Set up Rendering Pipeline
    renderer = vtk.vtkRenderer()
    renderer.AddViewProp(volume)

    for cur_land in lands_3d.values():
        add_sphere(renderer, cur_land, 0.5, 0.0, 0.5, 5)
    add_sphere(renderer,[0,0,0], 0.0, 1.0, 0.0, 10)

    cube_axes_actor = vtk.vtkCubeAxesActor()
    renderer.AddViewProp(cube_axes_actor)
    
    # Use a bright background to verify if the window is rendering at all
    renderer.SetBackground(0.2, 0.4, 0.6) 

    render_window = vtk.vtkRenderWindow()
    render_window.AddRenderer(renderer)
    render_window.SetSize(1200, 1200)
    render_window.SetWindowName("VTK Smart Volume Renderer")

    # 8. Set up Interactor
    interactor = vtk.vtkRenderWindowInteractor()
    interactor.SetRenderWindow(render_window)

    cube_axes_actor.VisibilityOn()
    cube_axes_actor.SetBounds(renderer.ComputeVisiblePropBounds())
    cube_axes_actor.SetCamera(renderer.GetActiveCamera())
    cube_axes_actor.GetTitleTextProperty(0).SetColor(1.0, 0.0, 0.0)
    cube_axes_actor.GetLabelTextProperty(0).SetColor(1.0, 0.0, 0.0)
    cube_axes_actor.GetTitleTextProperty(1).SetColor(0.0, 1.0, 0.0)
    cube_axes_actor.GetLabelTextProperty(1).SetColor(0.0, 1.0, 0.0)
    cube_axes_actor.GetTitleTextProperty(2).SetColor(0.0, 0.0, 1.0)
    cube_axes_actor.GetLabelTextProperty(2).SetColor(0.0, 0.0, 1.0)

    cube_axes_actor.DrawXGridlinesOn()
    cube_axes_actor.DrawYGridlinesOn()
    cube_axes_actor.DrawZGridlinesOn()

    cube_axes_actor.SetGridLineLocation(vtk.vtkCubeAxesActor.VTK_GRID_LINES_FURTHEST)

    cube_axes_actor.XAxisMinorTickVisibilityOff()
    cube_axes_actor.YAxisMinorTickVisibilityOff()
    cube_axes_actor.ZAxisMinorTickVisibilityOff()
    return renderer, render_window, interactor

    
##################################



file_path = "/home/sang/projects/data/ipcai_2020_full_res_data/ipcai_2020_full_res_data.h5"

spec_id = "17-1882"    
proj_idx = 3

# open dataset file for reading
f = h5.File(file_path, 'r')

print('reading projection parameters...')
proj_params_g = f['proj-params']
extrinsic = proj_params_g['extrinsic'][:] # world to camera
intrinsic = proj_params_g['intrinsic'][:] # project camera to image plane

proj_num_cols    = proj_params_g['num-cols'][()]
proj_num_rows    = proj_params_g['num-rows'][()]
proj_col_spacing = proj_params_g['pixel-col-spacing'][()]
proj_row_spacing = proj_params_g['pixel-row-spacing'][()]

focal_len = abs((intrinsic[0,0] * proj_col_spacing) + (intrinsic[1,1] * proj_row_spacing)) / 2.0


print('reading GT poses...')
spec_g = f[spec_id]
proj_g = spec_g['projections/{:03d}'.format(proj_idx)]
gt_poses_g = proj_g['gt-poses']

cam_to_pelvis_vol      = gt_poses_g['cam-to-pelvis-vol'][:]
cam_to_left_femur_vol  = gt_poses_g['cam-to-left-femur-vol'][:]
cam_to_right_femur_vol = gt_poses_g['cam-to-right-femur-vol'][:]

pelvis_vol_to_cam_proj      = extrinsic @ invert_rigid(cam_to_pelvis_vol)
left_femur_vol_to_cam_proj  = extrinsic @ invert_rigid(cam_to_left_femur_vol)
right_femur_vol_to_cam_proj = extrinsic @ invert_rigid(cam_to_right_femur_vol)



print('reading 3D segmentation...')

# read in the 3D volume segmentation
vol_seg_g = spec_g['vol-seg']
vol_seg_img_g = vol_seg_g['image']
vol_seg_pix = vol_seg_img_g['pixels'][:]


# form transformation mamtrix from voxel index to world coordinate
vol_seg_spacing = vol_seg_img_g['spacing'][:]
vol_seg_dir_mat = vol_seg_img_g['dir-mat'][:]
vol_seg_origin  = vol_seg_img_g['origin'][:]

vol_seg_idx_to_phys_pt = np.eye(4)
for r in range(3):
    for c in range(3):
        vol_seg_idx_to_phys_pt[r,c] = vol_seg_dir_mat[r,c] * vol_seg_spacing[c,0]
    vol_seg_idx_to_phys_pt[r,3] = vol_seg_origin[r,0]




lands_3d = {}

lands_3d_g = spec_g['vol-landmarks']
for land_name in lands_3d_g:
    lands_3d[land_name] = pelvis_vol_to_cam_proj @ (np.append(lands_3d_g[land_name][:],1)).T

vtk_volume = import_vtk_from_numpy(vol_seg_pix)

# generate transformation matrix from pixel index to physical coordinate
# flip in y-axis
yflip_xform = np.eye(4)
yflip_xform[1,1] = -1
yflip_xform[1,3] = vol_seg_pix.shape[1] + 1
inds_phys = vol_seg_idx_to_phys_pt @ yflip_xform


#-----------------------------------------------------
# vtkImageReslice applies the transformation to the output voxel coordinates 
# to sample the input volume. This means it operates in reverse (inverse) mode. 
vtk_volume = xform_volume(vtk_volume, np.linalg.inv(inds_phys))
newform = np.linalg.inv(pelvis_vol_to_cam_proj)
vtk_volume = xform_volume(vtk_volume, newform)




volume_mapper = vtk.vtkGPUVolumeRayCastMapper()
# volume_mapper = vtk.vtkSmartVolumeMapper()
volume_mapper.SetInputData(vtk_volume)
renderer, render_window, interactor = volume_ren(volume_mapper, lands_3d)


# Auto-adjust camera view to point directly at the data volume
renderer.ResetCamera()

# Print active rendering mode to the console for debugging
# print(f"Current Render Mode: {volume_mapper.GetLastUsedRenderMode()}")

render_window.Render()
interactor_style = vtk.vtkInteractorStyleTrackballCamera()
interactor.SetInteractorStyle(interactor_style)
interactor.Start()

# ---------- close window ------------
# render_window.Finalize()
