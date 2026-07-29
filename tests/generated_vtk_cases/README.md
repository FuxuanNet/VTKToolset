# Generated VTK Test Cases

These files are Legacy ASCII `.vtk` unstructured-grid cases for VTKToolset validation.

Run:

```powershell
python VTKToolset\tests\generate_vtk_cases.py
python VTKToolset\tests\verify_generated_vtk_cases.py
```

## Case List

| File | Coverage |
|---|---|
| `01_beam_rod_line_static.vtk` | Line elements for beam/rod style models; includes `U`, `UR`, `S`, `E`, stress components, strain components, `S_Mises`, `S_pressure`. |
| `02_shell_tri_quad_planar.vtk` | Triangle and quadrilateral shell elements; plane-stress style data with zero out-of-plane components. |
| `03_solid_tetra_wedge_hex.vtk` | Solid tetrahedron, wedge and hexahedron elements; full 3D stress and strain components. |
| `04_mixed_beam_shell_solid.vtk` | Mixed model containing line, triangle shell, quad shell and hexahedron cells. |
| `05_compression_pressure.vtk` | Compression-dominant solid data; verifies positive pressure from negative normal stresses. |
| `06_shear_dominant.vtk` | Shear-dominant stress data; verifies `S_Mises` with strong shear components. |
| `07_shell_zero_out_of_plane.vtk` | Shell-only case; verifies `S33`, `S23`, `S13`, `E33`, `E23`, `E13` can be present as zero components. |
| `08_quadratic_elements.vtk` | Higher-order VTK cell types: quadratic edge, triangle, quad and tetra. |
| `09_dense_many_elements.vtk` | Denser mixed mesh; checks multiple cells and repeated field rows. |
| `10_zero_and_extreme_fields.vtk` | Zero values and high-contrast values; checks robustness of scalar and tensor output. |

## Required Fields

Every case contains:

- `POINT_DATA`: `U`, `UR`
- `CELL_DATA`: `S`, `E`
- stress components: `S11`, `S22`, `S33`, `S12`, `S23`, `S13`
- derived stress fields: `S_Mises`, `S_pressure`
- strain components: `E11`, `E22`, `E33`, `E12`, `E23`, `E13`

The verification script checks mesh counts, VTK cell types, required fields, `S_Mises` formula and `S_pressure` formula.
