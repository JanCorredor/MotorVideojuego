#include "../Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../../Utils/Log.h"
#include "Scene.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/MeshComponent.h"
#include "GameObjects/Components/MaterialComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../Render/Primitives/GeometryGenerator.h"
#include "../Render/Texture/TextureLoader.h"




Scene::Scene() : Module()
{

}

Scene::~Scene()
{

}

bool Scene::Awake()
{
	LOG("Init Scene Module");
	bool ret = true;


	return ret;
}

bool Scene::Start()
{
	LOG("Scene Start");

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

	return true;
}

bool Scene::PreUpdate()
{

	return true;
}

bool Scene::Update(float dt)
{
	sceneRoot.Update(dt);
	return true;
}

bool Scene::PostUpdate()
{

	return true;
}

bool Scene::CleanUp()
{

	return true;
}
