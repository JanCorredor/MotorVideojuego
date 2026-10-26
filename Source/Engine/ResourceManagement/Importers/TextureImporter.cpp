#include "TextureImporter.h"
#include "../../Engine.h"
#include "../../Scene/GameObjects/GameObject.h"
#include "../../Scene/GameObjects/Components/MaterialComponent.h"
#include "../../../Utils/Log.h"

#include <algorithm>

bool TextureImporter::IsSupportedExtension(const std::string& extension)
{
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga" || ext == ".bmp");
}

const TextureInfo* TextureImporter::ImportTexture(const std::string& filePath)
{
    if (!Engine::GetInstance().textures)
    {
        LOG_ERROR("[TextureImporter] TextureManager no está inicializado.");
        return nullptr;
    }

    LOG("[TextureImporter] Importando textura desde: %s", filePath.c_str());
    const TextureInfo* texInfo = Engine::GetInstance().textures->Load(filePath);
    if (!texInfo)
    {
        LOG_ERROR("[TextureImporter] Falló la importación de la textura: %s", filePath.c_str());
    }
    return texInfo;
}

bool TextureImporter::ImportAndAssignToGameObject(const std::string& filePath, GameObject* target)
{
    if (target == nullptr)
    {
        LOG_WARNING("[TextureImporter] Se intentó importar '%s', pero no hay ningún GameObject seleccionado", filePath.c_str());
        return false;
    }

    MaterialComponent* mat = target->GetComponent<MaterialComponent>();
    if (mat == nullptr)
    {
        LOG_WARNING("[TextureImporter] El GameObject seleccionado '%s' no tiene MaterialComponent", target->GetName().c_str());
        return false;
    }

    if (mat->SetTexture(filePath))
    {
        mat->SetUseTexture(true);
        LOG("[TextureImporter] Textura '%s' importada y asignada a '%s'", filePath.c_str(), target->GetName().c_str());
        return true;
    }

    return false;
}