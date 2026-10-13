#include "../Engine.h"
#include "../Render/Render.h"
#include "../../Utils/Log.h"
#include "Scene.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/MeshComponent.h"
#include "GameObjects/Components/MaterialComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include <unordered_set>

#include <tracy/Tracy.hpp>

Scene::Scene() : Module()
{

}

Scene::~Scene()
{

}

bool Scene::Awake()
{
	LOG("[Scene] Inicializando módulo Scene...");
	bool ret = true;

	return ret;
}

bool Scene::Start()
{
	LOG("[Scene] Configurando escena por defecto...");

	// Instance Editor Camera
	auto camObj = std::make_unique<GameObject>("EditorCamera");
	auto camTrans = std::make_unique<TransformComponent>(camObj.get());
	camTrans->SetPosition(glm::vec3(0.0f, 1.0f, 5.0f)); // Posición inicial
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
	return true;
}

void Scene::CreateNewGameObject(vector<std::unique_ptr<Component>> objComponents)
{
	auto newObj = std::make_unique<GameObject>("NewGameObject");

	std::unordered_set<ComponentType> presentTypes;

	// Trasfer all components given
	for (auto& comp : objComponents)
	{
		if (comp)
		{
			presentTypes.insert(comp->GetType());
			newObj->AddComponent(std::move(comp));
		}
	}

	// TransformComponent: 0,0,0 Default
	if (presentTypes.find(COMPONENT_TRANSFORM) == presentTypes.end())
	{
		auto newTrans = std::make_unique<TransformComponent>(newObj.get());
		newTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
		newObj->AddComponent(std::move(newTrans));
	}

	// MeshComponent: Cube is default
	if (presentTypes.find(COMPONENT_MESH) == presentTypes.end())
	{
		auto newMesh = std::make_unique<MeshComponent>(newObj.get(), GeometryGenerator::CreateCube(1.0f));
		newObj->AddComponent(std::move(newMesh));
	}

	// MaterialComponent: Default Material: Color is White
	if (presentTypes.find(COMPONENT_MATERIAL) == presentTypes.end())
	{
		auto cubeMat = std::make_unique<MaterialComponent>(newObj.get());
		cubeMat->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
		cubeMat->SetUseTexture(false);
		newObj->AddComponent(std::move(cubeMat));
	}

	std::string objName = newObj->GetName();
	sceneRoot.AddChild(newObj.get());
	sceneEntities.push_back(std::move(newObj));

	LOG("[Scene] GameObject '%s' registrado en la escena (Total entidades: %zu)", objName.c_str(), sceneEntities.size());
}
