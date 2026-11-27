#pragma once

#include "Component.h"
#include "../../../Geometry/AABB.h"
#include "../../../ResourceManagement/Resource.h"

#include <memory>

class Primitive;
class ResourceMesh;

// No es dueño de la malla: guarda el UID de un ResourceMesh y lo pide al ResourceManager,
// así varios GameObjects con la misma malla comparten una sola copia en memoria
class MeshComponent : public Component
{
public:
	explicit MeshComponent(GameObject* owner, UID meshUID = INVALID_UID);
	~MeshComponent() override;

	// Copiar a mano rompería el conteo de referencias: se usa Clone()
	MeshComponent(const MeshComponent&) = delete;
	MeshComponent& operator=(const MeshComponent&) = delete;

	std::unique_ptr<Component> Clone(GameObject* newOwner) const override;

	// Pide la nueva malla (+1 referencia) y suelta la anterior (-1). INVALID_UID = sin malla
	void SetMesh(UID uid);
	UID GetMeshUID() const { return meshUID; }
	const ResourceMesh* GetMeshResource() const { return mesh; }

	// nullptr si no hay malla cargada
	const Primitive* GetPrimitive() const;
	AABB GetLocalAABB() const;

	// El UID guardado ya no existe (por ejemplo se borró el asset)
	bool IsMeshMissing() const { return missing; }
	void MarkMeshMissing();

	void Enable() override;
	void Disable() override;

	void Save(nlohmann::json& out) const override;
	void Load(const nlohmann::json& in) override;

private:
	void ReleaseMesh();

	UID meshUID = INVALID_UID;
	ResourceMesh* mesh = nullptr; // propiedad del ResourceManager
	bool missing = false;
};
