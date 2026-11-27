#include "Scene.h"

// Selección de la escena y pertenencia de los GameObjects

bool Scene::OwnsObject(const GameObject* object) const
{
	if (!object || object == &sceneRoot) return false;

	// Si está encolado para destrucción, ya no se considera disponible
	for (const GameObject* pending : pendingDestruction)
	{
		if (pending == object) return false;
	}

	for (const std::unique_ptr<GameObject>& entity : sceneEntities)
	{
		if (entity.get() == object) return true;
	}
	return false;
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
