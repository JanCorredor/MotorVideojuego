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

#include <filesystem>
#include <fstream>
#include <algorithm>

bool MeshImporter::IsSupportedExtension(const std::string& extension)
{
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".fbx" || ext == ".obj" || ext == ".dae");
}

std::string MeshImporter::BuildCustomMeshPath(const std::string& sourcePath, int subMeshIndex, uint64_t fallbackId)
{
    std::string libDir = SceneFile::ResolveLibraryMeshesDirectory();

    if (sourcePath.rfind("Primitive:", 0) == 0)
    {
        std::string primName = sourcePath.substr(10);
        if (primName.empty()) primName = "Cube";
        return libDir + "/Primitive_" + primName + ".mesh";
    }

    if (!sourcePath.empty())
    {
        std::filesystem::path p(sourcePath);
        std::string stem = p.stem().string();
        if (stem.empty()) stem = "ImportedMesh";
        if (subMeshIndex >= 0)
        {
            return libDir + "/" + stem + "_sub" + std::to_string(subMeshIndex) + ".mesh";
        }
        return libDir + "/" + stem + ".mesh";
    }

    return libDir + "/CustomMesh_" + std::to_string(fallbackId) + ".mesh";
}

bool MeshImporter::SaveMeshToCustomFormat(const Primitive* primitive, const std::string& customFilePath)
{
    if (!primitive || customFilePath.empty()) return false;

    std::filesystem::path targetPath(customFilePath);
    if (targetPath.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(targetPath.parent_path(), ec);
    }

    std::ofstream out(customFilePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        LOG_ERROR("[MeshImporter] No se pudo guardar el Custom File Format de malla: '%s'", customFilePath.c_str());
        return false;
    }

    const std::vector<Vertex>& vertices = primitive->GetVertices();
    const std::vector<unsigned int>& indices = primitive->GetIndices();

    // Header del Custom File Format (.mesh): magic + ranges[2] (numVertices, numIndices) + AABB bounds
    const char magic[8] = { 'M', 'V', 'M', 'E', 'S', 'H', '0', '1' };
    uint32_t ranges[2] = {
        static_cast<uint32_t>(vertices.size()),
        static_cast<uint32_t>(indices.size())
    };
    glm::vec3 bMin = primitive->GetBoundsMin();
    glm::vec3 bMax = primitive->GetBoundsMax();

    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char*>(ranges), sizeof(ranges));
    out.write(reinterpret_cast<const char*>(&bMin), sizeof(bMin));
    out.write(reinterpret_cast<const char*>(&bMax), sizeof(bMax));

    if (ranges[0] > 0)
    {
        out.write(reinterpret_cast<const char*>(vertices.data()), ranges[0] * sizeof(Vertex));
    }
    if (ranges[1] > 0)
    {
        out.write(reinterpret_cast<const char*>(indices.data()), ranges[1] * sizeof(unsigned int));
    }

    out.close();
    return true;
}

std::unique_ptr<Primitive> MeshImporter::LoadMeshFromCustomFormat(const std::string& customFilePath)
{
    if (customFilePath.empty()) return nullptr;

    std::string resolvedPath = SceneFile::ResolveExistingPath(customFilePath);
    std::error_code ec;
    if (!std::filesystem::exists(resolvedPath, ec))
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
    const char expectedMagic[8] = { 'M', 'V', 'M', 'E', 'S', 'H', '0', '1' };
    for (int i = 0; i < 8; ++i)
    {
        if (magic[i] != expectedMagic[i])
        {
            LOG_ERROR("[MeshImporter] Cabecera inválida en archivo Custom File Format: '%s'", resolvedPath.c_str());
            return nullptr;
        }
    }

    uint32_t ranges[2] = { 0, 0 };
    glm::vec3 bMin(0.0f);
    glm::vec3 bMax(0.0f);
    in.read(reinterpret_cast<char*>(ranges), sizeof(ranges));
    in.read(reinterpret_cast<char*>(&bMin), sizeof(bMin));
    in.read(reinterpret_cast<char*>(&bMax), sizeof(bMax));

    if (!in)
    {
        return nullptr;
    }

    std::vector<Vertex> vertices(ranges[0]);
    if (ranges[0] > 0)
    {
        in.read(reinterpret_cast<char*>(vertices.data()), ranges[0] * sizeof(Vertex));
    }

    std::vector<unsigned int> indices(ranges[1]);
    if (ranges[1] > 0)
    {
        in.read(reinterpret_cast<char*>(indices.data()), ranges[1] * sizeof(unsigned int));
    }
    in.close();

    auto prim = std::make_unique<Primitive>();
    prim->Init(std::move(vertices), std::move(indices));
    prim->SetCustomMeshPath(resolvedPath);
    return prim;
}

