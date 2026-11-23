#include "../Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Render/3DModels/ModelLoader.h"
#include "../Input/Input.h"
#include "../../Utils/Log.h"
#include "Scene.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/MeshComponent.h"
#include "GameObjects/Components/MaterialComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include "../ResourceManagement/Importers/ImporterManager.h"
#include "../ResourceManagement/Importers/MeshImporter.h"
#include "../ResourceManagement/Importers/TextureImporter.h"

#ifdef GetObject
#undef GetObject
#endif

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <stack>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cctype>

#ifdef _DEBUG
#include <tracy/Tracy.hpp>
#endif


Scene::Scene() : Module()
{
	name = "scene";
}

Scene::~Scene()
{
}

bool Scene::Awake()
{
	LOG("[Scene] Inicializando módulo Scene...");
	return true;
}

bool Scene::Start()
{
	LOG("[Scene] Configurando escena por defecto...");
	isLoadingOrInitializing = true;

	// Instance Editor Camera
	editorCameraObj = std::make_unique<GameObject>("EditorCamera");
	auto camTrans = std::make_unique<TransformComponent>(editorCameraObj.get());
	camTrans->SetPosition(glm::vec4(0.0f, 1.0f, 5.0f, 1.0f));
	editorCameraObj->AddComponent(std::move(camTrans));

	auto camComp = std::make_unique<CameraComponent>(editorCameraObj.get());
	editorCameraObj->AddComponent(std::move(camComp));

	mainCameraPtr = editorCameraObj.get();
	LOG("[Scene] Instanciada 'EditorCamera' interna en posición (0.0, 1.0, 5.0)");

	// Instance Game Camera (Main Camera para la ventana Game)
	auto gameCamObj = std::make_unique<GameObject>("Main Camera");
	auto gameCamTrans = std::make_unique<TransformComponent>(gameCamObj.get());
	gameCamTrans->SetPosition(glm::vec3(0.0f, 2.0f, 7.0f));
	gameCamTrans->SetRotation(glm::vec3(-10.0f, 0.0f, 0.0f));
	gameCamObj->AddComponent(std::move(gameCamTrans));

	auto gameCamComp = std::make_unique<CameraComponent>(gameCamObj.get());
	gameCamObj->AddComponent(std::move(gameCamComp));

	gameCameraPtr = gameCamObj.get();
	sceneRoot.AddChild(gameCamObj.get());
	sceneEntities.push_back(std::move(gameCamObj));
	LOG("[Scene] Instanciada 'Main Camera' para la vista Game");

	// Create Cube
	auto cubeObj = std::make_unique<GameObject>("Cube");
	auto cubeTrans = std::make_unique<TransformComponent>(cubeObj.get());
	cubeTrans->SetPosition(glm::vec3(-2.0f, 0.0f, 0.0f));
	cubeObj->AddComponent(std::move(cubeTrans));

	auto cubeMesh = std::make_unique<MeshComponent>(cubeObj.get(), GeometryGenerator::CreateCube(1.0f));
	cubeObj->AddComponent(std::move(cubeMesh));

	auto cubeMat = std::make_unique<MaterialComponent>(cubeObj.get());
	cubeMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	cubeMat->SetTexture("Assets/water.jpg");
	cubeMat->SetUseTexture(true);
	cubeObj->AddComponent(std::move(cubeMat));

	// Create Sphere
	auto sphereObj = std::make_unique<GameObject>("Sphere");
	auto sphereTrans = std::make_unique<TransformComponent>(sphereObj.get());
	sphereTrans->SetPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	sphereObj->AddComponent(std::move(sphereTrans));

	auto sphereMesh = std::make_unique<MeshComponent>(sphereObj.get(), GeometryGenerator::CreateSphere(0.5f, 20, 20));
	sphereObj->AddComponent(std::move(sphereMesh));

	auto sphereMat = std::make_unique<MaterialComponent>(sphereObj.get());
	sphereMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	sphereMat->SetTexture("Assets/lava.jpg");
	sphereMat->SetUseTexture(true);
	sphereObj->AddComponent(std::move(sphereMat));

	cubeObj->SetStatic(true);
	sphereObj->SetStatic(true);

	cubeObj->AddChild(sphereObj.get());
	sceneEntities.push_back(std::move(sphereObj));


	sceneRoot.AddChild(cubeObj.get());
	sceneEntities.push_back(std::move(cubeObj));
	LOG("[Scene] Instanciado 'Cube' con geometría procedural de cubo");


	LOG("[Scene] Instanciado 'Sphere' con geometría procedural de esfera y textura");

	// MODELO CASA CON TEXTURA PNG (Soporta múltiples partes)
	GameObject* bakerHouse = ImportModel("Assets/BakerHouse.fbx", &sceneRoot);
	if (bakerHouse)
	{
		TextureImporter::ImportAndAssignToGameObject("Assets/Baker_house.png", bakerHouse);
		if (TransformComponent* bakerHouseTransform = bakerHouse->GetComponent<TransformComponent>())
		{
			bakerHouseTransform->SetPosition(glm::vec3(0.0f, 0.0f, -3.0f));
		}
		bakerHouse->SetStatic(true, true);
	}

	// MODELO CON TEXTURA TGA 
	GameObject* candle = ImportModel("Assets/SM_Candle_Fundida.FBX", &sceneRoot);
	if (candle)
	{
		TextureImporter::ImportAndAssignToGameObject("Assets/T_Candle_D.tga", candle);
		if (TransformComponent* candleTransform = candle->GetComponent<TransformComponent>())
		{
			candleTransform->SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
			candleTransform->SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
			candleTransform->SetScale(glm::vec3(0.05f, 0.05f, 0.05f));
		}
		candle->SetStatic(true, true);
	}

	// MODELO SIN TEXTURA
	GameObject* character = ImportModel("Assets/HeavyTF2_2018.fbx", &sceneRoot);
	if (character)
	{
		TransformComponent* characterTransform = character->GetComponent<TransformComponent>();
		if (characterTransform)
		{
			characterTransform->SetPosition(glm::vec3(-3.0f, 0.0f, -2.0f));
			characterTransform->SetScale(glm::vec3(0.02f, 0.02f, 0.02f));
		}
		character->SetStatic(true, true);
	}

	RebuildSpacePartitioning();
	SetSelected(nullptr);

	currentScenePath.clear();
	sceneDirty = false;
	isLoadingOrInitializing = false;
	UpdateWindowTitle();

	LOG("[Scene] Escena inicial cargada con éxito (%zu entidades registradas)", sceneEntities.size());

	return true;
}

bool Scene::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	ProcessPendingDestructions();
	return true;
}

bool Scene::Update(float dt)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	// Centralización del drag & drop de archivos
	if (Engine::GetInstance().input && Engine::GetInstance().input->HasDroppedFile())
	{
		std::string droppedPath = Engine::GetInstance().input->GetDroppedFilePath();
		HandleDroppedFile(droppedPath);
		Engine::GetInstance().input->ClearDroppedFile();
	}
	if (editorCameraObj)
	{
		editorCameraObj->Update(dt);
	}
	sceneRoot.Update(dt);
	ProcessPendingDestructions();

	if (spacePartitioningDirty)
	{
		RebuildSpacePartitioning();
	}
	return true;
}

bool Scene::PostUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	return true;
}

bool Scene::CleanUp()
{
	LOG("[Scene] Limpiando entidades de la escena (%zu entidades)...", sceneEntities.size());
	const std::string& tempFile = Engine::GetInstance().GetTempSceneFilePath();
	std::error_code ec;
	if (!tempFile.empty() && std::filesystem::exists(tempFile, ec))
	{
		std::filesystem::remove(tempFile, ec);
	}
	quadtree.Clear();
	octree.Clear();
	pendingDestruction.clear();
	selected = nullptr;
	mainCameraPtr = nullptr;
	gameCameraPtr = nullptr;
	editorCameraObj.reset();
	sceneEntities.clear();
	return true;
}

