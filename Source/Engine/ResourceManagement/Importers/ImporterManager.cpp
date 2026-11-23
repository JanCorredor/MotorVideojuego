#include "ImporterManager.h"
#include "MeshImporter.h"
#include "TextureImporter.h"
#include "../../Engine.h"
#include "../../Scene/Scene.h"
#include "../../../Utils/Log.h"

#include <filesystem>
#include <algorithm>

bool ImporterManager::ImportFile(const std::string& filePath, GameObject* selectedObject)
{
    std::filesystem::path p(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".scene" || ext == ".json")
    {
        if (Engine::GetInstance().scene)
        {
            if (!Engine::GetInstance().IsStopped())
            {
                Engine::GetInstance().Stop();
            }
            std::string projectScenePath = SceneFile::EnsureAssetInProject(filePath, "Scenes");
            return Engine::GetInstance().scene->Load(projectScenePath);
        }
        return false;
    }
    else if (MeshImporter::IsSupportedExtension(ext))
    {
        return MeshImporter::ImportToScene(filePath) != nullptr;
    }
    else if (TextureImporter::IsSupportedExtension(ext))
    {
        return TextureImporter::ImportAndAssignToGameObject(filePath, selectedObject);
    }

    LOG_WARNING("[ImporterManager] Formato de archivo no reconocido para importar: %s", filePath.c_str());
    return false;
}