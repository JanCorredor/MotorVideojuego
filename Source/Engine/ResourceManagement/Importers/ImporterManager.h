#pragma once

#include <string>

class GameObject;

class ImporterManager
{
public:
    // Detecta el tipo de archivo por su extensión y delega en MeshImporter o TextureImporter
    static bool ImportFile(const std::string& filePath, GameObject* selectedObject = nullptr);
};