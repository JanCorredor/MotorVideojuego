#include "Scene.h"

#include "../Engine.h"
#include "../../Utils/Log.h"
#include "../../Utils/JsonHelpers.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../FileSystem/FileSystem.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

// Guardado y carga de escenas (.scene)

namespace
{
	// 2: lista plana de GameObjects con los componentes por nombre (la 1 anidaba los hijos)
	constexpr int SCENE_FILE_VERSION = 2;

	bool WriteTextFile(const std::string& diskPath, const std::string& text)
	{
		const std::filesystem::path path = std::filesystem::u8path(diskPath);
		std::error_code ec;
		if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) return false;
		out.write(text.data(), (std::streamsize)text.size());
		return static_cast<bool>(out);
	}

	bool ReadTextFile(const std::string& diskPath, std::string& outText)
	{
		std::ifstream in(std::filesystem::u8path(diskPath), std::ios::binary);
		if (!in.is_open()) return false;
		std::ostringstream ss;
		ss << in.rdbuf();
		outText = ss.str();
		return true;
	}
}

bool Scene::Save(const std::string& filePath) const
{
	if (filePath.empty())
	{
		LOG_ERROR("[Scene] No se puede guardar la escena: ruta vacía.");
		return false;
	}

	std::string sceneName = FileSystem::GetFileNameNoExtension(filePath);
	if (sceneName.empty()) sceneName = "Scene";

	nlohmann::json root;
	root["version"] = SCENE_FILE_VERSION;
	root["name"] = sceneName;
	root["spacePartitioning"] = {
		{ "maxObjectsPerNode", treeMaxObjectsPerNode },
		{ "maxDepth", treeMaxDepth }
	};
	root["selected"] = selected ? (uint64_t)selected->GetUUID() : 0;

	if (editorCameraObj)
	{
		nlohmann::json editorCamera = nlohmann::json::object();
		editorCameraObj->Save(editorCamera);
		root["editorCamera"] = std::move(editorCamera);
	}

	// Preorden: cada padre se escribe antes que sus hijos, así la carga los enlaza en una pasada
	nlohmann::json objects = nlohmann::json::array();
	const std::vector<GameObject*>& rootChildren = sceneRoot.GetChildren();
	std::vector<const GameObject*> stack(rootChildren.rbegin(), rootChildren.rend());
	while (!stack.empty())
	{
		const GameObject* node = stack.back();
		stack.pop_back();
		if (!node) continue;

		nlohmann::json object = nlohmann::json::object();
		node->Save(object);
		if (node == gameCameraPtr) object["mainCamera"] = true;
		objects.push_back(std::move(object));

		const std::vector<GameObject*>& children = node->GetChildren();
		for (auto it = children.rbegin(); it != children.rend(); ++it)
		{
			stack.push_back(*it);
		}
	}
	root["gameObjects"] = std::move(objects);

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

	// replace: un nombre con bytes que no son UTF-8 no debe impedir guardar
	const std::string text = root.dump(1, '\t', false, nlohmann::json::error_handler_t::replace);
	if (!WriteTextFile(targetSavePath, text))
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

	std::string text;
	if (!ReadTextFile(resolvedScenePath, text) || text.empty())
	{
		LOG_ERROR("[Scene] No se pudo cargar el archivo de escena: '%s'", resolvedScenePath.c_str());
		return false;
	}

	// Se valida antes de vaciar la escena actual, para no perderla si el fichero está mal
	const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		LOG_ERROR("[Scene] '%s' no es un JSON válido", resolvedScenePath.c_str());
		return false;
	}
	const int version = JsonHelpers::Get(root, "version", 0);
	if (version != SCENE_FILE_VERSION)
	{
		LOG_ERROR("[Scene] '%s' usa la versión %d del formato de escena y el motor lee la %d", resolvedScenePath.c_str(), version, SCENE_FILE_VERSION);
		return false;
	}
	const auto objectsIt = root.find("gameObjects");
	if (objectsIt == root.end() || !objectsIt->is_array())
	{
		LOG_ERROR("[Scene] '%s' no tiene la lista 'gameObjects'", resolvedScenePath.c_str());
		return false;
	}

	isLoadingOrInitializing = true;

	const auto spIt = root.find("spacePartitioning");
	if (spIt != root.end() && spIt->is_object())
	{
		treeMaxObjectsPerNode = std::max(1, JsonHelpers::Get(*spIt, "maxObjectsPerNode", treeMaxObjectsPerNode));
		treeMaxDepth = std::max(1, JsonHelpers::Get(*spIt, "maxDepth", treeMaxDepth));
	}

	const uint64_t savedSelectedUuid = JsonHelpers::Get(root, "selected", (uint64_t)0);

	// Limpiar escena actual antes de reconstruir
	pendingDestruction.clear();
	selected = nullptr;
	gameCameraPtr = nullptr;

	while (!sceneRoot.GetChildren().empty())
	{
		sceneRoot.RemoveChild(sceneRoot.GetChildren().back());
	}
	sceneEntities.clear();

	const auto editorCamIt = root.find("editorCamera");
	if (editorCamIt != root.end() && editorCamIt->is_object() && editorCameraObj)
	{
		editorCameraObj->Load(*editorCamIt);
		mainCameraPtr = editorCameraObj.get();
	}

	// UID del fichero -> objeto creado, para colgar cada hijo de su padre
	std::unordered_map<uint64_t, GameObject*> created;
	GameObject* mainCamera = nullptr;
	try
	{
		for (const nlohmann::json& objFile : *objectsIt)
		{
			if (!objFile.is_object()) continue;

			const uint64_t objUuid = JsonHelpers::Get(objFile, "uid", (uint64_t)0);
			const uint64_t parentUuid = JsonHelpers::Get(objFile, "parent", (uint64_t)0);
			const std::string objName = JsonHelpers::Get(objFile, "name", std::string("GameObject"));

			// Un UID que falta o está repetido recibe uno nuevo
			const bool reuseUuid = objUuid != 0 && created.find(objUuid) == created.end();
			auto newObj = std::make_unique<GameObject>(objName, reuseUuid ? GameUUID(objUuid) : GameUUID());

			const auto parentIt = created.find(parentUuid);
			GameObject* parent = (parentUuid != 0 && parentIt != created.end()) ? parentIt->second : &sceneRoot;
			GameObject* regObj = Register(std::move(newObj), parent);
			regObj->Load(objFile);

			if (reuseUuid) created[objUuid] = regObj;
			if (mainCamera == nullptr && JsonHelpers::Get(objFile, "mainCamera", false)) mainCamera = regObj;
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("[Scene] Error leyendo '%s': %s", resolvedScenePath.c_str(), e.what());
	}

	// Restaurar el GameObject seleccionado y la cámara de la vista Game
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

		if (savedSelectedUuid != 0 && (uint64_t)ent->GetUUID() == savedSelectedUuid)
		{
			restoredSelected = ent;
		}
		if (fallbackCamera == nullptr && ent->GetComponent<CameraComponent>() != nullptr)
		{
			fallbackCamera = ent;
		}
	}

	gameCameraPtr = mainCamera ? mainCamera : fallbackCamera;

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
