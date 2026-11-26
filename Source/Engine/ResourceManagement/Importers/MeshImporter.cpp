#include "MeshImporter.h"
#include "../../Engine.h"
#include "../../Scene/Scene.h"
#include "../../Scene/GameObjects/GameObject.h"
#include "../../Scene/GameObjects/Components/TransformComponent.h"
#include "../../Scene/GameObjects/Components/MeshComponent.h"
#include "../../Scene/GameObjects/Components/MaterialComponent.h"
#include "../../Render/3DModels/ModelLoader.h"
#include "../../Render/Primitives/GeometryGenerator.h"
#include "../../../Utils/Log.h"
#include "../../FileSystem/FileSystem.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    std::string ResolveCustomMeshDiskPath(const std::string& customFilePath)
    {
        if (customFilePath.empty()) return "";
        std::string norm = customFilePath;
        std::replace(norm.begin(), norm.end(), '\\', '/');
        std::filesystem::path p(norm);
        if (p.is_absolute()) return norm;

        std::string stripped = norm;
        while (stripped.rfind("../", 0) == 0) stripped = stripped.substr(3);
        while (stripped.rfind("./", 0) == 0) stripped = stripped.substr(2);
        return FileSystem::ToDiskPath(stripped);
    }

    bool ImportSettingsEqual(const MeshImportSettings& a, const MeshImportSettings& b)
    {
        return std::fabs(a.globalScale - b.globalScale) < 1e-5f &&
               a.axisPreset == b.axisPreset &&
               a.ignoreCameras == b.ignoreCameras &&
               a.ignoreLights == b.ignoreLights;
    }
}

bool MeshImporter::IsSupportedExtension(const std::string& extension)
{
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".fbx" || ext == ".obj" || ext == ".dae");
}

std::string MeshImporter::BuildCustomMeshPath(const std::string& sourcePath, int subMeshIndex, uint64_t fallbackId)
{
    // Asegurar que la carpeta Library/Meshes existe
    FileSystem::CreateDirectories(FileSystem::LIBRARY_MESHES_DIR);
    const std::string libRel = "Library/Meshes";

    if (sourcePath.rfind("Primitive:", 0) == 0)
    {
        std::string primName = sourcePath.substr(10);
        if (primName.empty()) primName = "Cube";
        return libRel + "/Primitive_" + primName + ".mesh";
    }

    if (!sourcePath.empty())
    {
        std::string norm = sourcePath;
        std::replace(norm.begin(), norm.end(), '\\', '/');
        std::filesystem::path p(norm);
        std::string stem = p.stem().string();
        if (stem.empty()) stem = "ImportedMesh";
        if (subMeshIndex >= 0)
        {
            return libRel + "/" + stem + "_sub" + std::to_string(subMeshIndex) + ".mesh";
        }
        return libRel + "/" + stem + ".mesh";
    }

    return libRel + "/CustomMesh_" + std::to_string(fallbackId) + ".mesh";
}

bool MeshImporter::SaveMeshToCustomFormat(const Primitive* primitive, const std::string& customFilePath)
{
    if (!primitive || customFilePath.empty()) return false;

    std::string diskPath = ResolveCustomMeshDiskPath(customFilePath);
    std::filesystem::path targetPath(diskPath);
    if (targetPath.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(targetPath.parent_path(), ec);
    }

    std::ofstream out(diskPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        LOG_ERROR("[MeshImporter] No se pudo guardar el Custom File Format de malla: '%s'", diskPath.c_str());
        return false;
    }

    const std::vector<Vertex>& vertices = primitive->GetVertices();
    const std::vector<unsigned int>& indices = primitive->GetIndices();

    // Header del Custom File Format (.mesh v2): magic + ranges[2] + AABB bounds + MeshImportSettings
    const char magic[8] = { 'M', 'V', 'M', 'E', 'S', 'H', '0', '2' };
    uint32_t ranges[2] = {
        static_cast<uint32_t>(vertices.size()),
        static_cast<uint32_t>(indices.size())
    };
    glm::vec3 bMin = primitive->GetBoundsMin();
    glm::vec3 bMax = primitive->GetBoundsMax();
    const MeshImportSettings& imp = primitive->GetImportSettings();
    uint8_t impFlags[2] = {
        static_cast<uint8_t>(imp.ignoreCameras ? 1 : 0),
        static_cast<uint8_t>(imp.ignoreLights ? 1 : 0)
    };

    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char*>(ranges), sizeof(ranges));
    out.write(reinterpret_cast<const char*>(&bMin), sizeof(bMin));
    out.write(reinterpret_cast<const char*>(&bMax), sizeof(bMax));
    out.write(reinterpret_cast<const char*>(&imp.globalScale), sizeof(imp.globalScale));
    out.write(reinterpret_cast<const char*>(&imp.axisPreset), sizeof(imp.axisPreset));
    out.write(reinterpret_cast<const char*>(impFlags), sizeof(impFlags));

    if (ranges[0] > 0)
    {
        out.write(reinterpret_cast<const char*>(vertices.data()), ranges[0] * sizeof(Vertex));
    }
    if (ranges[1] > 0)
    {
        out.write(reinterpret_cast<const char*>(indices.data()), ranges[1] * sizeof(unsigned int));
    }

    bool ok = out.good();
    out.close();
    return ok;
}

