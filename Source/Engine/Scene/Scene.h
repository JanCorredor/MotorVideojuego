#pragma once

#include "../Module.h"

#include "GameObjects/GameObject.h"
#include <vector>
#include <memory>

class Scene : public Module
{
public:

	Scene();

	// Destructor
	virtual ~Scene();

	// Called before render is available
	bool Awake();

	// Called before the first frame
	bool Start();

	// Called each loop iteration
	bool PreUpdate();
	bool Update(float dt);
	bool PostUpdate();

	// Called before quitting
	bool CleanUp();

	void CreateNewGameObject(vector<std::unique_ptr<Component>> objComponents);
	void CreateEmptyGameObject();
	void DuplicateGameObject(GameObject object);

	GameObject* GetRoot() { return &sceneRoot; }
	GameObject* GetMainCameraObject() const { return mainCameraPtr; }

private:
	GameObject sceneRoot;
	std::vector<std::unique_ptr<GameObject>> sceneEntities;
	GameObject* mainCameraPtr;
};
