#ifndef MESH_IMPORT_SETTINGS_DEFINED
#define MESH_IMPORT_SETTINGS_DEFINED
struct MeshImportSettings
{
    float globalScale = 1.0f;
    // 0: Estándar (+X, +Y, +Z)
    // 1: Z-Up a Y-Up (+X, +Z, -Y) [ej. Blender / 3ds Max]
    // 2: Invertir eje Z (+X, +Y, -Z)
    // 3: Invertir eje X (-X, +Y, +Z)
    int axisPreset = 0;
    bool ignoreCameras = true;
    bool ignoreLights = true;
};
#endif