std::unique_ptr<Primitive> MeshImporter::LoadMeshFromCustomFormat(const std::string& customFilePath)
{
    if (customFilePath.empty()) return nullptr;

    std::string resolvedPath = FileSystem::ResolveExistingPath(customFilePath);
    std::error_code ec;
    if (!std::filesystem::exists(resolvedPath, ec) || !std::filesystem::is_regular_file(resolvedPath, ec))
    {
        return nullptr;
    }

    std::ifstream in(resolvedPath, std::ios::binary);
    if (!in.is_open())
    {
        return nullptr;
    }

    char magic[8] = {};
    in.read(magic, sizeof(magic));
    if (!in) return nullptr;

    const char magicV1[8] = { 'M', 'V', 'M', 'E', 'S', 'H', '0', '1' };
    const char magicV2[8] = { 'M', 'V', 'M', 'E', 'S', 'H', '0', '2' };
    const bool isV1 = (std::memcmp(magic, magicV1, 8) == 0);
    const bool isV2 = (std::memcmp(magic, magicV2, 8) == 0);

    if (!isV1 && !isV2)
    {
        LOG_ERROR("[MeshImporter] Cabecera inválida en archivo Custom File Format: '%s'", resolvedPath.c_str());
        return nullptr;
    }

    uint32_t ranges[2] = { 0, 0 };
    glm::vec3 bMin(0.0f);
    glm::vec3 bMax(0.0f);
    in.read(reinterpret_cast<char*>(ranges), sizeof(ranges));
    in.read(reinterpret_cast<char*>(&bMin), sizeof(bMin));
    in.read(reinterpret_cast<char*>(&bMax), sizeof(bMax));

    MeshImportSettings loadedSettings;
    if (isV2)
    {
        uint8_t impFlags[2] = { 1, 1 };
        in.read(reinterpret_cast<char*>(&loadedSettings.globalScale), sizeof(loadedSettings.globalScale));
        in.read(reinterpret_cast<char*>(&loadedSettings.axisPreset), sizeof(loadedSettings.axisPreset));
        in.read(reinterpret_cast<char*>(impFlags), sizeof(impFlags));
        loadedSettings.ignoreCameras = (impFlags[0] != 0);
        loadedSettings.ignoreLights = (impFlags[1] != 0);
    }

    if (!in || ranges[0] == 0 || ranges[1] == 0)
    {
        return nullptr;
    }

    std::vector<Vertex> vertices(ranges[0]);
    in.read(reinterpret_cast<char*>(vertices.data()), ranges[0] * sizeof(Vertex));

    std::vector<unsigned int> indices(ranges[1]);
    in.read(reinterpret_cast<char*>(indices.data()), ranges[1] * sizeof(unsigned int));

    if (!in)
    {
        LOG_WARNING("[MeshImporter] Archivo Custom File Format corrupto o incompleto: '%s'", resolvedPath.c_str());
        return nullptr;
    }
    in.close();

    auto prim = std::make_unique<Primitive>();
    prim->Init(std::move(vertices), std::move(indices));
    prim->SetCustomMeshPath(FileSystem::ToProjectRelativePath(resolvedPath));
    prim->SetImportSettings(loadedSettings);
    return prim;
}

