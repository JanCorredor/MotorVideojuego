#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_set>
#include "../Primitives/GeometryGenerator.h" // Includes Primitives & Vertex
#include "../../ResourceManagement/Importers/MeshImportSettings.h"

// Declaraciones adelantadas (forward declarations) de estructuras de Assimp
// para evitar incluir cabeceras pesadas de Assimp en el .h
struct aiScene;
struct aiNode;
template<typename TReal> class aiMatrix4x4t;
typedef aiMatrix4x4t<float> aiMatrix4x4;

// Estructura que representa una parte individual de un modelo compuesto
struct SubMeshData
{
    std::string name;
    std::unique_ptr<Primitive> primitive;
};

class ModelLoader
{
public:
    // Loads a file FBX / OBJ / DAE & returns a Primitive (or an array of Primitives)
    static std::unique_ptr<Primitive> LoadFBX(const std::string& path, const MeshImportSettings& settings = MeshImportSettings());

    // Loads a file FBX / OBJ / DAE divide every SubMesh individually
    static std::vector<SubMeshData> LoadFBXMeshes(const std::string& path, const MeshImportSettings& settings = MeshImportSettings());

private:
    // Convierte una matriz 4x4 de Assimp (row-major) a formato GLM (column-major)
    static glm::mat4 AiToGlm(const aiMatrix4x4& m);

    static void ProcessNodeMerged(
        const aiNode* node,
        const aiScene* scene,
        const glm::mat4& parentTransform,
        const std::unordered_set<std::string>& ignoredNodes,
        const MeshImportSettings& settings,
        std::vector<Vertex>& outVertices,
        std::vector<unsigned int>& outIndices);

    static void ProcessNodeSubMeshes(
        const aiNode* node,
        const aiScene* scene,
        const glm::mat4& parentTransform,
        const std::unordered_set<std::string>& ignoredNodes,
        const MeshImportSettings& settings,
        const std::string& path,
        std::vector<SubMeshData>& outSubMeshes);

    // Aplica la conversión de sistema de coordenadas (Z-Up a Y-Up, flips) y la escala global
    static void ApplyAxisAndScale(glm::vec3& pos, glm::vec3& normal, const MeshImportSettings& settings);

    // Indica si el preset de ejes seleccionado invierte la orientación espacial (espejo)
    static bool IsAxisMirrored(int axisPreset);

    // Recopila los nombres de los nodos asociados a cámaras o luces para poder filtrarlos
    static std::unordered_set<std::string> CollectIgnoredNodeNames(const aiScene* scene, const MeshImportSettings& settings);
};