std::string Scene::MakeUniqueName(const std::string& baseName) const
{
	bool exists = false;
	for (const std::unique_ptr<GameObject>& entity : sceneEntities)
	{
		if (entity->GetName() == baseName)
		{
			exists = true;
			break;
		}
	}
	if (!exists) return baseName;

	int index = 1;
	while (true)
	{
		std::string candidate = baseName + " (" + std::to_string(index) + ")";
		bool found = false;
		for (const std::unique_ptr<GameObject>& entity : sceneEntities)
		{
			if (entity->GetName() == candidate)
			{
				found = true;
				break;
			}
		}
		if (!found) return candidate;
		index++;
	}
}

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

GameObject* Scene::Register(std::unique_ptr<GameObject> newObj, GameObject* parent)
{
	GameObject* ptr = newObj.get();
	if (parent != nullptr && (parent == &sceneRoot || OwnsObject(parent)))
	{
		parent->AddChild(ptr);
	}
	else
	{
		sceneRoot.AddChild(ptr);
	}
	sceneEntities.push_back(std::move(newObj));
	spacePartitioningDirty = true;
	if (!isLoadingOrInitializing)
	{
		MarkSceneDirty();
	}
	return ptr;
}

GameObject* Scene::CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name, GameObject* parent)
{
	std::string uniqueName = MakeUniqueName(name);
	auto newObj = std::make_unique<GameObject>(uniqueName);

	std::unordered_set<ComponentType> presentTypes;
	for (std::unique_ptr<Component>& comp : objComponents)
	{
		if (comp)
		{
			presentTypes.insert(comp->GetType());
			newObj->AddComponent(std::move(comp));
		}
	}

	if (presentTypes.find(COMPONENT_TRANSFORM) == presentTypes.end())
	{
		auto newTrans = std::make_unique<TransformComponent>(newObj.get());
		newTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
		newObj->AddComponent(std::move(newTrans));
	}

	if (presentTypes.find(COMPONENT_MESH) == presentTypes.end())
	{
		auto newMesh = std::make_unique<MeshComponent>(newObj.get(), GeometryGenerator::CreateCube(1.0f));
		newObj->AddComponent(std::move(newMesh));
	}

	if (presentTypes.find(COMPONENT_MATERIAL) == presentTypes.end())
	{
		auto cubeMat = std::make_unique<MaterialComponent>(newObj.get());
		cubeMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		cubeMat->SetUseTexture(false);
		newObj->AddComponent(std::move(cubeMat));
	}

	GameObject* registered = Register(std::move(newObj), parent);
	LOG("[Scene] GameObject '%s' creado (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::CreateEmptyGameObject(GameObject* parent)
{
	std::string uniqueName = MakeUniqueName("GameObject");
	auto newObj = std::make_unique<GameObject>(uniqueName);

	auto newTrans = std::make_unique<TransformComponent>(newObj.get());
	newTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	newObj->AddComponent(std::move(newTrans));

	GameObject* registered = Register(std::move(newObj), parent);
	LOG("[Scene] Empty GameObject '%s' creado (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::CreatePrimitive(PrimitiveType type, GameObject* parent)
{
	std::string baseName = GeometryGenerator::GetName(type);
	std::string uniqueName = MakeUniqueName(baseName);
	auto newObj = std::make_unique<GameObject>(uniqueName);

	auto trans = std::make_unique<TransformComponent>(newObj.get());
	trans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	newObj->AddComponent(std::move(trans));

	auto prim = GeometryGenerator::Create(type);
	auto mesh = std::make_unique<MeshComponent>(newObj.get(), std::move(prim));
	newObj->AddComponent(std::move(mesh));

	auto mat = std::make_unique<MaterialComponent>(newObj.get());
	mat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	mat->SetUseTexture(false);
	newObj->AddComponent(std::move(mat));

	GameObject* registered = Register(std::move(newObj), parent);
	LOG("[Scene] Primitiva '%s' creada (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::ImportModel(const std::string& filePath, GameObject* parent)
{
	return MeshImporter::ImportToScene(filePath, parent);
}

void Scene::DestroyGameObject(GameObject* target)
{
	
	if (target == nullptr || target == &sceneRoot) return;

	// Verificar pertenencia real a la escena
	if (!OwnsObject(target)) return;

	std::vector<GameObject*> toDestroy;
	std::stack<GameObject*> stack;
	stack.push(target);

	while (!stack.empty())
	{
		GameObject* node = stack.top();
		stack.pop();
		if (!node) continue;

		toDestroy.push_back(node);
		for (GameObject* child : node->GetChildren())
		{
			stack.push(child);
		}
	}
	std::reverse(toDestroy.begin(), toDestroy.end());

	for (GameObject* obj : toDestroy)
	{
		if (selected == obj)
		{
			SetSelected(nullptr);
			break;
		}
	}

	// Si la cámara principal estuviera en el subárbol, desvincularla para no dejar puntero colgante
	for (GameObject* obj : toDestroy)
	{
		if (mainCameraPtr == obj) mainCameraPtr = nullptr;
		if (gameCameraPtr == obj) gameCameraPtr = nullptr;
	}

	// Desvinculación inmediata del padre superior
	if (target->GetParent())
	{
		target->GetParent()->RemoveChild(target);
	}

	for (GameObject* obj : toDestroy)
	{
		if (std::find(pendingDestruction.begin(), pendingDestruction.end(), obj) == pendingDestruction.end())
		{
			pendingDestruction.push_back(obj);
		}
	}

	LOG("[Scene] GameObject '%s' y %zu descendiente(s) marcados para eliminación segura",
		target->GetName().c_str(), toDestroy.size() - 1);
	MarkSceneDirty();
}

void Scene::ProcessPendingDestructions()
{
	if (pendingDestruction.empty()) return;

	std::unordered_set<GameObject*> destroySet(pendingDestruction.begin(), pendingDestruction.end());
	pendingDestruction.clear();

	// Desvincular relaciones padre-hijo internas antes de la destrucción de memoria
	for (GameObject* obj : destroySet)
	{
		if (obj->GetParent() && destroySet.find(obj->GetParent()) == destroySet.end())
		{
			obj->GetParent()->RemoveChild(obj);
		}
	}

	const size_t initialCount = sceneEntities.size();
	sceneEntities.erase(
		std::remove_if(sceneEntities.begin(), sceneEntities.end(), [&](const std::unique_ptr<GameObject>& ptr)
		{
			return destroySet.find(ptr.get()) != destroySet.end();
		}),
		sceneEntities.end()
	);

	LOG("[Scene] Liberados %zu GameObjects de la memoria de la escena (%zu restantes)",
		initialCount - sceneEntities.size(), sceneEntities.size());
	spacePartitioningDirty = true;
}

GameObject* Scene::DuplicateGameObject(GameObject* target, GameObject* parent)
{
	if (target == nullptr || !OwnsObject(target))
	{
		LOG_WARNING("[Scene] No se puede duplicar: GameObject nulo o no pertenece a la escena.");
		return nullptr;
	}

	GameObject* targetParent = (parent != nullptr) ? parent : target->GetParent();
	if (targetParent == nullptr)
	{
		targetParent = &sceneRoot;
	}

	struct DuplicateTask
	{
		GameObject* src;
		GameObject* parentNode;
	};

	std::stack<DuplicateTask> stack;
	stack.push({ target, targetParent });
	GameObject* duplicatedRoot = nullptr;

	while (!stack.empty())
	{
		DuplicateTask current = stack.top();
		stack.pop();

		GameObject* src = current.src;
		GameObject* parentNode = current.parentNode;
		if (!src) continue;

		TransformComponent* srcTrans = src->GetComponent<TransformComponent>();
		glm::vec3 localPos = srcTrans ? srcTrans->GetPosition() : glm::vec3(0.0f);
		glm::vec3 localRot = srcTrans ? srcTrans->GetRotation() : glm::vec3(0.0f);
		glm::vec3 localScale = srcTrans ? srcTrans->GetScale() : glm::vec3(1.0f);

		std::unique_ptr<GameObject> clone = src->Duplicate();
		clone->SetName(MakeUniqueName(src->GetName()));
		GameObject* registeredClone = Register(std::move(clone), parentNode);

		if (TransformComponent* cloneTrans = registeredClone->GetComponent<TransformComponent>())
		{
			cloneTrans->SetPosition(localPos);
			cloneTrans->SetRotation(localRot);
			cloneTrans->SetScale(localScale);
		}

		if (duplicatedRoot == nullptr)
		{
			duplicatedRoot = registeredClone;
		}

		const std::vector<GameObject*>& children = src->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			stack.push({ children[i - 1], registeredClone });
		}
	}

	SetSelected(duplicatedRoot);
	if (duplicatedRoot)
	{
		LOG("[Scene] GameObject '%s' duplicado como '%s' (Total entidades: %zu)",
			target->GetName().c_str(), duplicatedRoot->GetName().c_str(), sceneEntities.size());
	}

	return duplicatedRoot;
}

void Scene::HandleDroppedFile(const std::string& filePath)
{
	ImporterManager::ImportFile(filePath, selected);
	spacePartitioningDirty = true;
}

void Scene::SetTreeParameters(int maxObjectsPerNode, int maxDepth)
{
	maxObjectsPerNode = std::max(1, maxObjectsPerNode);
	maxDepth = std::max(1, maxDepth);
	if (treeMaxObjectsPerNode != maxObjectsPerNode || treeMaxDepth != maxDepth)
	{
		treeMaxObjectsPerNode = maxObjectsPerNode;
		treeMaxDepth = maxDepth;
		RebuildSpacePartitioning();
		MarkSceneDirty();
	}
}

void Scene::RebuildSpacePartitioning()
{
	std::vector<Quadtree::Entry> staticEntries;
	AABB worldBounds(glm::vec3(-8.0f, -2.0f, -8.0f), glm::vec3(8.0f, 8.0f, 8.0f));
	bool firstBox = true;

	std::stack<GameObject*> stack;
	stack.push(&sceneRoot);

	while (!stack.empty())
	{
		GameObject* node = stack.top();
		stack.pop();
		if (!node || !node->IsActive()) continue;

		if (node != &sceneRoot && node->IsStatic() && node->HasWorldAABB())
		{
			AABB box = node->GetWorldAABB();
			staticEntries.push_back({ node, box });

			if (firstBox)
			{
				worldBounds = box;
				firstBox = false;
			}
			else
			{
				worldBounds.Encapsulate(box);
			}
		}

		const std::vector<GameObject*>& children = node->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			stack.push(children[i - 1]);
		}
	}

	// Asegurar un volumen mínimo simétrico y un pequeño margen alrededor de los objetos estáticos
	worldBounds.Encapsulate(AABB(glm::vec3(-6.0f, -1.0f, -6.0f), glm::vec3(6.0f, 6.0f, 6.0f)));
	worldBounds.minPoint -= glm::vec3(0.5f);
	worldBounds.maxPoint += glm::vec3(0.5f);

	quadtree.Build(worldBounds, staticEntries, treeMaxObjectsPerNode, treeMaxDepth);
	octree.Build(worldBounds, staticEntries, treeMaxObjectsPerNode, treeMaxDepth);
	spacePartitioningDirty = false;

	LOG("[Scene] Space Partitioning reconstruido (%zu objetos Static): Quadtree=%zu nodos, Octree=%zu nodos",
		staticEntries.size(), quadtree.GetNodeCount(), octree.GetNodeCount());
}

void Scene::SetCurrentScenePath(const std::string& path)
{
	currentScenePath = path;
	UpdateWindowTitle();
}

std::string Scene::GetSceneDisplayName() const
{
	if (currentScenePath.empty())
	{
		return "Untitled";
	}
	std::filesystem::path p(currentScenePath);
	std::string stem = p.stem().string();
	return stem.empty() ? "Untitled" : stem;
}

void Scene::MarkSceneDirty()
{
	if (isLoadingOrInitializing) return;
	if (!sceneDirty)
	{
		sceneDirty = true;
		UpdateWindowTitle();
	}
}

void Scene::ClearSceneDirty()
{
	if (sceneDirty)
	{
		sceneDirty = false;
		UpdateWindowTitle();
	}
}

void Scene::UpdateWindowTitle() const
{
	if (Engine::GetInstance().window)
	{
		std::string suffix = " - " + GetSceneDisplayName();
		if (sceneDirty)
		{
			suffix += "*";
		}
		Engine::GetInstance().window->SetSceneTitleInfo(suffix);
	}
}

void Scene::NewScene()
{
	isLoadingOrInitializing = true;
	pendingDestruction.clear();
	selected = nullptr;
	gameCameraPtr = nullptr;

	while (!sceneRoot.GetChildren().empty())
	{
		sceneRoot.RemoveChild(sceneRoot.GetChildren().back());
	}
	sceneEntities.clear();

	// Instanciar Main Camera por defecto en la nueva escena
	auto gameCamObj = std::make_unique<GameObject>("Main Camera");
	auto gameCamTrans = std::make_unique<TransformComponent>(gameCamObj.get());
	gameCamTrans->SetPosition(glm::vec3(0.0f, 2.0f, 7.0f));
	gameCamTrans->SetRotation(glm::vec3(-10.0f, 0.0f, 0.0f));
	gameCamObj->AddComponent(std::move(gameCamTrans));

	auto gameCamComp = std::make_unique<CameraComponent>(gameCamObj.get());
	gameCamObj->AddComponent(std::move(gameCamComp));

	gameCameraPtr = Register(std::move(gameCamObj), &sceneRoot);
	selectionRevision++;
	RebuildSpacePartitioning();

	currentScenePath.clear();
	sceneDirty = false;
	isLoadingOrInitializing = false;
	UpdateWindowTitle();

	LOG("[Scene] Nueva escena creada ('Untitled', 1 entidad: 'Main Camera').");
}

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
				targetSavePath = SceneFile::ResolveProjectRoot() + "/" + norm;
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
		currentScenePath = SceneFile::ToProjectRelativePath(targetSavePath);
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
	std::string resolvedScenePath = isTempFile ? filePath : SceneFile::ResolveExistingPath(filePath);

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
		currentScenePath = SceneFile::ToProjectRelativePath(resolvedScenePath);
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

// ============================================================================
// AABB
// ============================================================================
void AABB::Encapsulate(const glm::vec3& point)
{
	minPoint = glm::min(minPoint, point);
	maxPoint = glm::max(maxPoint, point);
}

void AABB::Encapsulate(const AABB& other)
{
	minPoint = glm::min(minPoint, other.minPoint);
	maxPoint = glm::max(maxPoint, other.maxPoint);
}

bool AABB::Intersects(const AABB& other) const
{
	return (minPoint.x <= other.maxPoint.x && maxPoint.x >= other.minPoint.x) &&
		   (minPoint.y <= other.maxPoint.y && maxPoint.y >= other.minPoint.y) &&
		   (minPoint.z <= other.maxPoint.z && maxPoint.z >= other.minPoint.z);
}

bool AABB::Intersects2DXZ(const AABB& other) const
{
	return (minPoint.x <= other.maxPoint.x && maxPoint.x >= other.minPoint.x) &&
		   (minPoint.z <= other.maxPoint.z && maxPoint.z >= other.minPoint.z);
}

bool AABB::IntersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float& tMinOut) const
{
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;

	const float eps = 1e-7f;
	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::fabs(rayDir[axis]) < eps)
		{
			if (rayOrigin[axis] < minPoint[axis] || rayOrigin[axis] > maxPoint[axis])
			{
				return false;
			}
		}
		else
		{
			float invD = 1.0f / rayDir[axis];
			float t0 = (minPoint[axis] - rayOrigin[axis]) * invD;
			float t1 = (maxPoint[axis] - rayOrigin[axis]) * invD;
			if (t0 > t1) std::swap(t0, t1);

			tmin = std::max(tmin, t0);
			tmax = std::min(tmax, t1);
			if (tmax < tmin) return false;
		}
	}

	if (tmax >= 0.0f)
	{
		tMinOut = (tmin >= 0.0f) ? tmin : tmax;
		return true;
	}
	return false;
}

AABB AABB::FromTransformedLocalBox(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& worldMatrix)
{
	const glm::vec3 corners[8] = {
		glm::vec3(localMin.x, localMin.y, localMin.z),
		glm::vec3(localMax.x, localMin.y, localMin.z),
		glm::vec3(localMin.x, localMax.y, localMin.z),
		glm::vec3(localMax.x, localMax.y, localMin.z),
		glm::vec3(localMin.x, localMin.y, localMax.z),
		glm::vec3(localMax.x, localMin.y, localMax.z),
		glm::vec3(localMin.x, localMax.y, localMax.z),
		glm::vec3(localMax.x, localMax.y, localMax.z)
	};

	glm::vec3 firstWorld = glm::vec3(worldMatrix * glm::vec4(corners[0], 1.0f));
	AABB result(firstWorld, firstWorld);

	for (int i = 1; i < 8; ++i)
	{
		glm::vec3 worldCorner = glm::vec3(worldMatrix * glm::vec4(corners[i], 1.0f));
		result.Encapsulate(worldCorner);
	}

	result.minPoint -= glm::vec3(0.001f);
	result.maxPoint += glm::vec3(0.001f);
	return result;
}

// ============================================================================
// FrustumPlane & Frustum
// ============================================================================
void FrustumPlane::Normalize()
{
	float len = glm::length(normal);
	if (len > 1e-6f)
	{
		normal /= len;
		d /= len;
	}
}

Frustum Frustum::FromViewProjection(const glm::mat4& m)
{
	Frustum frustum;

	// Left
	frustum.planes[0].normal.x = m[0][3] + m[0][0];
	frustum.planes[0].normal.y = m[1][3] + m[1][0];
	frustum.planes[0].normal.z = m[2][3] + m[2][0];
	frustum.planes[0].d        = m[3][3] + m[3][0];

	// Right
	frustum.planes[1].normal.x = m[0][3] - m[0][0];
	frustum.planes[1].normal.y = m[1][3] - m[1][0];
	frustum.planes[1].normal.z = m[2][3] - m[2][0];
	frustum.planes[1].d        = m[3][3] - m[3][0];

	// Bottom
	frustum.planes[2].normal.x = m[0][3] + m[0][1];
	frustum.planes[2].normal.y = m[1][3] + m[1][1];
	frustum.planes[2].normal.z = m[2][3] + m[2][1];
	frustum.planes[2].d        = m[3][3] + m[3][1];

	// Top
	frustum.planes[3].normal.x = m[0][3] - m[0][1];
	frustum.planes[3].normal.y = m[1][3] - m[1][1];
	frustum.planes[3].normal.z = m[2][3] - m[2][1];
	frustum.planes[3].d        = m[3][3] - m[3][1];

	// Near
	frustum.planes[4].normal.x = m[0][3] + m[0][2];
	frustum.planes[4].normal.y = m[1][3] + m[1][2];
	frustum.planes[4].normal.z = m[2][3] + m[2][2];
	frustum.planes[4].d        = m[3][3] + m[3][2];

	// Far
	frustum.planes[5].normal.x = m[0][3] - m[0][2];
	frustum.planes[5].normal.y = m[1][3] - m[1][2];
	frustum.planes[5].normal.z = m[2][3] - m[2][2];
	frustum.planes[5].d        = m[3][3] - m[3][2];

	for (size_t i = 0; i < frustum.planes.size(); ++i)
	{
		frustum.planes[i].Normalize();
	}

	return frustum;
}

bool Frustum::IntersectsAABB(const AABB& box) const
{
	for (size_t i = 0; i < planes.size(); ++i)
	{
		const FrustumPlane& plane = planes[i];
		glm::vec3 positiveVertex(
			(plane.normal.x >= 0.0f) ? box.maxPoint.x : box.minPoint.x,
			(plane.normal.y >= 0.0f) ? box.maxPoint.y : box.minPoint.y,
			(plane.normal.z >= 0.0f) ? box.maxPoint.z : box.minPoint.z
		);

		if (plane.DistanceToPoint(positiveVertex) < 0.0f)
		{
			return false;
		}
	}
	return true;
}

std::array<glm::vec3, 8> Frustum::ExtractCorners(const glm::mat4& viewProj)
{
	glm::mat4 invVP = glm::inverse(viewProj);
	const glm::vec4 ndcCorners[8] = {
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f),
		glm::vec4( 1.0f, -1.0f, -1.0f, 1.0f),
		glm::vec4( 1.0f,  1.0f, -1.0f, 1.0f),
		glm::vec4(-1.0f,  1.0f, -1.0f, 1.0f),
		glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f),
		glm::vec4( 1.0f, -1.0f,  1.0f, 1.0f),
		glm::vec4( 1.0f,  1.0f,  1.0f, 1.0f),
		glm::vec4(-1.0f,  1.0f,  1.0f, 1.0f)
	};

	std::array<glm::vec3, 8> worldCorners;
	for (size_t i = 0; i < 8; ++i)
	{
		glm::vec4 worldPt = invVP * ndcCorners[i];
		if (std::fabs(worldPt.w) > 1e-6f)
		{
			worldCorners[i] = glm::vec3(worldPt) / worldPt.w;
		}
		else
		{
			worldCorners[i] = glm::vec3(worldPt);
		}
	}
	return worldCorners;
}

