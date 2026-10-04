#include "../Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../../Utils/Log.h"
#include "Scene.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/MeshComponent.h"
#include "GameObjects/Components/MaterialComponent.h"
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

	// Create Cube
	auto cubeObj = std::make_unique<GameObject>("Cube");
	auto cubeTrans = std::make_unique<TransformComponent>(cubeObj.get());
	cubeTrans->SetPosition(glm::vec3(-2.0f, 0.0f, 0.0f));
	cubeObj->AddComponent(std::move(cubeTrans));

	auto cubeMesh = std::make_unique<MeshComponent>(cubeObj.get(), GeometryGenerator::CreateCube(1.0f));
	cubeObj->AddComponent(std::move(cubeMesh));

	auto cubeMat = std::make_unique<MaterialComponent>(cubeObj.get());
	cubeMat->color = glm::vec3(1.0f, 0.2f, 0.2f);
	cubeMat->useTexture = false;
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
	sphereMat->color = glm::vec3(1.0f, 1.0f, 1.0f);
	sphereMat->textureID = LoadTexture("../test.jpg");
	sphereMat->useTexture = true;
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
	// Simulate the spinning animation for the cube and sphere
	for (auto& entity : sceneEntities) {
		TransformComponent* transform = entity->GetComponent<TransformComponent>();
		if (transform) {
			glm::vec3 rot = transform->GetRotation();
			rot.y += 0.1f * dt;
			if (entity->GetName() == "Cube") rot.x += 0.05f * dt;
			transform->SetRotation(rot);
		}
	}
	sceneRoot.Update();
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