std::unique_ptr<Primitive> MeshImporter::ResolveOrLoadPrimitive(const std::string& sourcePath, int subMeshIndex, const std::string& customMeshPath)
{
    std::string targetCustomPath = customMeshPath.empty()
        ? BuildCustomMeshPath(sourcePath, subMeshIndex)
        : customMeshPath;

    // 1. Intentar cargar directamente desde el Custom File Format (.mesh en Library/Meshes/)
    std::unique_ptr<Primitive> loadedCustom = LoadMeshFromCustomFormat(targetCustomPath);
    if (loadedCustom)
    {
        loadedCustom->SetSourceInfo(sourcePath, subMeshIndex);
        loadedCustom->SetCustomMeshPath(targetCustomPath);
        return loadedCustom;
    }

    // 2. Fallback: regenerar desde sourcePath (primitiva procedural o archivo FBX/OBJ/DAE)
    std::unique_ptr<Primitive> fallbackPrim = nullptr;
    if (sourcePath == "Primitive:Cube")          fallbackPrim = GeometryGenerator::CreateCube(1.0f);
    else if (sourcePath == "Primitive:Sphere")   fallbackPrim = GeometryGenerator::CreateSphere(0.5f, 20, 20);
    else if (sourcePath == "Primitive:Plane")    fallbackPrim = GeometryGenerator::CreatePlane(1.0f);
    else if (sourcePath == "Primitive:Cylinder") fallbackPrim = GeometryGenerator::CreateCylinder(0.5f, 1.0f, 32);
    else if (sourcePath == "Primitive:Pyramid")  fallbackPrim = GeometryGenerator::CreatePyramid(1.0f, 1.0f);
    else if (!sourcePath.empty())
    {
        std::string resolvedSource = SceneFile::ResolveExistingPath(sourcePath);
        if (subMeshIndex >= 0)
        {
            std::vector<SubMeshData> subMeshes = ModelLoader::LoadFBXMeshes(resolvedSource);
            for (size_t i = 0; i < subMeshes.size(); ++i)
            {
                if (subMeshes[i].primitive)
                {
                    std::string subCustom = BuildCustomMeshPath(sourcePath, (int)i);
                    subMeshes[i].primitive->SetSourceInfo(sourcePath, (int)i);
                    subMeshes[i].primitive->SetCustomMeshPath(subCustom);
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
            fallbackPrim = ModelLoader::LoadFBX(resolvedSource);
        }
    }

    if (fallbackPrim)
    {
        fallbackPrim->SetSourceInfo(sourcePath, subMeshIndex);
        fallbackPrim->SetCustomMeshPath(targetCustomPath);
        SaveMeshToCustomFormat(fallbackPrim.get(), targetCustomPath);
    }

    return fallbackPrim;
}

std::unique_ptr<Primitive> MeshImporter::ImportMesh(const std::string& filePath)
{
    LOG("[MeshImporter] Importando malla desde: %s", filePath.c_str());
    auto loadedMesh = ModelLoader::LoadFBX(filePath);
    if (!loadedMesh)
    {
        LOG_ERROR("[MeshImporter] Error al importar la malla: %s", filePath.c_str());
        return nullptr;
    }
    std::string customPath = BuildCustomMeshPath(filePath, -1);
    loadedMesh->SetSourceInfo(filePath, -1);
    loadedMesh->SetCustomMeshPath(customPath);
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

    std::vector<SubMeshData> subMeshes = ModelLoader::LoadFBXMeshes(filePath);
    if (subMeshes.empty())
    {
        LOG_ERROR("[MeshImporter] No se encontraron mallas válidas en: %s", filePath.c_str());
        return nullptr;
    }

    // Guardar cada sub-malla importada en nuestro Custom File Format (.mesh en Library/Meshes/)
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        if (subMeshes[i].primitive)
        {
            std::string customPath = BuildCustomMeshPath(filePath, (int)i);
            subMeshes[i].primitive->SetSourceInfo(filePath, (int)i);
            subMeshes[i].primitive->SetCustomMeshPath(customPath);
            SaveMeshToCustomFormat(subMeshes[i].primitive.get(), customPath);
        }
    }

    std::filesystem::path p(filePath);
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
        mat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        mat->SetUseTexture(false);
        newObj->AddComponent(std::move(mat));

        GameObject* registered = scene->Register(std::move(newObj), parent);
        scene->SetSelected(registered);

        LOG("[MeshImporter] Modelo simple '%s' importado en la escena (Custom Format: Library/Meshes)", uniqueRootName.c_str());
        return registered;
    }

    // Si el archivo tiene múltiples partes -> creamos un GameObject padre y un hijo por cada parte
    auto rootObj = std::make_unique<GameObject>(uniqueRootName);
    auto rootTrans = std::make_unique<TransformComponent>(rootObj.get());
    rootTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    rootObj->AddComponent(std::move(rootTrans));

    GameObject* registeredRoot = scene->Register(std::move(rootObj), parent);

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
        childMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        childMat->SetUseTexture(false);
        childObj->AddComponent(std::move(childMat));

        scene->Register(std::move(childObj), registeredRoot);
    }

    scene->SetSelected(registeredRoot);
    LOG("[MeshImporter] Modelo compuesto '%s' importado con %zu partes hijas (Custom Format: Library/Meshes)", uniqueRootName.c_str(), subMeshes.size());
    return registeredRoot;
}