// ============================================================================
// QUADTREE (Iterativo con std::stack)
// ============================================================================
void Quadtree::Clear()
{
	if (!root)
	{
		totalNodes = 0;
		totalObjects = 0;
		return;
	}

	std::stack<std::unique_ptr<Node>> cleanupStack;
	cleanupStack.push(std::move(root));

	while (!cleanupStack.empty())
	{
		std::unique_ptr<Node> current = std::move(cleanupStack.top());
		cleanupStack.pop();
		if (!current) continue;

		for (size_t i = 0; i < 4; ++i)
		{
			if (current->children[i])
			{
				cleanupStack.push(std::move(current->children[i]));
			}
		}
	}

	totalNodes = 0;
	totalObjects = 0;
}

void Quadtree::SubdivideNode(Node* node)
{
	if (!node || !node->IsLeaf()) return;

	const glm::vec3 minP = node->bounds.minPoint;
	const glm::vec3 maxP = node->bounds.maxPoint;
	const glm::vec3 center = node->bounds.GetCenter();
	const int nextDepth = node->depth + 1;

	node->children[0] = std::make_unique<Node>();
	node->children[0]->bounds = AABB(glm::vec3(minP.x, minP.y, minP.z), glm::vec3(center.x, maxP.y, center.z));
	node->children[0]->depth = nextDepth;

	node->children[1] = std::make_unique<Node>();
	node->children[1]->bounds = AABB(glm::vec3(center.x, minP.y, minP.z), glm::vec3(maxP.x, maxP.y, center.z));
	node->children[1]->depth = nextDepth;

	node->children[2] = std::make_unique<Node>();
	node->children[2]->bounds = AABB(glm::vec3(minP.x, minP.y, center.z), glm::vec3(center.x, maxP.y, maxP.z));
	node->children[2]->depth = nextDepth;

	node->children[3] = std::make_unique<Node>();
	node->children[3]->bounds = AABB(glm::vec3(center.x, minP.y, center.z), glm::vec3(maxP.x, maxP.y, maxP.z));
	node->children[3]->depth = nextDepth;

	totalNodes += 4;
}

