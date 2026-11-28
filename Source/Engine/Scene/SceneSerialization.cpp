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

// Guardado y carga de escenas (.scene)

namespace
{
	// 2: lista plana de GameObjects con los componentes por nombre (la 1 anidaba los hijos)
	constexpr int SCENE_FILE_VERSION = 3; // 3: mallas y texturas por UID de recurso

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

	// Cámara del editor y selección: van en Library/ (de cada usuario) y no en el .scene, que se sube al repo
	constexpr int EDITOR_STATE_VERSION = 1;

	// "Assets/Scenes/A.scene" -> "Library/EditorState/Assets/Scenes/A.scene.json"; "" si la escena está fuera de Assets
	std::string EditorStatePath(const std::string& scenePath)
	{
		if (!FileSystem::IsInsideFolder(scenePath, FileSystem::ASSETS_DIR)) return std::string();
		return FileSystem::Join(FileSystem::Join(FileSystem::LIBRARY_DIR, "EditorState"), FileSystem::Normalize(scenePath) + ".json");
	}

	nlohmann::json ReadEditorState(const std::string& scenePath)
	{
		const std::string statePath = EditorStatePath(scenePath);
		std::string text;
		if (statePath.empty() || !FileSystem::Exists(statePath) || !FileSystem::ReadText(statePath, text)) return nlohmann::json::object();

		nlohmann::json state = nlohmann::json::parse(text, nullptr, false);
		return state.is_object() ? state : nlohmann::json::object();
	}
}

nlohmann::json Scene::SerializeToJson(const std::string& sceneName) const
{
	nlohmann::json root;
	root["version"] = SCENE_FILE_VERSION;
	root["name"] = sceneName;
	root["spacePartitioning"] = {
		{ "maxObjectsPerNode", treeMaxObjectsPerNode },
		{ "maxDepth", treeMaxDepth }
	};

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
	return root;
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

	const nlohmann::json root = SerializeToJson(sceneName);

	std::string targetSavePath = filePath;
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

	// replace: un nombre con bytes que no son UTF-8 no debe impedir guardar
	const std::string text = root.dump(1, '\t', false, nlohmann::json::error_handler_t::replace);
	if (!WriteTextFile(targetSavePath, text))
	{
		LOG_ERROR("[Scene] Error al guardar la escena en: '%s'", targetSavePath.c_str());
		return false;
	}

	currentScenePath = FileSystem::ToProjectRelativePath(targetSavePath);
	sceneDirty = false;
	UpdateWindowTitle();

	const std::string statePath = EditorStatePath(currentScenePath);
	if (!statePath.empty())
	{
		nlohmann::json state;
		state["version"] = EDITOR_STATE_VERSION;
		state["selected"] = selected ? (uint64_t)selected->GetUUID() : 0; // el activo
		state["selection"] = GetSelectionUIDs();
		if (editorCameraObj)
		{
			nlohmann::json editorCamera = nlohmann::json::object();
			editorCameraObj->Save(editorCamera);
			state["editorCamera"] = std::move(editorCamera);
		}
		FileSystem::WriteText(statePath, state.dump(1, '\t', false, nlohmann::json::error_handler_t::replace));
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

	std::string resolvedScenePath = FileSystem::ResolveExistingPath(filePath);

	std::string text;
	if (!ReadTextFile(resolvedScenePath, text) || text.empty())
	{
		LOG_ERROR("[Scene] No se pudo cargar el archivo de escena: '%s'", resolvedScenePath.c_str());
		return false;
	}

	const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
	const nlohmann::json editorState = ReadEditorState(FileSystem::ToProjectRelativePath(resolvedScenePath));
	if (!LoadFromJson(root, resolvedScenePath, editorState))
	{
		return false;
	}

	currentScenePath = FileSystem::ToProjectRelativePath(resolvedScenePath);
	sceneDirty = false;
	UpdateWindowTitle();
	return true;
}

bool Scene::LoadFromJson(const nlohmann::json& root, const std::string& sourceName, const nlohmann::json& editorState)
{
	// Se valida antes de vaciar la escena actual, para no perderla si el fichero está mal
	if (root.is_discarded() || !root.is_object())
	{
		LOG_ERROR("[Scene] '%s' no es un JSON válido", sourceName.c_str());
		return false;
	}
	const int version = JsonHelpers::Get(root, "version", 0);
	if (version != SCENE_FILE_VERSION)
	{
		LOG_ERROR("[Scene] '%s' usa la versión %d del formato de escena y el motor lee la %d", sourceName.c_str(), version, SCENE_FILE_VERSION);
		return false;
	}
	const auto objectsIt = root.find("gameObjects");
	if (objectsIt == root.end() || !objectsIt->is_array())
	{
		LOG_ERROR("[Scene] '%s' no tiene la lista 'gameObjects'", sourceName.c_str());
		return false;
	}

	isLoadingOrInitializing = true;

	const auto spIt = root.find("spacePartitioning");
	if (spIt != root.end() && spIt->is_object())
	{
		treeMaxObjectsPerNode = std::max(1, JsonHelpers::Get(*spIt, "maxObjectsPerNode", treeMaxObjectsPerNode));
		treeMaxDepth = std::max(1, JsonHelpers::Get(*spIt, "maxDepth", treeMaxDepth));
	}

	const uint64_t savedSelectedUuid = JsonHelpers::Get(editorState, "selected", (uint64_t)0);
	// Los estados guardados antes de la selección múltiple solo tienen "selected"
	std::vector<uint64_t> savedSelection = JsonHelpers::Get(editorState, "selection", std::vector<uint64_t>());
	if (savedSelection.empty() && savedSelectedUuid != 0) savedSelection.push_back(savedSelectedUuid);

	// Limpiar escena actual antes de reconstruir
	ClearScene();

	const auto editorCamIt = editorState.find("editorCamera");
	if (editorCamIt != editorState.end() && editorCamIt->is_object() && editorCameraObj)
	{
		editorCameraObj->Load(*editorCamIt);
		mainCameraPtr = editorCameraObj.get();
	}

	// El mapa de UID de la escena sirve para colgar cada hijo de su padre (en preorden ya está creado)
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
			const bool reuseUuid = objUuid != 0 && FindByUID(objUuid) == nullptr;
			auto newObj = std::make_unique<GameObject>(objName, reuseUuid ? GameUUID(objUuid) : GameUUID());

			GameObject* parent = FindByUID(parentUuid);
			GameObject* regObj = Register(std::move(newObj), parent ? parent : &sceneRoot);
			regObj->Load(objFile);

			if (mainCamera == nullptr && JsonHelpers::Get(objFile, "mainCamera", false)) mainCamera = regObj;
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("[Scene] Error leyendo '%s': %s", sourceName.c_str(), e.what());
	}

	// Restaurar la cámara de la vista Game y el GameObject seleccionado
	GameObject* fallbackCamera = nullptr;
	for (size_t i = 0; i < sceneEntities.size(); ++i)
	{
		GameObject* ent = sceneEntities[i].get();
		if (!ent) continue;

		if (ent->GetComponent<TransformComponent>() == nullptr)
		{
			ent->AddComponent(std::make_unique<TransformComponent>(ent));
		}

		if (fallbackCamera == nullptr && ent->GetComponent<CameraComponent>() != nullptr)
		{
			fallbackCamera = ent;
		}
	}

	gameCameraPtr = mainCamera ? mainCamera : fallbackCamera;

	SelectByUIDs(savedSelection, savedSelectedUuid);
	RebuildSpacePartitioning();
	isLoadingOrInitializing = false;

	LOG("[Scene] Escena cargada con éxito desde '%s' (%zu GameObjects totales)", sourceName.c_str(), sceneEntities.size());
	return true;
}

