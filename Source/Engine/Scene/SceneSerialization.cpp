#include "Scene.h"

#include "../Engine.h"
#include "../../Utils/Log.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../FileSystem/FileSystem.h"

#include <algorithm>
#include <filesystem>

// Guardado y carga de escenas (.scene)

bool Scene::Save(const std::string& filePath) const
{
	if (filePath.empty())
	{
		LOG_ERROR("[Scene] No se puede guardar la escena: ruta vacía.");
		return false;
	}

	SceneFile file;
	std::filesystem::path p(filePath);
	std::string sceneName = p.stem().string();
	if (sceneName.empty()) sceneName = "Scene";

	file.SetString("scene_name", sceneName);
	file.SetInt("version", 1);

	SceneFile spNode = file.AddObject("space_partitioning");
	spNode.SetInt("max_objects_per_node", treeMaxObjectsPerNode);
	spNode.SetInt("max_depth", treeMaxDepth);

	file.SetUInt64("selected_uuid", selected ? (uint64_t)selected->GetUUID() : 0);
	file.SetUInt64("game_camera_uuid", gameCameraPtr ? (uint64_t)gameCameraPtr->GetUUID() : 0);

	if (editorCameraObj)
	{
		SceneFile editorCamNode = file.AddObject("editor_camera");
		editorCameraObj->Save(editorCamNode);
	}

	// Pasar Save(file) a los hijos raíz de la escena (cada GameObject se la pasa a sus componentes y a sus hijos)
	SceneFile objectsArray = file.AddArray("game_objects");
	const std::vector<GameObject*>& rootChildren = sceneRoot.GetChildren();
	for (size_t i = 0; i < rootChildren.size(); ++i)
	{
		GameObject* child = rootChildren[i];
		if (child)
		{
			SceneFile childFile = objectsArray.PushObject();
			child->Save(childFile);
		}
	}

	const bool isTempFile = (filePath == Engine::GetInstance().GetTempSceneFilePath());
	std::string targetSavePath = filePath;
	if (!isTempFile)
	{
		std::filesystem::path rawPath(filePath);
		if (!rawPath.is_absolute())
		{
			std::string norm = filePath;
			std::replace(norm.begin(), norm.end(), '\\', '/');
			if (norm.rfind("Assets/", 0) == 0 || norm.rfind("Library/", 0) == 0)
			{
				targetSavePath = FileSystem::ToDiskPath(norm);
			}
		}
	}

	if (!file.SaveToFile(targetSavePath))
	{
		LOG_ERROR("[Scene] Error al guardar la escena en: '%s'", targetSavePath.c_str());
		return false;
	}

	if (!isTempFile)
	{
		currentScenePath = FileSystem::ToProjectRelativePath(targetSavePath);
		sceneDirty = false;
		UpdateWindowTitle();
	}

	LOG("[Scene] Escena guardada con éxito en '%s' (%zu GameObjects totales)", targetSavePath.c_str(), sceneEntities.size());
	return true;
}