void Quadtree::Build(const AABB& worldBounds, const std::vector<Entry>& staticEntries, int maxObjectsPerNode, int maxDepth)
{
	Clear();

	maxObjects = std::max(1, maxObjectsPerNode);
	maxTreeDepth = std::max(1, maxDepth);
	totalObjects = staticEntries.size();

	root = std::make_unique<Node>();
	root->bounds = worldBounds;
	root->depth = 0;
	root->objects = staticEntries;
	totalNodes = 1;

	std::stack<Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		Node* current = stack.top();
		stack.pop();
		if (!current) continue;

		if ((int)current->objects.size() > maxObjects && current->depth < maxTreeDepth)
		{
			SubdivideNode(current);

			std::vector<Entry> currentEntries = std::move(current->objects);
			current->objects.clear();

			for (const Entry& entry : currentEntries)
			{
				for (size_t c = 0; c < 4; ++c)
				{
					if (current->children[c]->bounds.Intersects2DXZ(entry.box))
					{
						current->children[c]->objects.push_back(entry);
					}
				}
			}

			for (size_t c = 4; c > 0; --c)
			{
				Node* child = current->children[c - 1].get();
				if ((int)child->objects.size() > maxObjects && child->objects.size() < currentEntries.size())
				{
					stack.push(child);
				}
			}
		}
	}
}

