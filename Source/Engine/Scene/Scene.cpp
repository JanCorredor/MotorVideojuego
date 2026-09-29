#include "../Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../../Utils/Log.h"
#include "Scene.h"




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


	return true;
}

bool Scene::PreUpdate()
{

	return true;
}

bool Scene::Update(float dt)
{

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
