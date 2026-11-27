#include "ImporterManager.h"
#include "../ResourceManager.h"
#include "../../Engine.h"
#include "../../Scene/Scene.h"
#include "../../Scene/GameObjects/GameObject.h"
#include "../../Scene/GameObjects/Components/MaterialComponent.h"
#include "../../Scene/GameObjects/Components/MeshComponent.h"
#include "../../../Utils/Log.h"

bool ImporterManager::HandleDroppedFile(const std::string& diskPathUtf8, GameObject* selectedObject)
{
	ResourceManager* resources = Engine::GetInstance().resources.get();
	Scene* scene = Engine::GetInstance().scene.get();
	if (resources == nullptr || scene == nullptr) return false;

	// 1) Copiar a Assets + importar a Library
	const std::string assetPath = resources->ImportExternalFile(diskPathUtf8, resources->GetImportFolder());
	if (assetPath.empty())
	{
		LOG_WARNING("[ImporterManager] No se pudo importar '%s'", diskPathUtf8.c_str());
		return false;
	}

	// 2) Accion segun el tipo de fichero
	if (ResourceManager::IsModelFile(assetPath))
	{
		const UID modelUID = resources->GetAssetUID(assetPath);
		GameObject* instance = (modelUID != INVALID_UID) ? scene->InstantiateModel(modelUID) : nullptr;
		if (instance != nullptr) scene->SetSelected(instance);
		return instance != nullptr;
	}

	if (ResourceManager::IsTextureFile(assetPath))
	{
		const UID textureUID = resources->GetAssetUID(assetPath);
		if (textureUID == INVALID_UID) return false;

		if (selectedObject == nullptr)
		{
			LOG("[ImporterManager] Textura '%s' importada. Selecciona un GameObject y arrastrala desde Assets para usarla.", assetPath.c_str());
			return true;
		}
		return AssignTextureToHierarchy(textureUID, selectedObject);
	}

	if (ResourceManager::IsSceneFile(assetPath))
	{
		return scene->Load(assetPath);
	}

	return false;
}

namespace
{
	// Recorre el GameObject y sus hijos poniendo la textura. Devuelve cuantos materiales ha cambiado.
	int AssignTextureRecursive(UID textureUID, GameObject* target)
	{
		int assigned = 0;
		MaterialComponent* material = target->GetComponent<MaterialComponent>();

		// Un objeto con malla pero sin material recibe uno nuevo
		if (material == nullptr && target->GetComponent<MeshComponent>() != nullptr)
		{
			target->AddComponent(std::make_unique<MaterialComponent>(target));
			material = target->GetComponent<MaterialComponent>();
		}

		if (material != nullptr)
		{
			material->SetTexture(textureUID);
			material->SetUseTexture(true);
			++assigned;
		}

		// Si es el padre de un modelo con varias partes, tambien se aplica a sus hijos
		for (GameObject* child : target->GetChildren())
		{
			assigned += AssignTextureRecursive(textureUID, child);
		}
		return assigned;
	}
}

bool ImporterManager::AssignTextureToHierarchy(UID textureUID, GameObject* target)
{
	if (target == nullptr || textureUID == INVALID_UID) return false;

	const int assigned = AssignTextureRecursive(textureUID, target);
	if (assigned > 0)
	{
		LOG("[ImporterManager] Textura asignada a '%s' (%d material(es))", target->GetName().c_str(), assigned);
	}
	else
	{
		LOG_WARNING("[ImporterManager] '%s' no tiene mallas a las que poner la textura", target->GetName().c_str());
	}
	return assigned > 0;
}