int Quadtree::QueryFrustum(const Frustum& frustum, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		if (!frustum.IntersectsAABB(node->bounds))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					if (frustum.IntersectsAABB(entry.box))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 4; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

int Quadtree::QueryRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		float tHit = 0.0f;
		if (!node->bounds.IntersectsRay(rayOrigin, rayDir, tHit))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					float tObj = 0.0f;
					if (entry.box.IntersectsRay(rayOrigin, rayDir, tObj))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 4; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

void Quadtree::CollectNodeBounds(std::vector<AABB>& outBoxes) const
{
	outBoxes.clear();
	if (!root) return;

	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		outBoxes.push_back(node->bounds);

		if (!node->IsLeaf())
		{
			for (size_t i = 4; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}
}

// ============================================================================
// OCTREE (Iterativo con std::stack)
// ============================================================================
void Octree::Clear()
{
	if (!root)
	{
		totalNodes = 0;
		totalObjects = 0;
		return;
	}

	std::stack<std::unique_ptr<Node>> cleanupStack;
	cleanupStack.push(std::move(root));

	while (!cleanupStack.empty())
	{
		std::unique_ptr<Node> current = std::move(cleanupStack.top());
		cleanupStack.pop();
		if (!current) continue;

		for (size_t i = 0; i < 8; ++i)
		{
			if (current->children[i])
			{
				cleanupStack.push(std::move(current->children[i]));
			}
		}
	}

	totalNodes = 0;
	totalObjects = 0;
}

void Octree::SubdivideNode(Node* node)
{
	if (!node || !node->IsLeaf()) return;

	const glm::vec3 minP = node->bounds.minPoint;
	const glm::vec3 maxP = node->bounds.maxPoint;
	const glm::vec3 center = node->bounds.GetCenter();
	const int nextDepth = node->depth + 1;

	for (size_t i = 0; i < 8; ++i)
	{
		glm::vec3 childMin(
			(i & 1) ? center.x : minP.x,
			(i & 2) ? center.y : minP.y,
			(i & 4) ? center.z : minP.z
		);
		glm::vec3 childMax(
			(i & 1) ? maxP.x : center.x,
			(i & 2) ? maxP.y : center.y,
			(i & 4) ? maxP.z : center.z
		);

		node->children[i] = std::make_unique<Node>();
		node->children[i]->bounds = AABB(childMin, childMax);
		node->children[i]->depth = nextDepth;
	}

	totalNodes += 8;
}

void Octree::Build(const AABB& worldBounds, const std::vector<Entry>& staticEntries, int maxObjectsPerNode, int maxDepth)
{
	Clear();

	maxObjects = std::max(1, maxObjectsPerNode);
	maxTreeDepth = std::max(1, maxDepth);
	totalObjects = staticEntries.size();

	root = std::make_unique<Node>();
	root->bounds = worldBounds;
	root->depth = 0;
	root->objects = staticEntries;
	totalNodes = 1;

	std::stack<Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		Node* current = stack.top();
		stack.pop();
		if (!current) continue;

		if ((int)current->objects.size() > maxObjects && current->depth < maxTreeDepth)
		{
			SubdivideNode(current);

			std::vector<Entry> currentEntries = std::move(current->objects);
			current->objects.clear();

			for (const Entry& entry : currentEntries)
			{
				for (size_t c = 0; c < 8; ++c)
				{
					if (current->children[c]->bounds.Intersects(entry.box))
					{
						current->children[c]->objects.push_back(entry);
					}
				}
			}

			for (size_t c = 8; c > 0; --c)
			{
				Node* child = current->children[c - 1].get();
				if ((int)child->objects.size() > maxObjects && child->objects.size() < currentEntries.size())
				{
					stack.push(child);
				}
			}
		}
	}
}

int Octree::QueryFrustum(const Frustum& frustum, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		if (!frustum.IntersectsAABB(node->bounds))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					if (frustum.IntersectsAABB(entry.box))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 8; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

int Octree::QueryRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		float tHit = 0.0f;
		if (!node->bounds.IntersectsRay(rayOrigin, rayDir, tHit))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					float tObj = 0.0f;
					if (entry.box.IntersectsRay(rayOrigin, rayDir, tObj))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 8; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

void Octree::CollectNodeBounds(std::vector<AABB>& outBoxes) const
{
	outBoxes.clear();
	if (!root) return;

	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		outBoxes.push_back(node->bounds);

		if (!node->IsLeaf())
		{
			for (size_t i = 8; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}
}

// ============================================================================
// SCENEFILE (Serialización de Escena en JSON con RapidJSON)
// ============================================================================
struct SceneFile::Node
{
	enum class Type
	{
		Null,
		Bool,
		Int,
		UInt64,
		Float,
		String,
		Object,
		Array
	};

	Type type = Type::Object;
	bool boolVal = false;
	int intVal = 0;
	uint64_t uint64Val = 0;
	double floatVal = 0.0;
	std::string stringVal;

	std::vector<std::pair<std::string, std::shared_ptr<Node>>> members;
	std::vector<std::shared_ptr<Node>> elements;

	std::shared_ptr<Node> FindMember(const char* key) const
	{
		if (!key) return nullptr;
		for (size_t i = 0; i < members.size(); ++i)
		{
			if (members[i].first == key)
			{
				return members[i].second;
			}
		}
		return nullptr;
	}

	std::shared_ptr<Node> SetOrCreateMember(const char* key, Type memberType)
	{
		if (type != Type::Object)
		{
			type = Type::Object;
		}
		for (size_t i = 0; i < members.size(); ++i)
		{
			if (members[i].first == key)
			{
				members[i].second = std::make_shared<Node>();
				members[i].second->type = memberType;
				return members[i].second;
			}
		}
		std::shared_ptr<Node> child = std::make_shared<Node>();
		child->type = memberType;
		members.push_back(std::make_pair(std::string(key ? key : ""), child));
		return child;
	}
};

namespace
{
	void WriteNodeToRapidJson(const SceneFile::Node* n, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
	{
		if (!n)
		{
			writer.Null();
			return;
		}

		switch (n->type)
		{
		case SceneFile::Node::Type::Null:
			writer.Null();
			break;
		case SceneFile::Node::Type::Bool:
			writer.Bool(n->boolVal);
			break;
		case SceneFile::Node::Type::Int:
			writer.Int(n->intVal);
			break;
		case SceneFile::Node::Type::UInt64:
			writer.Uint64(n->uint64Val);
			break;
		case SceneFile::Node::Type::Float:
			writer.Double(n->floatVal);
			break;
		case SceneFile::Node::Type::String:
			writer.String(n->stringVal.c_str(), (rapidjson::SizeType)n->stringVal.size());
			break;
		case SceneFile::Node::Type::Object:
			writer.StartObject();
			for (size_t i = 0; i < n->members.size(); ++i)
			{
				const std::string& k = n->members[i].first;
				writer.Key(k.c_str(), (rapidjson::SizeType)k.size());
				WriteNodeToRapidJson(n->members[i].second.get(), writer);
			}
			writer.EndObject();
			break;
		case SceneFile::Node::Type::Array:
			writer.StartArray();
			for (size_t i = 0; i < n->elements.size(); ++i)
			{
				WriteNodeToRapidJson(n->elements[i].get(), writer);
			}
			writer.EndArray();
			break;
		}
	}

	std::shared_ptr<SceneFile::Node> ReadNodeFromRapidJson(const rapidjson::Value& val)
	{
		std::shared_ptr<SceneFile::Node> n = std::make_shared<SceneFile::Node>();
		if (val.IsNull())
		{
			n->type = SceneFile::Node::Type::Null;
		}
		else if (val.IsBool())
		{
			n->type = SceneFile::Node::Type::Bool;
			n->boolVal = val.GetBool();
		}
		else if (val.IsInt())
		{
			n->type = SceneFile::Node::Type::Int;
			n->intVal = val.GetInt();
			n->uint64Val = (uint64_t)(n->intVal >= 0 ? n->intVal : 0);
			n->floatVal = (double)n->intVal;
		}
		else if (val.IsUint64())
		{
			n->type = SceneFile::Node::Type::UInt64;
			n->uint64Val = val.GetUint64();
			n->intVal = (int)n->uint64Val;
			n->floatVal = (double)n->uint64Val;
		}
		else if (val.IsInt64())
		{
			int64_t v = val.GetInt64();
			n->type = SceneFile::Node::Type::Int;
			n->intVal = (int)v;
			n->uint64Val = (uint64_t)v;
			n->floatVal = (double)v;
		}
		else if (val.IsNumber())
		{
			n->type = SceneFile::Node::Type::Float;
			n->floatVal = val.GetDouble();
			n->intVal = (int)n->floatVal;
		}
		else if (val.IsString())
		{
			n->type = SceneFile::Node::Type::String;
			n->stringVal = std::string(val.GetString(), val.GetStringLength());
		}
		else if (val.IsObject())
		{
			n->type = SceneFile::Node::Type::Object;
			for (rapidjson::Value::ConstMemberIterator it = val.MemberBegin(); it != val.MemberEnd(); ++it)
			{
				std::string k(it->name.GetString(), it->name.GetStringLength());
				n->members.push_back(std::make_pair(k, ReadNodeFromRapidJson(it->value)));
			}
		}
		else if (val.IsArray())
		{
			n->type = SceneFile::Node::Type::Array;
			n->elements.reserve(val.Size());
			for (rapidjson::SizeType i = 0; i < val.Size(); ++i)
			{
				n->elements.push_back(ReadNodeFromRapidJson(val[i]));
			}
		}
		return n;
	}
}

SceneFile::SceneFile()
	: node(std::make_shared<Node>())
{
	node->type = Node::Type::Object;
}

SceneFile::SceneFile(std::shared_ptr<Node> n)
	: node(std::move(n))
{
}

SceneFile::~SceneFile() = default;

SceneFile::SceneFile(const SceneFile& other) = default;
SceneFile& SceneFile::operator=(const SceneFile& other) = default;

bool SceneFile::IsValid() const
{
	return node != nullptr && node->type != Node::Type::Null;
}

bool SceneFile::LoadFromFile(const std::string& filePath)
{
	std::string resolvedPath = ResolveExistingPath(filePath);
	std::ifstream in(resolvedPath, std::ios::binary);
	if (!in.is_open())
	{
		LOG_ERROR("[SceneFile] No se pudo abrir el archivo de escena: '%s'", filePath.c_str());
		return false;
	}

	std::ostringstream ss;
	ss << in.rdbuf();
	std::string content = ss.str();
	in.close();

	if (content.empty())
	{
		LOG_ERROR("[SceneFile] El archivo de escena está vacío: '%s'", filePath.c_str());
		return false;
	}

	rapidjson::Document doc;
	doc.Parse(content.c_str(), content.size());
	if (doc.HasParseError() || !doc.IsObject())
	{
		LOG_ERROR("[SceneFile] Error al parsear JSON en '%s' (offset %zu): %s",
			filePath.c_str(), doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError()));
		return false;
	}

	node = ReadNodeFromRapidJson(doc);
	return true;
}

bool SceneFile::SaveToFile(const std::string& filePath) const
{
	if (!node) return false;

	std::filesystem::path targetPath(filePath);
	if (targetPath.has_parent_path())
	{
		std::error_code ec;
		std::filesystem::create_directories(targetPath.parent_path(), ec);
	}

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	writer.SetIndent(' ', 4);

	WriteNodeToRapidJson(node.get(), writer);

	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
	{
		LOG_ERROR("[SceneFile] No se pudo escribir el archivo de escena: '%s'", filePath.c_str());
		return false;
	}

	out.write(buffer.GetString(), (std::streamsize)buffer.GetSize());
	out.close();
	return true;
}

void SceneFile::SetBool(const char* key, bool value)
{
	if (!node) return;
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::Bool);
	child->boolVal = value;
}

void SceneFile::SetInt(const char* key, int value)
{
	if (!node) return;
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::Int);
	child->intVal = value;
	child->uint64Val = (uint64_t)(value >= 0 ? value : 0);
	child->floatVal = (double)value;
}

void SceneFile::SetUInt64(const char* key, uint64_t value)
{
	if (!node) return;
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::UInt64);
	child->uint64Val = value;
	child->intVal = (int)value;
	child->floatVal = (double)value;
}

