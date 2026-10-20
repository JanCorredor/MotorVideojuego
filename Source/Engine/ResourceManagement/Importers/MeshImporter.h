#pragma once

#include <string>
#include <memory>
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
};