std::unique_ptr<Primitive> MeshImporter::ResolveOrLoadPrimitive(const std::string& sourcePath, int subMeshIndex, const std::string& customMeshPath,
    const MeshImportSettings& importSettings, bool hasExplicitImportSettings)
{
    std::string normalizedSource = FileSystem::ToProjectRelativePath(sourcePath);
    std::string targetCustomPath = customMeshPath.empty()
        ? BuildCustomMeshPath(normalizedSource, subMeshIndex)
        : FileSystem::ToProjectRelativePath(customMeshPath);

    // 1. Intentar cargar directamente desde el Custom File Format (.mesh en Library/Meshes/)
    std::unique_ptr<Primitive> loadedCustom = LoadMeshFromCustomFormat(targetCustomPath);
    if (loadedCustom)
    {
        bool settingsMatch = !hasExplicitImportSettings || ImportSettingsEqual(loadedCustom->GetImportSettings(), importSettings);
        if (settingsMatch)
        {
            // Si el archivo fuente existe dentro del proyecto, asegurarnos de guardar su ruta relativa limpia
            std::string existingSource = FileSystem::ResolveExistingPath(normalizedSource);
            std::error_code srcEc;
            if (!normalizedSource.empty() && normalizedSource.rfind("Primitive:", 0) != 0 && std::filesystem::exists(existingSource, srcEc))
            {
                normalizedSource = FileSystem::EnsureAssetInProject(existingSource, "Models");
            }
            loadedCustom->SetSourceInfo(normalizedSource, subMeshIndex);
            loadedCustom->SetCustomMeshPath(targetCustomPath);
            if (hasExplicitImportSettings)
            {
                loadedCustom->SetImportSettings(importSettings);
            }
            return loadedCustom;
        }
    }

    // 2. Fallback: regenerar desde sourcePath (primitiva procedural o archivo FBX/OBJ/DAE)
    std::unique_ptr<Primitive> fallbackPrim = nullptr;
    if (normalizedSource == "Primitive:Cube")          fallbackPrim = GeometryGenerator::CreateCube(1.0f);
    else if (normalizedSource == "Primitive:Sphere")   fallbackPrim = GeometryGenerator::CreateSphere(0.5f, 20, 20);
    else if (normalizedSource == "Primitive:Plane")    fallbackPrim = GeometryGenerator::CreatePlane(1.0f);
    else if (normalizedSource == "Primitive:Cylinder") fallbackPrim = GeometryGenerator::CreateCylinder(0.5f, 1.0f, 32);
    else if (normalizedSource == "Primitive:Pyramid")  fallbackPrim = GeometryGenerator::CreatePyramid(1.0f, 1.0f);
    else if (!normalizedSource.empty())
    {
        std::string resolvedSource = FileSystem::ResolveExistingPath(normalizedSource);
        std::error_code ec;
        if (!std::filesystem::exists(resolvedSource, ec) || !std::filesystem::is_regular_file(resolvedSource, ec))
        {
            LOG_ERROR("[MeshImporter] No se encontró el archivo de malla '%s' (ni en Library/Meshes ni en Assets/Models).", normalizedSource.c_str());
            return nullptr;
        }

        // Asegurar que el archivo está copiado dentro de Assets/Models si venía de fuera
        normalizedSource = FileSystem::EnsureAssetInProject(resolvedSource, "Models");
        resolvedSource = FileSystem::ResolveExistingPath(normalizedSource);
        targetCustomPath = BuildCustomMeshPath(normalizedSource, subMeshIndex);

        if (subMeshIndex >= 0)
        {
            std::vector<SubMeshData> subMeshes = ModelLoader::LoadFBXMeshes(resolvedSource, importSettings);
            for (size_t i = 0; i < subMeshes.size(); ++i)
            {
                if (subMeshes[i].primitive)
                {
                    std::string subCustom = BuildCustomMeshPath(normalizedSource, (int)i);
                    subMeshes[i].primitive->SetSourceInfo(normalizedSource, (int)i);
                    subMeshes[i].primitive->SetCustomMeshPath(subCustom);
                    subMeshes[i].primitive->SetImportSettings(importSettings);
                    SaveMeshToCustomFormat(subMeshes[i].primitive.get(), subCustom);
                }
            }
            if (static_cast<size_t>(subMeshIndex) < subMeshes.size())
            {
                fallbackPrim = std::move(subMeshes[subMeshIndex].primitive);
            }
        }
        else
        {
            fallbackPrim = ModelLoader::LoadFBX(resolvedSource, importSettings);
        }
    }

    if (fallbackPrim)
    {
        fallbackPrim->SetSourceInfo(normalizedSource, subMeshIndex);
        fallbackPrim->SetCustomMeshPath(targetCustomPath);
        fallbackPrim->SetImportSettings(importSettings);
        SaveMeshToCustomFormat(fallbackPrim.get(), targetCustomPath);
    }

    return fallbackPrim;
}