void SceneFile::SetFloat(const char* key, float value)
{
	if (!node) return;
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::Float);
	child->floatVal = (double)value;
	child->intVal = (int)value;
}

void SceneFile::SetString(const char* key, const std::string& value)
{
	if (!node) return;
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::String);
	child->stringVal = value;
}

void SceneFile::SetVec3(const char* key, const glm::vec3& value)
{
	if (!node) return;
	std::shared_ptr<Node> arr = node->SetOrCreateMember(key, Node::Type::Array);
	arr->elements.clear();
	for (int i = 0; i < 3; ++i)
	{
		std::shared_ptr<Node> elem = std::make_shared<Node>();
		elem->type = Node::Type::Float;
		elem->floatVal = (double)value[i];
		arr->elements.push_back(elem);
	}
}

void SceneFile::SetVec4(const char* key, const glm::vec4& value)
{
	if (!node) return;
	std::shared_ptr<Node> arr = node->SetOrCreateMember(key, Node::Type::Array);
	arr->elements.clear();
	for (int i = 0; i < 4; ++i)
	{
		std::shared_ptr<Node> elem = std::make_shared<Node>();
		elem->type = Node::Type::Float;
		elem->floatVal = (double)value[i];
		arr->elements.push_back(elem);
	}
}

bool SceneFile::HasKey(const char* key) const
{
	if (!node) return false;
	return node->FindMember(key) != nullptr;
}

bool SceneFile::GetBool(const char* key, bool defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child || child->type != Node::Type::Bool) return defaultVal;
	return child->boolVal;
}

int SceneFile::GetInt(const char* key, int defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child) return defaultVal;
	if (child->type == Node::Type::Int || child->type == Node::Type::UInt64 || child->type == Node::Type::Float)
	{
		return child->intVal;
	}
	return defaultVal;
}

uint64_t SceneFile::GetUInt64(const char* key, uint64_t defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child) return defaultVal;
	if (child->type == Node::Type::UInt64 || child->type == Node::Type::Int)
	{
		return child->uint64Val;
	}
	return defaultVal;
}

float SceneFile::GetFloat(const char* key, float defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child) return defaultVal;
	if (child->type == Node::Type::Float || child->type == Node::Type::Int || child->type == Node::Type::UInt64)
	{
		return (float)child->floatVal;
	}
	return defaultVal;
}

std::string SceneFile::GetString(const char* key, const std::string& defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child || child->type != Node::Type::String) return defaultVal;
	return child->stringVal;
}

glm::vec3 SceneFile::GetVec3(const char* key, const glm::vec3& defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child || child->type != Node::Type::Array || child->elements.size() < 3) return defaultVal;
	return glm::vec3(
		(float)child->elements[0]->floatVal,
		(float)child->elements[1]->floatVal,
		(float)child->elements[2]->floatVal
	);
}

glm::vec4 SceneFile::GetVec4(const char* key, const glm::vec4& defaultVal) const
{
	if (!node) return defaultVal;
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child || child->type != Node::Type::Array || child->elements.size() < 4) return defaultVal;
	return glm::vec4(
		(float)child->elements[0]->floatVal,
		(float)child->elements[1]->floatVal,
		(float)child->elements[2]->floatVal,
		(float)child->elements[3]->floatVal
	);
}

SceneFile SceneFile::AddObject(const char* key)
{
	if (!node) return SceneFile(nullptr);
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::Object);
	return SceneFile(child);
}

SceneFile SceneFile::GetObjectNode(const char* key) const
{
	if (!node) return SceneFile(nullptr);
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child || child->type != Node::Type::Object) return SceneFile(nullptr);
	return SceneFile(child);
}

SceneFile SceneFile::AddArray(const char* key)
{
	if (!node) return SceneFile(nullptr);
	std::shared_ptr<Node> child = node->SetOrCreateMember(key, Node::Type::Array);
	return SceneFile(child);
}

SceneFile SceneFile::GetArray(const char* key) const
{
	if (!node) return SceneFile(nullptr);
	std::shared_ptr<Node> child = node->FindMember(key);
	if (!child || child->type != Node::Type::Array) return SceneFile(nullptr);
	return SceneFile(child);
}