bool Scene::Load(const std::string& filePath)
{
	if (filePath.empty())
	{
		LOG_ERROR("[Scene] No se puede cargar la escena: ruta vacía.");
		return false;
	}

	const bool isTempFile = (filePath == Engine::GetInstance().GetTempSceneFilePath());
	std::string resolvedScenePath = isTempFile ? filePath : FileSystem::ResolveExistingPath(filePath);

	SceneFile file;
	if (!file.LoadFromFile(resolvedScenePath))
	{
		LOG_ERROR("[Scene] No se pudo cargar el archivo de escena: '%s'", resolvedScenePath.c_str());
		return false;
	}

	isLoadingOrInitializing = true;

	SceneFile spNode = file.GetObjectNode("space_partitioning");
	if (spNode.IsValid())
	{
		treeMaxObjectsPerNode = std::max(1, spNode.GetInt("max_objects_per_node", treeMaxObjectsPerNode));
		treeMaxDepth = std::max(1, spNode.GetInt("max_depth", treeMaxDepth));
	}

	uint64_t savedSelectedUuid = file.GetUInt64("selected_uuid", 0);
	uint64_t savedGameCameraUuid = file.GetUInt64("game_camera_uuid", 0);

	// Limpiar escena actual antes de reconstruir
	pendingDestruction.clear();
	selected = nullptr;
	gameCameraPtr = nullptr;

	while (!sceneRoot.GetChildren().empty())
	{
		sceneRoot.RemoveChild(sceneRoot.GetChildren().back());
	}
	sceneEntities.clear();

	SceneFile editorCamNode = file.GetObjectNode("editor_camera");
	if (editorCamNode.IsValid() && editorCameraObj)
	{
		editorCameraObj->Load(editorCamNode);
		mainCameraPtr = editorCameraObj.get();
	}

	// Cargar GameObjects raíz pasando Load(file) a cada uno (ellos se la pasan a sus componentes y a sus hijos)
	SceneFile objectsArray = file.GetArray("game_objects");
	for (size_t i = 0; i < objectsArray.Size(); ++i)
	{
		SceneFile objFile = objectsArray.GetElement(i);
		uint64_t objUuid = objFile.GetUInt64("uuid", 0);
		std::string objName = objFile.GetString("name", "GameObject");

		auto newObj = std::make_unique<GameObject>(objName, objUuid != 0 ? GameUUID(objUuid) : GameUUID());
		GameObject* regObj = Register(std::move(newObj), &sceneRoot);
		regObj->Load(objFile);
	}

	// Restaurar punteros a Main Camera (vista Game) y al GameObject seleccionado por UUID
	GameObject* restoredSelected = nullptr;
	GameObject* fallbackCamera = nullptr;
	for (size_t i = 0; i < sceneEntities.size(); ++i)
	{
		GameObject* ent = sceneEntities[i].get();
		if (!ent) continue;

		if (ent->GetComponent<TransformComponent>() == nullptr)
		{
			ent->AddComponent(std::make_unique<TransformComponent>(ent));
		}

		uint64_t entUuid = (uint64_t)ent->GetUUID();
		if (savedGameCameraUuid != 0 && entUuid == savedGameCameraUuid)
		{
			gameCameraPtr = ent;
		}
		if (savedSelectedUuid != 0 && entUuid == savedSelectedUuid)
		{
			restoredSelected = ent;
		}
		if (fallbackCamera == nullptr && ent->GetComponent<CameraComponent>() != nullptr)
		{
			fallbackCamera = ent;
		}
	}

	if (gameCameraPtr == nullptr)
	{
		gameCameraPtr = fallbackCamera;
	}

	SetSelected(restoredSelected);
	RebuildSpacePartitioning();
	isLoadingOrInitializing = false;

	if (!isTempFile)
	{
		currentScenePath = FileSystem::ToProjectRelativePath(resolvedScenePath);
		sceneDirty = false;
		UpdateWindowTitle();
	}

	LOG("[Scene] Escena cargada con éxito desde '%s' (%zu GameObjects totales)", resolvedScenePath.c_str(), sceneEntities.size());
	return true;
}

bool Scene::SaveTemporaryScene(const std::string& filePath) const
{
	prePlayScenePath = currentScenePath;
	prePlaySceneDirty = sceneDirty;
	return Save(filePath);
}

bool Scene::LoadTemporaryScene(const std::string& filePath, bool deleteFileAfterLoad)
{
	if (!Load(filePath))
	{
		return false;
	}

	currentScenePath = prePlayScenePath;
	sceneDirty = prePlaySceneDirty;
	UpdateWindowTitle();

	if (deleteFileAfterLoad)
	{
		std::error_code ec;
		std::filesystem::remove(filePath, ec);
		LOG("[Scene] Archivo temporal '%s' restaurado (%zu objetos) y eliminado del disco", filePath.c_str(), sceneEntities.size());
	}
	return true;
}
