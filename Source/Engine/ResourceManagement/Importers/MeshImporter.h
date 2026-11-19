#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "../../Render/Primitives/Primitive.h"

class GameObject;

class MeshImporter
{
public:
    // Verifica si la extensión corresponde a un modelo 3D soportado (.fbx, .obj, .dae)
    static bool IsSupportedExtension(const std::string& extension);

    // Carga la geometría desde un archivo y devuelve el Primitive
    static std::unique_ptr<Primitive> ImportMesh(const std::string& filePath);

    // Importa el archivo y lo instancia directamente como GameObject en la Scene
    static GameObject* ImportToScene(const std::string& filePath, GameObject* parent = nullptr);

    ////////////////////////
    // Importer
    ///////////////////////

    // Opciones globales por defecto para nuevas importaciones de mallas
    static MeshImportSettings& GetDefaultSettings();
    static void SetDefaultSettings(const MeshImportSettings& newSettings);

    // Reimporta una primitiva desde su archivo fuente original aplicando nuevas opciones
    static std::unique_ptr<Primitive> ReimportPrimitive(const Primitive* existingPrim, const MeshImportSettings& newSettings);

    // ========================================================================
    // Custom File Format (.mesh en Library/Meshes/)
    // ========================================================================
    static std::string BuildCustomMeshPath(const std::string& sourcePath, int subMeshIndex = -1, uint64_t fallbackId = 0);
    static bool SaveMeshToCustomFormat(const Primitive* primitive, const std::string& customFilePath);
    static std::unique_ptr<Primitive> LoadMeshFromCustomFormat(const std::string& customFilePath);
    static std::unique_ptr<Primitive> ResolveOrLoadPrimitive(const std::string& sourcePath, int subMeshIndex, const std::string& customMeshPath);
};