SceneFile SceneFile::PushObject()
{
	if (!node) return SceneFile(nullptr);
	if (node->type != Node::Type::Array)
	{
		node->type = Node::Type::Array;
	}
	std::shared_ptr<Node> elem = std::make_shared<Node>();
	elem->type = Node::Type::Object;
	node->elements.push_back(elem);
	return SceneFile(elem);
}

size_t SceneFile::Size() const
{
	if (!node || node->type != Node::Type::Array) return 0;
	return node->elements.size();
}

SceneFile SceneFile::GetElement(size_t index) const
{
	if (!node || node->type != Node::Type::Array || index >= node->elements.size())
	{
		return SceneFile(nullptr);
	}
	return SceneFile(node->elements[index]);
}

namespace
{
	std::string NormalizeSlashes(const std::string& pathStr)
	{
		std::string out = pathStr;
		std::replace(out.begin(), out.end(), '\\', '/');
		while (out.size() > 1 && out.back() == '/')
		{
			out.pop_back();
		}
		return out;
	}

	std::string ToLowerCopy(const std::string& s)
	{
		std::string out = s;
		for (size_t i = 0; i < out.size(); ++i)
		{
			out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
		}
		return out;
	}

	std::string ExtractFilenamePortable(const std::string& rawPath)
	{
		std::string norm = NormalizeSlashes(rawPath);
		size_t slashPos = norm.find_last_of('/');
		if (slashPos != std::string::npos)
		{
			return norm.substr(slashPos + 1);
		}
		return norm;
	}

	bool AreFilesIdentical(const std::filesystem::path& pathA, const std::filesystem::path& pathB)
	{
		std::error_code ec;
		if (std::filesystem::equivalent(pathA, pathB, ec)) return true;

		uintmax_t sizeA = std::filesystem::file_size(pathA, ec);
		if (ec) return false;
		uintmax_t sizeB = std::filesystem::file_size(pathB, ec);
		if (ec || sizeA != sizeB) return false;

		std::ifstream fa(pathA, std::ios::binary);
		std::ifstream fb(pathB, std::ios::binary);
		if (!fa.is_open() || !fb.is_open()) return false;

		char bufA[8192];
		char bufB[8192];
		while (fa && fb)
		{
			fa.read(bufA, sizeof(bufA));
			fb.read(bufB, sizeof(bufB));
			std::streamsize countA = fa.gcount();
			std::streamsize countB = fb.gcount();
			if (countA != countB) return false;
			if (countA > 0 && std::memcmp(bufA, bufB, static_cast<size_t>(countA)) != 0)
			{
				return false;
			}
		}
		return true;
	}

	// Búsqueda iterativa (con std::stack) en un directorio por nombre de archivo sin distinguir mayúsculas/minúsculas
	std::string FindFileByNameCaseInsensitive(const std::string& rootDir, const std::string& targetFilename)
	{
		if (rootDir.empty() || targetFilename.empty()) return "";
		std::error_code ec;
		if (!std::filesystem::exists(rootDir, ec) || !std::filesystem::is_directory(rootDir, ec)) return "";

		const std::string targetLower = ToLowerCopy(targetFilename);
		std::stack<std::filesystem::path> dirStack;
		dirStack.push(std::filesystem::path(rootDir));

		while (!dirStack.empty())
		{
			std::filesystem::path currentDir = dirStack.top();
			dirStack.pop();

			for (std::filesystem::directory_iterator it(currentDir, ec), end; !ec && it != end; it.increment(ec))
			{
				if (ec) break;
				std::error_code entryEc;
				if (it->is_directory(entryEc))
				{
					dirStack.push(it->path());
				}
				else if (it->is_regular_file(entryEc))
				{
					std::string fname = it->path().filename().string();
					if (ToLowerCopy(fname) == targetLower)
					{
						return NormalizeSlashes(it->path().string());
					}
				}
			}
		}
		return "";
	}
}

std::string SceneFile::ResolveProjectRoot()
{
	std::error_code ec;
	std::filesystem::path current = std::filesystem::current_path(ec);
	if (ec || current.empty())
	{
		return ".";
	}

	std::filesystem::path probe = current;
	for (int depth = 0; depth < 6; ++depth)
	{
		if (std::filesystem::exists(probe / "Assets", ec) && std::filesystem::is_directory(probe / "Assets", ec))
		{
			return NormalizeSlashes(probe.string());
		}
		if (std::filesystem::exists(probe / "CMakeLists.txt", ec))
		{
			return NormalizeSlashes(probe.string());
		}
		if (!probe.has_parent_path() || probe.parent_path() == probe)
		{
			break;
		}
		probe = probe.parent_path();
	}

	return NormalizeSlashes(current.string());
}

std::string SceneFile::ResolveAssetsDirectory()
{
	std::string assetsDir = ResolveProjectRoot() + "/Assets";
	std::error_code ec;
	if (!std::filesystem::exists(assetsDir, ec))
	{
		std::filesystem::create_directories(assetsDir, ec);
	}
	return assetsDir;
}

std::string SceneFile::ResolveModelsDirectory()
{
	std::string modelsDir = ResolveAssetsDirectory() + "/Models";
	std::error_code ec;
	if (!std::filesystem::exists(modelsDir, ec))
	{
		std::filesystem::create_directories(modelsDir, ec);
	}
	return modelsDir;
}

std::string SceneFile::ResolveTexturesDirectory()
{
	std::string texturesDir = ResolveAssetsDirectory() + "/Textures";
	std::error_code ec;
	if (!std::filesystem::exists(texturesDir, ec))
	{
		std::filesystem::create_directories(texturesDir, ec);
	}
	return texturesDir;
}

std::string SceneFile::ResolveScenesDirectory()
{
	std::string scenesDir = ResolveAssetsDirectory() + "/Scenes";
	std::error_code ec;
	if (!std::filesystem::exists(scenesDir, ec))
	{
		std::filesystem::create_directories(scenesDir, ec);
	}
	return scenesDir;
}

std::string SceneFile::ResolveLibraryMeshesDirectory()
{
	std::string libDir = ResolveProjectRoot() + "/Library/Meshes";
	std::error_code ec;
	if (!std::filesystem::exists(libDir, ec))
	{
		std::filesystem::create_directories(libDir, ec);
	}
	return libDir;
}

std::string SceneFile::ToProjectRelativePath(const std::string& anyPath)
{
	if (anyPath.empty()) return "";
	if (anyPath.rfind("Primitive:", 0) == 0 || anyPath.find("(procedural)") != std::string::npos)
	{
		return anyPath;
	}

	std::string norm = NormalizeSlashes(anyPath);
	std::string stripped = norm;
	while (stripped.rfind("../", 0) == 0)
	{
		stripped = stripped.substr(3);
	}
	while (stripped.rfind("./", 0) == 0)
	{
		stripped = stripped.substr(2);
	}

	std::string projectRoot = ResolveProjectRoot();
	std::error_code ec;
	std::filesystem::path rootCanon = std::filesystem::weakly_canonical(std::filesystem::path(projectRoot), ec);
	std::string rootStr = NormalizeSlashes((!ec && !rootCanon.empty()) ? rootCanon.string() : projectRoot);
	std::string rootLower = ToLowerCopy(rootStr);

	// Si ya es una ruta relativa al root tipo "Assets/..." o "Library/..."
	if (stripped.rfind("Assets/", 0) == 0 || stripped.rfind("Library/", 0) == 0)
	{
		return stripped;
	}

	std::filesystem::path inputPath(norm);
	std::filesystem::path inputCanon = std::filesystem::weakly_canonical(inputPath, ec);
	if (!ec && !inputCanon.empty())
	{
		std::string canonStr = NormalizeSlashes(inputCanon.string());
		std::string canonLower = ToLowerCopy(canonStr);
		if (canonLower.rfind(rootLower + "/", 0) == 0)
		{
			return canonStr.substr(rootStr.size() + 1);
		}
	}

	return norm;
}

