#include "../Engine.h"
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

#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <tracy/Tracy.hpp>

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

	// Instance Editor Camera
	auto camObj = std::make_unique<GameObject>("EditorCamera");
	auto camTrans = std::make_unique<TransformComponent>(camObj.get());
	camTrans->SetPosition(glm::vec4(0.0f, 1.0f, 5.0f,1.0f));
	camObj->AddComponent(std::move(camTrans));

	auto camComp = std::make_unique<CameraComponent>(camObj.get());
	camObj->AddComponent(std::move(camComp));

	mainCameraPtr = camObj.get();
	sceneRoot.AddChild(camObj.get());
	sceneEntities.push_back(std::move(camObj));
	LOG("[Scene] Instanciada 'EditorCamera' en posición (0.0, 1.0, 5.0)");

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
	cubeMat->SetTexture("../Assets/water.jpg");
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
	sphereMat->SetTexture("../Assets/lava.jpg");
	sphereMat->SetUseTexture(true);
	sphereObj->AddComponent(std::move(sphereMat));

	cubeObj->AddChild(sphereObj.get());
	sceneEntities.push_back(std::move(sphereObj));


	sceneRoot.AddChild(cubeObj.get());
	sceneEntities.push_back(std::move(cubeObj));
	LOG("[Scene] Instanciado 'Cube' con geometría procedural de cubo");


	LOG("[Scene] Instanciado 'Sphere' con geometría procedural de esfera y textura");

	// MODELO CASA CON TEXTURA PNG
	GameObject* bakerHouse = ImportModel("../Assets/BakerHouse.fbx", &sceneRoot);
	MaterialComponent* bakerHouseMat = bakerHouse->GetComponent<MaterialComponent>();
	bakerHouseMat->SetTexture("../Assets/Baker_house.png");
	bakerHouseMat->SetUseTexture(true);
	TransformComponent* bakerHouseTransform = bakerHouse->GetComponent<TransformComponent>();
	bakerHouseTransform->SetPosition(glm::vec3(0.0f, 0.0f, -3.0f));

	// MODELO CON TEXTURA TGA 
	GameObject* candle = ImportModel("../Assets/SM_Candle_Fundida.fbx", &sceneRoot);
	MaterialComponent* candleMat = candle->GetComponent<MaterialComponent>();
	candleMat->SetTexture("../Assets/T_Candle_D.tga");
	candleMat->SetUseTexture(true);
	TransformComponent* candleTransform = candle->GetComponent<TransformComponent>();
	candleTransform->SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
	candleTransform->SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
	candleTransform->SetScale(glm::vec3(0.05f, 0.05f, 0.05f));

	// MODELO SIN TEXTURA
	GameObject* character = ImportModel("../Assets/HeavyTF2_2018.fbx", &sceneRoot);
	MaterialComponent* characterMat = character->GetComponent<MaterialComponent>();
	TransformComponent* characterTransform = character->GetComponent<TransformComponent>();
	characterTransform->SetPosition(glm::vec3(-3.0f, 0.0f, -2.0f));
	characterTransform->SetScale(glm::vec3(0.02f, 0.02f, 0.02f));

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

	sceneRoot.Update(dt);
	ProcessPendingDestructions();
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
	pendingDestruction.clear();
	selected = nullptr;
	mainCameraPtr = nullptr;
	gameCameraPtr = nullptr;
	sceneEntities.clear();
	return true;
}

std::string Scene::MakeUniqueName(const std::string& baseName) const
{
	bool exists = false;
	for (const auto& entity : sceneEntities)
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
		for (const auto& entity : sceneEntities)
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

	for (const auto& entity : sceneEntities)
	{
		if (entity.get() == object) return true;
	}
	return false;
}

void Scene::SetSelected(GameObject* object)
{
	if (object == nullptr)
	{
		selected = nullptr;
		return;
	}
	if (OwnsObject(object))
	{
		selected = object;
	}
	else
	{
		selected = nullptr;
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
	return ptr;
}

GameObject* Scene::CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name, GameObject* parent)
{
	std::string uniqueName = MakeUniqueName(name);
	auto newObj = std::make_unique<GameObject>(uniqueName);

	std::unordered_set<ComponentType> presentTypes;
	for (auto& comp : objComponents)
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
	std::function<void(GameObject*)> collectPostOrder = [&](GameObject* node)
	{
		for (GameObject* child : node->GetChildren())
		{
			collectPostOrder(child);
		}
		toDestroy.push_back(node);
	};
	collectPostOrder(target);

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
}

void Scene::DuplicateGameObject(GameObject object)
{
	// TODO: Clonado profundo de componentes
}

void Scene::HandleDroppedFile(const std::string& filePath)
{
	ImporterManager::ImportFile(filePath, selected);
}
