#include "MeshImporter.h"
#include "../../Engine.h"
#include "../../Scene/Scene.h"
#include "../../Scene/GameObjects/GameObject.h"
#include "../../Scene/GameObjects/Components/TransformComponent.h"
#include "../../Scene/GameObjects/Components/MeshComponent.h"
#include "../../Scene/GameObjects/Components/MaterialComponent.h"
#include "../../Render/3DModels/ModelLoader.h"
#include "../../../Utils/Log.h"

#include <filesystem>
#include <algorithm>

bool MeshImporter::IsSupportedExtension(const std::string& extension)
{
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".fbx" || ext == ".obj" || ext == ".dae");
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

    auto subMeshes = ModelLoader::LoadFBXMeshes(filePath);
    if (subMeshes.empty())
    {
        LOG_ERROR("[MeshImporter] No se encontraron mallas válidas en: %s", filePath.c_str());
        return nullptr;
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

        LOG("[MeshImporter] Modelo simple '%s' importado en la escena", uniqueRootName.c_str());
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
    LOG("[MeshImporter] Modelo compuesto '%s' importado con %zu partes hijas", uniqueRootName.c_str(), subMeshes.size());
    return registeredRoot;
}