std::unique_ptr<Primitive> MeshImporter::ImportMesh(const std::string& filePath)
{
    std::string projectRelPath = FileSystem::EnsureAssetInProject(filePath, "Models");
    std::string diskPath = FileSystem::ResolveExistingPath(projectRelPath);

    LOG("[MeshImporter] Importando malla desde: %s", projectRelPath.c_str());
    auto loadedMesh = ModelLoader::LoadFBX(diskPath, GetDefaultSettings());
    if (!loadedMesh)
    {
        LOG_ERROR("[MeshImporter] Error al importar la malla: %s", diskPath.c_str());
        return nullptr;
    }
    std::string customPath = BuildCustomMeshPath(projectRelPath, -1);
    loadedMesh->SetSourceInfo(projectRelPath, -1);
    loadedMesh->SetCustomMeshPath(customPath);
    loadedMesh->SetImportSettings(GetDefaultSettings());
    SaveMeshToCustomFormat(loadedMesh.get(), customPath);
    return loadedMesh;
}

GameObject* MeshImporter::ImportToScene(const std::string& filePath, GameObject* parent)
{
    Scene* scene = Engine::GetInstance().scene.get();
    if (!scene)
    {
        LOG_ERROR("[MeshImporter] No se puede importar a la escena: módulo Scene no disponible.");
        return nullptr;
    }

    // Conservar la carpeta de origen original (antes de copiar a Assets/Models/) para localizar texturas acompañantes (ej. carpeta Texture/)
    std::string originalSourceDir;
    {
        std::string resolvedInput = FileSystem::ResolveExistingPath(filePath);
        std::filesystem::path inputPath(resolvedInput);
        if (inputPath.has_parent_path())
        {
            originalSourceDir = inputPath.parent_path().string();
        }
    }

    // Copiar automáticamente modelos externos (Descargas, Escritorio, etc.) dentro de Assets/Models/
    std::string projectRelPath = FileSystem::EnsureAssetInProject(filePath, "Models");
    std::string diskPath = FileSystem::ResolveExistingPath(projectRelPath);

    std::vector<SubMeshData> subMeshes = ModelLoader::LoadFBXMeshes(diskPath, GetDefaultSettings(), originalSourceDir);
    if (subMeshes.empty())
    {
        LOG_ERROR("[MeshImporter] No se encontraron mallas válidas en: %s", diskPath.c_str());
        return nullptr;
    }

    // Guardar cada sub-malla importada en nuestro Custom File Format (.mesh en Library/Meshes/)
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        if (subMeshes[i].primitive)
        {
            std::string customPath = BuildCustomMeshPath(projectRelPath, (int)i);
            subMeshes[i].primitive->SetSourceInfo(projectRelPath, (int)i);
            subMeshes[i].primitive->SetCustomMeshPath(customPath);
            subMeshes[i].primitive->SetImportSettings(GetDefaultSettings());
            SaveMeshToCustomFormat(subMeshes[i].primitive.get(), customPath);
        }
    }

    auto applyImportedMaterial = [](MaterialComponent* mat, const SubMeshData& subData) -> bool
    {
        if (!mat) return false;
        if (!subData.diffuseTexturePath.empty())
        {
            if (mat->SetTexture(subData.diffuseTexturePath))
            {
                mat->SetUseTexture(true);
                mat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, subData.diffuseColor.a));
                return true;
            }
            // Si la carga de la textura falló, volver limpiamente al color base sin dejar textura "missing"
            mat->SetUsingMissingTexture(false);
        }
        mat->SetColor(subData.diffuseColor);
        mat->SetUseTexture(false);
        return false;
    };

    std::filesystem::path p(projectRelPath);
    std::string stemName = p.stem().string();
    if (stemName.empty()) stemName = "ImportedModel";
    std::string uniqueRootName = scene->MakeUniqueName(stemName);

    // Si el archivo solo tiene 1 malla -> creamos un único GameObject directo
    if (subMeshes.size() == 1)
    {
        auto newObj = std::make_unique<GameObject>(uniqueRootName);

        auto trans = std::make_unique<TransformComponent>(newObj.get());
        trans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        newObj->AddComponent(std::move(trans));

        auto mesh = std::make_unique<MeshComponent>(newObj.get(), std::move(subMeshes[0].primitive));
        newObj->AddComponent(std::move(mesh));

        auto mat = std::make_unique<MaterialComponent>(newObj.get());
        bool texLoaded = applyImportedMaterial(mat.get(), subMeshes[0]);
        newObj->AddComponent(std::move(mat));

        GameObject* registered = scene->Register(std::move(newObj), parent);
        scene->SetSelected(registered);

        LOG("[MeshImporter] Modelo simple '%s' importado en la escena (Textura automática: %s, Custom Format: Library/Meshes)",
            uniqueRootName.c_str(), texLoaded ? "Sí" : "No");
        return registered;
    }

    // Si el archivo tiene múltiples partes -> creamos un GameObject padre y un hijo por cada parte
    auto rootObj = std::make_unique<GameObject>(uniqueRootName);
    auto rootTrans = std::make_unique<TransformComponent>(rootObj.get());
    rootTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    rootObj->AddComponent(std::move(rootTrans));

    GameObject* registeredRoot = scene->Register(std::move(rootObj), parent);
    size_t texturedParts = 0;

    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        std::string partBaseName = subMeshes[i].name.empty() ? (stemName + "_Part_" + std::to_string(i + 1)) : subMeshes[i].name;
        std::string uniquePartName = scene->MakeUniqueName(partBaseName);

        auto childObj = std::make_unique<GameObject>(uniquePartName);

        auto childTrans = std::make_unique<TransformComponent>(childObj.get());
        childTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        childObj->AddComponent(std::move(childTrans));

        auto childMesh = std::make_unique<MeshComponent>(childObj.get(), std::move(subMeshes[i].primitive));
        childObj->AddComponent(std::move(childMesh));

        auto childMat = std::make_unique<MaterialComponent>(childObj.get());
        if (applyImportedMaterial(childMat.get(), subMeshes[i]))
        {
            ++texturedParts;
        }
        childObj->AddComponent(std::move(childMat));

        scene->Register(std::move(childObj), registeredRoot);
    }

    scene->SetSelected(registeredRoot);
    LOG("[MeshImporter] Modelo compuesto '%s' importado con %zu partes hijas (%zu con textura automática, Custom Format: Library/Meshes)",
        uniqueRootName.c_str(), subMeshes.size(), texturedParts);
    return registeredRoot;
}