bool Scene::SavePlaySnapshot()
{
	try
	{
		// Sin sangría: ocupa menos y nadie la lee a mano
		playSnapshot = SerializeToJson(GetSceneDisplayName()).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
	}
	catch (const std::exception& e)
	{
		playSnapshot.clear();
		LOG_ERROR("[Scene] No se pudo copiar la escena antes del Play: %s", e.what());
		return false;
	}
	prePlaySceneDirty = sceneDirty;
	LOG("[Scene] Escena copiada en memoria para el Play (%.1f KB)", playSnapshot.size() / 1024.0f);
	return true;
}

void Scene::RestorePlaySnapshot()
{
	// La selección no va en la copia: se conserva la del momento de Stop con los objetos que siguen existiendo
	const std::vector<uint64_t> selectionUuids = GetSelectionUIDs();
	const uint64_t activeUuid = selected ? (uint64_t)selected->GetUUID() : 0;

	const nlohmann::json root = nlohmann::json::parse(playSnapshot, nullptr, false);
	playSnapshot.clear();
	// Sin estado del editor: la cámara del editor se queda donde está
	if (!LoadFromJson(root, "copia del Play", nlohmann::json::object()))
	{
		LOG_ERROR("[Scene] No se pudo restaurar la escena de antes del Play");
		return;
	}

	SelectByUIDs(selectionUuids, activeUuid);
	sceneDirty = prePlaySceneDirty;
	UpdateWindowTitle();
}

void Scene::RequestLoadScene(const std::string& filePath)
{
	if (filePath.empty()) return;
	// Otra escena saca del Play: Stop pide la restauración y ProcessPendingSceneChange la descarta si la carga va bien
	Engine::GetInstance().Stop();
	pendingSceneChange = PendingSceneChange::Load;
	pendingScenePath = filePath;
}

void Scene::RequestNewScene()
{
	Engine::GetInstance().Stop();
	pendingSceneChange = PendingSceneChange::New;
	pendingScenePath.clear();
}

void Scene::RequestRestorePlaySnapshot()
{
	pendingPlayRestore = !playSnapshot.empty();
}

void Scene::ProcessPendingSceneChange()
{
	if (!HasPendingSceneChange()) return;

	// Se copian y se limpian antes: Load y NewScene podrían pedir otro cambio
	const PendingSceneChange change = pendingSceneChange;
	const std::string scenePath = pendingScenePath;
	const bool restore = pendingPlayRestore;
	pendingSceneChange = PendingSceneChange::None;
	pendingScenePath.clear();
	pendingPlayRestore = false;

	bool replaced = false;
	if (change == PendingSceneChange::Load)
	{
		replaced = Load(scenePath);
	}
	else if (change == PendingSceneChange::New)
	{
		NewScene();
		replaced = true;
	}

	if (!restore) return;

	// Stop y otra escena en el mismo frame: se carga solo la otra (una vez) y la copia del Play sobra.
	// Si esa carga falla, se restaura la escena de antes del Play como hacía Stop.
	if (replaced)
	{
		playSnapshot.clear();
		LOG("[Scene] Restauración del Play descartada: se ha abierto otra escena");
	}
	else
	{
		RestorePlaySnapshot();
	}
}
