#pragma once

#include "Resource.h"
#include "../Geometry/AABB.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include <memory>

// Malla en Library/Meshes/<UID>.mesh. Cargada guarda un Primitive (buffers GL + copia en RAM para picking).
// Las primitivas internas (Cube, Sphere...) no tienen fichero: se generan al cargarse
class ResourceMesh : public Resource
{
public:
	explicit ResourceMesh(UID uid) : Resource(uid, ResourceType::Mesh) {}
	~ResourceMesh() override = default;

	// nullptr si la malla no esta cargada
	const Primitive* GetPrimitive() const { return primitive.get(); }

	// AABB en espacio local de la malla (invalida si no esta cargada)
	AABB GetLocalAABB() const;

	size_t GetVertexCount() const { return primitive ? primitive->GetVertexCount() : 0; }
	size_t GetTriangleCount() const { return primitive ? primitive->GetTriangleCount() : 0; }

	size_t GetMemoryUsage() const override;

	// Lo usa el ResourceManager al registrar las primitivas internas del motor
	void SetBuiltInPrimitive(PrimitiveType type) { builtInType = type; builtIn = true; }

protected:
	bool LoadInMemory() override;
	void UnloadFromMemory() override;

private:
	std::unique_ptr<Primitive> primitive;
	PrimitiveType builtInType = PrimitiveType::Cube;
};
