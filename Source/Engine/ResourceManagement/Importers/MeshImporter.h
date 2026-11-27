#pragma once

#include "../../Render/Primitives/Primitive.h"

#include <memory>
#include <string>
#include <vector>

// Formato propio .mesh (binario): cabecera "MMSH" (versión, nº de vértices e índices y AABB local),
// vértices (posición, normal, UV) e índices uint32. Cargarlo es casi un memcpy
class MeshImporter
{
public:
	static bool Save(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::string& libraryPath);

	// Lee el .mesh y crea el Primitive (sube los buffers a la GPU). nullptr si falla.
	static std::unique_ptr<Primitive> Load(const std::string& libraryPath);
};
