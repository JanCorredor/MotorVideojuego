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
	virtual ~Scene();

	bool Awake() override;
	bool Start() override;
	bool PreUpdate() override;
	bool Update(float dt) override;
	bool PostUpdate() override;
	bool CleanUp() override;

	// Creación de GameObjects con soporte para padre opcional (nullptr = cuelga de sceneRoot)
	GameObject* CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name = "GameObject", GameObject* parent = nullptr);
	GameObject* CreateEmptyGameObject(GameObject* parent = nullptr);
	GameObject* CreatePrimitive(PrimitiveType type, GameObject* parent = nullptr);
	GameObject* ImportModel(const std::string& filePath, GameObject* parent = nullptr);
	void DuplicateGameObject(GameObject object);

	// Destrucción recursiva en cascada y segura de GameObjects
	void DestroyGameObject(GameObject* target);

	// Archivo soltado en la ventana: modelo => importa, textura => se asigna al seleccionado
	void HandleDroppedFile(const std::string& filePath);

	GameObject* GetRoot() { return &sceneRoot; }
	const GameObject* GetRoot() const { return &sceneRoot; }
	GameObject* GetMainCameraObject() const { return mainCameraPtr; }

	// Selección única de la escena (la consumen Editor y cámara). nullptr = sin selección.
	// El root y objetos que no pertenezcan a la escena se rechazan.
	void SetSelected(GameObject* object);
	GameObject* GetSelected() const { return selected; }

	// Comprueba si un puntero a GameObject es propiedad viva de esta escena
	bool OwnsObject(const GameObject* object) const;

private:
	std::string MakeUniqueName(const std::string& baseName) const;
	GameObject* Register(std::unique_ptr<GameObject> newObj, GameObject* parent = nullptr);
	void ProcessPendingDestructions();

	GameObject sceneRoot;
	std::vector<std::unique_ptr<GameObject>> sceneEntities;
	std::vector<GameObject*> pendingDestruction;

	GameObject* mainCameraPtr = nullptr;
	GameObject* selected = nullptr;
};
