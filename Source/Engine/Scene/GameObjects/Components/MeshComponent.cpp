#include "MeshComponent.h"
#include "../GameObject.h"
#include "../../Scene.h"
#include "../../../Engine.h"
#include "../../../ResourceManagement/ResourceManager.h"
#include "../../../ResourceManagement/ResourceMesh.h"
#include "../../../../Utils/JsonHelpers.h"
#include "../../../../Utils/Log.h"

#include <nlohmann/json.hpp>

namespace
{
	// La AABB del objeto cambia: el árbol de partición espacial tiene que rehacerse
	void MarkSpacePartitioningDirty()
	{
		if (Scene* scene = Engine::GetInstance().scene.get()) scene->MarkSpacePartitioningDirty();
	}
}

MeshComponent::MeshComponent(GameObject* owner, UID meshUID) : Component(COMPONENT_MESH, owner)
{
	SetMesh(meshUID);
}

MeshComponent::~MeshComponent()
{
	ReleaseMesh();
}

std::unique_ptr<Component> MeshComponent::Clone(GameObject* newOwner) const
{
	// El clon pide su propia referencia a la misma malla
	auto clone = std::make_unique<MeshComponent>(newOwner, missing ? INVALID_UID : meshUID);
	if (!active) clone->Disable();
	return clone;
}

void MeshComponent::SetMesh(UID uid)
{
	if (uid == meshUID && !missing) return;

	ResourceManager* resources = Engine::GetInstance().resources.get();

	// Se pide la nueva antes de soltar la vieja: si fuese la misma no se descarga
	ResourceMesh* newMesh = nullptr;
	bool newMissing = false;
	if (uid != INVALID_UID && resources != nullptr)
	{
		Resource* resource = resources->RequestResource(uid);
		if (resource != nullptr && resource->GetType() == ResourceType::Mesh)
		{
			newMesh = static_cast<ResourceMesh*>(resource);
		}
		else
		{
			if (resource != nullptr) resources->ReleaseResource(uid);
			newMissing = true;
			LOG_WARNING("[Mesh] La malla con UID %llu no existe (¿asset borrado?)", (unsigned long long)uid);
		}
	}

	ReleaseMesh();

	meshUID = uid;
	mesh = newMesh;
	missing = newMissing;

	MarkSpacePartitioningDirty();
	NotifyOwnerModified();
}

void MeshComponent::ReleaseMesh()
{
	if (mesh != nullptr)
	{
		if (ResourceManager* resources = Engine::GetInstance().resources.get())
		{
			resources->ReleaseResource(meshUID);
		}
		mesh = nullptr;
	}
}

void MeshComponent::MarkMeshMissing()
{
	ReleaseMesh();
	missing = (meshUID != INVALID_UID);
	MarkSpacePartitioningDirty();
}

const Primitive* MeshComponent::GetPrimitive() const
{
	return mesh ? mesh->GetPrimitive() : nullptr;
}

AABB MeshComponent::GetLocalAABB() const
{
	return mesh ? mesh->GetLocalAABB() : AABB();
}

void MeshComponent::Enable()
{
	Component::Enable();
	MarkSpacePartitioningDirty();
}

void MeshComponent::Disable()
{
	Component::Disable();
	MarkSpacePartitioningDirty();
}

void MeshComponent::Save(nlohmann::json& out) const
{
	out["active"] = active;
	out["mesh"] = meshUID;
}

void MeshComponent::Load(const nlohmann::json& in)
{
	SetMesh(JsonHelpers::Get(in, "mesh", (UID)INVALID_UID));
	if (JsonHelpers::Get(in, "active", true)) Enable();
	else Disable();
}
