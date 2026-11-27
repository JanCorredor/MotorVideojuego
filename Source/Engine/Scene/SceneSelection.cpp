#include "Scene.h"

// Selección de la escena y pertenencia de los GameObjects

bool Scene::OwnsObject(const GameObject* object) const
{
	if (!object || object == &sceneRoot) return false;

	// Los objetos encolados para destruirse ya han salido del mapa
	const auto it = objectsByUID.find((uint64_t)object->GetUUID());
	return it != objectsByUID.end() && it->second == object;
}

GameObject* Scene::FindByUID(uint64_t uid) const
{
	if (uid == 0) return nullptr;
	const auto it = objectsByUID.find(uid);
	return it != objectsByUID.end() ? it->second : nullptr;
}

void Scene::SetSelected(GameObject* object)
{
	if (object == nullptr)
	{
		if (selected != nullptr)
		{
			selected = nullptr;
			selectionRevision++;
		}
		return;
	}
	if (OwnsObject(object))
	{
		selected = object;
		selectionRevision++;
	}
	else
	{
		selected = nullptr;
		selectionRevision++;
	}
}
