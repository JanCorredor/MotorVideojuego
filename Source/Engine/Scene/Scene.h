#pragma once

#include "../Module.h"

#include "GameObjects/GameObject.h"
#include "../Render/Primitives/GeometryGenerator.h"
#include <vector>
#include <memory>
#include <string>

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

	// Creación de GameObjects (todos devuelven el objeto creado, propiedad de la escena)
	GameObject* CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name = "GameObject");
	GameObject* CreateEmptyGameObject();
	GameObject* CreatePrimitive(PrimitiveType type);
	GameObject* ImportModel(const std::string& filePath);
	void DuplicateGameObject(GameObject object);

	// Archivo soltado en la ventana: modelo => importa, textura => se asigna al seleccionado
	void HandleDroppedFile(const std::string& filePath);

	GameObject* GetRoot() { return &sceneRoot; }
	GameObject* GetMainCameraObject() const { return mainCameraPtr; }

	// Selección única de la escena (la consumen Editor y cámara). nullptr = sin selección.
	// El root y objetos que no pertenezcan a la escena se rechazan.
	void SetSelected(GameObject* object);
	GameObject* GetSelected() const { return selected; }

private:
	std::string MakeUniqueName(const std::string& baseName) const;
	bool OwnsObject(const GameObject* object) const;
	GameObject* Register(std::unique_ptr<GameObject> newObj);

	GameObject sceneRoot;
	std::vector<std::unique_ptr<GameObject>> sceneEntities;
	GameObject* mainCameraPtr = nullptr;
	GameObject* selected = nullptr;
};