///////////////////////////////////////
// Importers
///////////////////////////////////////
static MeshImportSettings g_defaultMeshImportSettings;

MeshImportSettings& MeshImporter::GetDefaultSettings()
{
    return g_defaultMeshImportSettings;
}

void MeshImporter::SetDefaultSettings(const MeshImportSettings& newSettings)
{
    g_defaultMeshImportSettings = newSettings;
}

std::unique_ptr<Primitive> MeshImporter::ReimportPrimitive(const Primitive* existingPrim, const MeshImportSettings& newSettings)
{
    if (!existingPrim) return nullptr;

    const std::string& sourcePath = existingPrim->GetSourcePath();
    const int subMeshIndex = existingPrim->GetSubMeshIndex();
    if (sourcePath.empty() || sourcePath.rfind("Primitive:", 0) == 0)
    {
        return nullptr;
    }

    std::string resolvedSource = FileSystem::ResolveExistingPath(sourcePath);
    std::unique_ptr<Primitive> reimported = nullptr;

    if (subMeshIndex >= 0)
    {
        std::vector<SubMeshData> subMeshes = ModelLoader::LoadFBXMeshes(resolvedSource, newSettings);
        if (static_cast<size_t>(subMeshIndex) < subMeshes.size())
        {
            reimported = std::move(subMeshes[subMeshIndex].primitive);
        }
    }
    else
    {
        reimported = ModelLoader::LoadFBX(resolvedSource, newSettings);
    }

    if (reimported)
    {
        std::string cleanSource = FileSystem::EnsureAssetInProject(resolvedSource, "Models");
        std::string customPath = existingPrim->GetCustomMeshPath().empty()
            ? BuildCustomMeshPath(cleanSource, subMeshIndex)
            : FileSystem::ToProjectRelativePath(existingPrim->GetCustomMeshPath());

        reimported->SetSourceInfo(cleanSource, subMeshIndex);
        reimported->SetCustomMeshPath(customPath);
        reimported->SetImportSettings(newSettings);
        SaveMeshToCustomFormat(reimported.get(), customPath);
    }

    return reimported;
}


