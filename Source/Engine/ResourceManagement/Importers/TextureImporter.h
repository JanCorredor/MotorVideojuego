#pragma once

#include <string>
#include "../../Render/Texture/TextureManager.h"

class GameObject;

class TextureImporter
{
public:
    // Verifica si la extensión es un formato de imagen soportado (.png, .jpg, .jpeg, .dds, .tga, .bmp)
    static bool IsSupportedExtension(const std::string& extension);

    // Importa la textura a través del TextureManager (con caché en GPU)
    static const TextureInfo* ImportTexture(const std::string& filePath);

    // Importa la textura y la asigna al MaterialComponent del GameObject indicado
    static bool ImportAndAssignToGameObject(const std::string& filePath, GameObject* target);
};