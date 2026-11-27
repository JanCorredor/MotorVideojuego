#include "Scene.h"

#include <algorithm>

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

bool Scene::IsInSelectedHierarchy(const GameObject* object) const
{
	if (selection.empty()) return false;
	for (const GameObject* node = object; node != nullptr; node = node->GetParent())
	{
		if (IsSelected(node)) return true;
	}
	return false;
}

void Scene::Select(GameObject* object)
{
	const bool valid = OwnsObject(object);
	if (!valid && selection.empty()) return;

	selection.clear();
	selectionSet.clear();
	selected = nullptr;
	if (valid)
	{
		selection.push_back(object);
		selectionSet.insert(object);
		selected = object;
	}
	// Sube aunque se repita el mismo objeto: la Hierarchy vuelve a desplegarse hasta él
	selectionRevision++;
}

void Scene::AddToSelection(GameObject* object)
{
	if (!OwnsObject(object)) return;
	if (selectionSet.insert(object).second) selection.push_back(object);
	selected = object;
	selectionRevision++;
}

void Scene::RemoveFromSelection(GameObject* object)
{
	if (selectionSet.erase(object) == 0) return;
	selection.erase(std::remove(selection.begin(), selection.end(), object), selection.end());
	if (selected == object) selected = selection.empty() ? nullptr : selection.back();
	selectionRevision++;
}

void Scene::ToggleSelection(GameObject* object)
{
	if (IsSelected(object)) RemoveFromSelection(object);
	else AddToSelection(object);
}

void Scene::SetSelection(const std::vector<GameObject*>& objects, GameObject* active)
{
	selection.clear();
	selectionSet.clear();
	for (GameObject* object : objects)
	{
		if (OwnsObject(object) && selectionSet.insert(object).second) selection.push_back(object);
	}
	selected = (active != nullptr && IsSelected(active)) ? active : (selection.empty() ? nullptr : selection.back());
	selectionRevision++;
}

void Scene::SelectAll()
{
	std::vector<GameObject*> all;
	all.reserve(sceneEntities.size());
	for (const std::unique_ptr<GameObject>& entity : sceneEntities)
	{
		if (OwnsObject(entity.get())) all.push_back(entity.get());
	}
	SetSelection(all, selected);
}

std::vector<GameObject*> Scene::GetTopLevelSelection() const
{
	std::vector<GameObject*> result;
	result.reserve(selection.size());
	for (GameObject* object : selection)
	{
		bool hasSelectedAncestor = false;
		for (const GameObject* node = object->GetParent(); node != nullptr; node = node->GetParent())
		{
			if (IsSelected(node))
			{
				hasSelectedAncestor = true;
				break;
			}
		}
		if (!hasSelectedAncestor) result.push_back(object);
	}
	return result;
}

std::vector<uint64_t> Scene::GetSelectionUIDs() const
{
	std::vector<uint64_t> uids;
	uids.reserve(selection.size());
	for (const GameObject* object : selection) uids.push_back((uint64_t)object->GetUUID());
	return uids;
}

void Scene::SelectByUIDs(const std::vector<uint64_t>& uids, uint64_t activeUID)
{
	std::vector<GameObject*> objects;
	objects.reserve(uids.size());
	for (uint64_t uid : uids)
	{
		if (GameObject* object = FindByUID(uid)) objects.push_back(object);
	}
	SetSelection(objects, FindByUID(activeUID));
}

std::vector<GameObject*> Scene::DuplicateSelection()
{
	const std::vector<GameObject*> originals = GetTopLevelSelection();
	std::vector<GameObject*> copies;
	copies.reserve(originals.size());
	GameObject* activeCopy = nullptr;
	for (GameObject* original : originals)
	{
		GameObject* copy = DuplicateGameObject(original);
		if (!copy) continue;
		copies.push_back(copy);
		if (original == selected) activeCopy = copy;
	}
	if (!copies.empty()) SetSelection(copies, activeCopy);
	return copies;
}

void Scene::DestroySelection()
{
	// Se copia antes: DestroyGameObject va sacando cada objeto de la selección
	const std::vector<GameObject*> targets = GetTopLevelSelection();
	for (GameObject* target : targets)
	{
		DestroyGameObject(target);
	}
}
