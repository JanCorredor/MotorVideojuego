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

    auto modelPrimitive = ImportMesh(filePath);
    if (!modelPrimitive)
    {
        return nullptr;
    }

    std::filesystem::path p(filePath);
    std::string stemName = p.stem().string();
    if (stemName.empty()) stemName = "ImportedModel";
    std::string uniqueName = scene->MakeUniqueName(stemName);

    auto newObj = std::make_unique<GameObject>(uniqueName);

    auto trans = std::make_unique<TransformComponent>(newObj.get());
    trans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    newObj->AddComponent(std::move(trans));

    // Se mueve el unique_ptr<Primitive> al MeshComponent sin realizar copias
    auto mesh = std::make_unique<MeshComponent>(newObj.get(), std::move(modelPrimitive));
    newObj->AddComponent(std::move(mesh));

    auto mat = std::make_unique<MaterialComponent>(newObj.get());
    mat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    mat->SetUseTexture(false);
    newObj->AddComponent(std::move(mat));

    GameObject* registered = scene->Register(std::move(newObj), parent);
    scene->SetSelected(registered);

    LOG("[MeshImporter] Modelo '%s' importado y seleccionado en la escena", uniqueName.c_str());
    return registered;
}