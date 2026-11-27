#include "ResourceMesh.h"
#include "Importers/MeshImporter.h"
#include "../../Utils/Log.h"

AABB ResourceMesh::GetLocalAABB() const
{
	if (!primitive || primitive->GetVertexCount() == 0) return AABB();
	return AABB(primitive->GetBoundsMin(), primitive->GetBoundsMax());
}

size_t ResourceMesh::GetMemoryUsage() const
{
	if (!primitive) return 0;
	// Vertices e indices estan en RAM (para picking) y tambien en la GPU
	const size_t bytes = primitive->GetVertexCount() * sizeof(Vertex) +
		primitive->GetIndices().size() * sizeof(unsigned int);
	return bytes * 2;
}

bool ResourceMesh::LoadInMemory()
{
	if (builtIn)
	{
		primitive = GeometryGenerator::Create(builtInType);
	}
	else
	{
		primitive = MeshImporter::Load(libraryPath);
	}

	if (!primitive)
	{
		LOG_ERROR("[Resources] No se pudo cargar la malla '%s' (%s)", name.c_str(), libraryPath.c_str());
		return false;
	}
	return true;
}

void ResourceMesh::UnloadFromMemory()
{
	// El destructor de Primitive libera el VAO/VBO/EBO de OpenGL
	primitive.reset();
}