std::string SceneFile::EnsureAssetInProject(const std::string& sourcePath, const std::string& targetSubfolder)
{
	if (sourcePath.empty()) return "";
	if (sourcePath.rfind("Primitive:", 0) == 0 || sourcePath.find("(procedural)") != std::string::npos)
	{
		return sourcePath;
	}

	std::error_code ec;
	std::string resolvedSource = ResolveExistingPath(sourcePath);
	std::filesystem::path srcPathObj(resolvedSource);

	if (!std::filesystem::exists(srcPathObj, ec) || !std::filesystem::is_regular_file(srcPathObj, ec))
	{
		// El archivo no existe en disco; devolver ruta normalizada por si se resuelve más tarde
		return ToProjectRelativePath(sourcePath);
	}

	std::string assetsDir = ResolveAssetsDirectory();
	std::filesystem::path assetsCanon = std::filesystem::weakly_canonical(std::filesystem::path(assetsDir), ec);
	std::string assetsStr = NormalizeSlashes((!ec && !assetsCanon.empty()) ? assetsCanon.string() : assetsDir);
	std::string assetsLower = ToLowerCopy(assetsStr);

	std::filesystem::path srcCanon = std::filesystem::weakly_canonical(srcPathObj, ec);
	std::string srcStr = NormalizeSlashes((!ec && !srcCanon.empty()) ? srcCanon.string() : resolvedSource);
	std::string srcLower = ToLowerCopy(srcStr);

	// Si ya está dentro de Assets/, no duplicamos el archivo: devolvemos su ruta relativa al proyecto
	if (srcLower.rfind(assetsLower + "/", 0) == 0)
	{
		return ToProjectRelativePath(srcStr);
	}

	// El archivo proviene de fuera del proyecto (ej. Descargas / Escritorio): copiarlo a Assets/<targetSubfolder>/
	std::string destDirStr = assetsDir;
	if (!targetSubfolder.empty())
	{
		destDirStr += "/" + targetSubfolder;
	}
	std::filesystem::create_directories(destDirStr, ec);

	std::string origFilename = ExtractFilenamePortable(resolvedSource);
	if (origFilename.empty())
	{
		return ToProjectRelativePath(resolvedSource);
	}

	std::filesystem::path origFilePath(origFilename);
	std::string stem = origFilePath.stem().string();
	std::string ext = origFilePath.extension().string();

	std::filesystem::path destPath = std::filesystem::path(destDirStr) / origFilename;
	if (std::filesystem::exists(destPath, ec))
	{
		if (AreFilesIdentical(srcPathObj, destPath))
		{
			std::string relExisting = ToProjectRelativePath(destPath.string());
			LOG("[Assets] Archivo externo ya existía en el proyecto (reutilizado): '%s'", relExisting.c_str());
			return relExisting;
		}

		// Evitar sobrescribir un asset distinto con el mismo nombre
		int suffix = 1;
		while (true)
		{
			std::string candidateName = stem + "_" + std::to_string(suffix) + ext;
			std::filesystem::path candidatePath = std::filesystem::path(destDirStr) / candidateName;
			if (!std::filesystem::exists(candidatePath, ec))
			{
				destPath = candidatePath;
				break;
			}
			if (AreFilesIdentical(srcPathObj, candidatePath))
			{
				std::string relExisting = ToProjectRelativePath(candidatePath.string());
				LOG("[Assets] Archivo externo ya existía en el proyecto (reutilizado): '%s'", relExisting.c_str());
				return relExisting;
			}
			suffix++;
		}
	}

	std::filesystem::copy_file(srcPathObj, destPath, std::filesystem::copy_options::overwrite_existing, ec);
	if (ec)
	{
		LOG_ERROR("[Assets] Error al copiar archivo externo '%s' hacia '%s': %s",
			resolvedSource.c_str(), destPath.string().c_str(), ec.message().c_str());
		return ToProjectRelativePath(resolvedSource);
	}

	// Si es un modelo .obj y existe un archivo .mtl hermano, copiarlo también
	if (ToLowerCopy(ext) == ".obj" && srcPathObj.has_parent_path())
	{
		std::filesystem::path mtlSrc = srcPathObj.parent_path() / (stem + ".mtl");
		if (std::filesystem::exists(mtlSrc, ec) && std::filesystem::is_regular_file(mtlSrc, ec))
		{
			std::filesystem::path mtlDest = destPath.parent_path() / (destPath.stem().string() + ".mtl");
			std::filesystem::copy_file(mtlSrc, mtlDest, std::filesystem::copy_options::overwrite_existing, ec);
		}
	}

	std::string relDest = ToProjectRelativePath(destPath.string());
	LOG("[Assets] Archivo externo copiado al proyecto: '%s' -> '%s'", resolvedSource.c_str(), relDest.c_str());
	return relDest;
}

std::string SceneFile::ResolveExistingPath(const std::string& storedPath)
{
	if (storedPath.empty()) return storedPath;
	if (storedPath.rfind("Primitive:", 0) == 0 || storedPath.find("(procedural)") != std::string::npos)
	{
		return storedPath;
	}

	std::error_code ec;
	std::string norm = NormalizeSlashes(storedPath);
	std::string projectRoot = ResolveProjectRoot();

	// 1. Probar relativo al root del proyecto (ej. "Assets/BakerHouse.fbx", "Library/Meshes/...")
	std::string rootCandidate = projectRoot + "/" + norm;
	if (std::filesystem::exists(rootCandidate, ec)) return rootCandidate;

	// 2. Compatibilidad con rutas relativas antiguas con "../" o "../../"
	std::string stripped = norm;
	while (stripped.rfind("../", 0) == 0) stripped = stripped.substr(3);
	while (stripped.rfind("./", 0) == 0) stripped = stripped.substr(2);
	if (stripped != norm)
	{
		std::string strippedCandidate = projectRoot + "/" + stripped;
		if (std::filesystem::exists(strippedCandidate, ec)) return strippedCandidate;
	}

	// 3. Probar la ruta tal cual (ruta absoluta local o relativa al directorio actual)
	if (std::filesystem::exists(norm, ec)) return norm;
	if (std::filesystem::exists(storedPath, ec)) return storedPath;

	// 4. Fallback inteligente por nombre de archivo dentro de las carpetas del proyecto (case-insensitive)
	std::string filename = ExtractFilenamePortable(norm);
	if (filename.empty()) return storedPath;

	std::filesystem::path fnPath(filename);
	std::string extLower = ToLowerCopy(fnPath.extension().string());

	if (extLower == ".mesh")
	{
		std::string found = FindFileByNameCaseInsensitive(ResolveLibraryMeshesDirectory(), filename);
		if (!found.empty()) return found;
	}
	else if (extLower == ".scene" || extLower == ".json")
	{
		std::string found = FindFileByNameCaseInsensitive(ResolveScenesDirectory(), filename);
		if (!found.empty()) return found;
	}
	else if (extLower == ".fbx" || extLower == ".obj" || extLower == ".dae")
	{
		std::string foundModel = FindFileByNameCaseInsensitive(ResolveModelsDirectory(), filename);
		if (!foundModel.empty()) return foundModel;
	}
	else if (extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" ||
		     extLower == ".dds" || extLower == ".tga" || extLower == ".bmp")
	{
		std::string foundTex = FindFileByNameCaseInsensitive(ResolveTexturesDirectory(), filename);
		if (!foundTex.empty()) return foundTex;
	}

	// Búsqueda general en todo Assets/ (incluye subcarpetas y la raíz Assets/)
	std::string foundInAssets = FindFileByNameCaseInsensitive(ResolveAssetsDirectory(), filename);
	if (!foundInAssets.empty()) return foundInAssets;

	return storedPath;
}

std::vector<std::string> SceneFile::ListSavedSceneFiles()
{
	std::vector<std::string> results;
	std::string scenesDir = ResolveScenesDirectory();
	std::error_code ec;
	if (!std::filesystem::exists(scenesDir, ec)) return results;

	for (std::filesystem::directory_iterator it(scenesDir, ec), end; it != end; it.increment(ec))
	{
		if (ec) break;
		if (!it->is_regular_file(ec)) continue;
		std::filesystem::path path = it->path();
		std::string ext = ToLowerCopy(path.extension().string());
		if (ext == ".scene" || ext == ".json")
		{
			results.push_back(ToProjectRelativePath(scenesDir + "/" + path.filename().string()));
		}
	}
	std::sort(results.begin(), results.end());
	return results;
}
