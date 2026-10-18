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

#include <unordered_set>
#include <algorithm>
#include <filesystem>
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
	camTrans->SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
	camObj->AddComponent(std::move(camTrans));

	auto camComp = std::make_unique<CameraComponent>(camObj.get());
	camObj->AddComponent(std::move(camComp));

	mainCameraPtr = camObj.get();
	sceneRoot.AddChild(camObj.get());
	sceneEntities.push_back(std::move(camObj));
	LOG("[Scene] Instanciada 'EditorCamera' en posición (0.0, 1.0, 5.0)");

	// Create Cube
	auto cubeObj = std::make_unique<GameObject>("Cube");
	auto cubeTrans = std::make_unique<TransformComponent>(cubeObj.get());
	cubeTrans->SetPosition(glm::vec3(-2.0f, 0.0f, 0.0f));
	cubeObj->AddComponent(std::move(cubeTrans));

	auto cubeMesh = std::make_unique<MeshComponent>(cubeObj.get(), GeometryGenerator::CreateCube(1.0f));
	cubeObj->AddComponent(std::move(cubeMesh));

	auto cubeMat = std::make_unique<MaterialComponent>(cubeObj.get());
	cubeMat->SetColor(glm::vec3(1.0f, 0.2f, 0.2f));
	cubeMat->SetUseTexture(false);
	cubeObj->AddComponent(std::move(cubeMat));

	sceneRoot.AddChild(cubeObj.get());
	sceneEntities.push_back(std::move(cubeObj));
	LOG("[Scene] Instanciado 'Cube' con geometría procedural de cubo");

	// Create Sphere
	auto sphereObj = std::make_unique<GameObject>("Sphere");
	auto sphereTrans = std::make_unique<TransformComponent>(sphereObj.get());
	sphereTrans->SetPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	sphereObj->AddComponent(std::move(sphereTrans));

	auto sphereMesh = std::make_unique<MeshComponent>(sphereObj.get(), GeometryGenerator::CreateSphere(0.5f, 20, 20));
	sphereObj->AddComponent(std::move(sphereMesh));

	auto sphereMat = std::make_unique<MaterialComponent>(sphereObj.get());
	sphereMat->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
	sphereMat->SetTexture("../test.jpg");
	sphereMat->SetUseTexture(true);
	sphereObj->AddComponent(std::move(sphereMat));

	sceneRoot.AddChild(sphereObj.get());
	sceneEntities.push_back(std::move(sphereObj));
	LOG("[Scene] Instanciado 'Sphere' con geometría procedural de esfera y textura");

	LOG("[Scene] Escena inicial cargada con éxito (%zu entidades registradas)", sceneEntities.size());

	return true;
}

bool Scene::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	return true;
}

bool Scene::Update(float dt)
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	// Centralización del drag & drop
	if (Engine::GetInstance().input && Engine::GetInstance().input->HasDroppedFile())
	{
		std::string droppedPath = Engine::GetInstance().input->GetDroppedFilePath();
		HandleDroppedFile(droppedPath);
		Engine::GetInstance().input->ClearDroppedFile();
	}

	sceneRoot.Update(dt);
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
	selected = nullptr;
	mainCameraPtr = nullptr;
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

GameObject* Scene::Register(std::unique_ptr<GameObject> newObj)
{
	GameObject* ptr = newObj.get();
	sceneRoot.AddChild(ptr);
	sceneEntities.push_back(std::move(newObj));
	return ptr;
}

GameObject* Scene::CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name)
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
		cubeMat->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
		cubeMat->SetUseTexture(false);
		newObj->AddComponent(std::move(cubeMat));
	}

	GameObject* registered = Register(std::move(newObj));
	LOG("[Scene] GameObject '%s' creado (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::CreateEmptyGameObject()
{
	std::string uniqueName = MakeUniqueName("GameObject");
	auto newObj = std::make_unique<GameObject>(uniqueName);

	auto newTrans = std::make_unique<TransformComponent>(newObj.get());
	newTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	newObj->AddComponent(std::move(newTrans));

	GameObject* registered = Register(std::move(newObj));
	LOG("[Scene] Empty GameObject '%s' creado (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::CreatePrimitive(PrimitiveType type)
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
	mat->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
	mat->SetUseTexture(false);
	newObj->AddComponent(std::move(mat));

	GameObject* registered = Register(std::move(newObj));
	LOG("[Scene] Primitiva '%s' creada (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::ImportModel(const std::string& filePath)
{
	LOG("[Scene] Importando modelo 3D desde: %s", filePath.c_str());
	auto modelPrimitive = ModelLoader::LoadFBX(filePath);
	if (!modelPrimitive)
	{
		LOG_ERROR("[Scene] Falló la importación del modelo: %s", filePath.c_str());
		return nullptr;
	}

	std::filesystem::path p(filePath);
	std::string stemName = p.stem().string();
	if (stemName.empty()) stemName = "ImportedModel";
	std::string uniqueName = MakeUniqueName(stemName);

	auto newObj = std::make_unique<GameObject>(uniqueName);

	auto trans = std::make_unique<TransformComponent>(newObj.get());
	trans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	newObj->AddComponent(std::move(trans));

	auto mesh = std::make_unique<MeshComponent>(newObj.get(), std::move(modelPrimitive));
	newObj->AddComponent(std::move(mesh));

	auto mat = std::make_unique<MaterialComponent>(newObj.get());
	mat->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
	mat->SetUseTexture(false);
	newObj->AddComponent(std::move(mat));

	GameObject* registered = Register(std::move(newObj));
	SetSelected(registered);
	LOG("[Scene] Modelo '%s' importado y seleccionado en la escena", uniqueName.c_str());
	return registered;
}

void Scene::DuplicateGameObject(GameObject object)
{
	// TODO: Clonado profundo de componentes
}

void Scene::HandleDroppedFile(const std::string& filePath)
{
	std::filesystem::path p(filePath);
	std::string ext = p.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".fbx" || ext == ".obj" || ext == ".dae")
	{
		ImportModel(filePath);
	}
	else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
		ext == ".dds" || ext == ".tga" || ext == ".bmp")
	{
		if (selected != nullptr)
		{
			MaterialComponent* mat = selected->GetComponent<MaterialComponent>();
			if (mat != nullptr)
			{
				mat->SetTexture(filePath);
				mat->SetUseTexture(true);
				LOG("[Scene] Textura '%s' asignada al GameObject seleccionado '%s'", filePath.c_str(), selected->GetName().c_str());
			}
			else
			{
				LOG_WARNING("[Scene] El GameObject seleccionado '%s' no tiene MaterialComponent", selected->GetName().c_str());
			}
		}
		else
		{
			LOG_WARNING("[Scene] Se soltó la textura '%s' pero no hay ningún GameObject seleccionado", filePath.c_str());
		}
	}
	else
	{
		LOG_WARNING("[Scene] Formato de archivo no reconocido para Drag & Drop: %s", filePath.c_str());
	}
}
