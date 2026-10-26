#pragma once

#include "../../../Utils/UUID.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

class Component;

class GameObject
{
public:
	GameObject(const std::string& name = "GameObject", GameUUID uuid = GameUUID());
	~GameObject();

	GameUUID GetUUID() const { return uuid; }

	void Update(float dt);

    void AddComponent(std::unique_ptr<Component> component);

	template <typename T>
	T* GetComponent()
	{
		for (auto& comp : objComponents)
		{
			T* casted = dynamic_cast<T*>(comp.get());
			if (casted) return casted;
		}
		return nullptr;
	}

	template <typename T>
	const T* GetComponent() const
	{
		for (const auto& comp : objComponents)
		{
			const T* casted = dynamic_cast<const T*>(comp.get());
			if (casted) return casted;
		}
		return nullptr;
	}

    bool SetParent(GameObject* newParent);

    // Atajos sobre SetParent (única ruta de modificación de la jerarquía)
    bool AddChild(GameObject* child);
    void RemoveChild(GameObject* child);

    // true si 'ancestor' aparece en la cadena de padres de este nodo
    bool IsDescendantOf(const GameObject* ancestor) const;

    GameObject* GetParent() const { return parent; }
    const std::vector<GameObject*>& GetChildren() const { return children; }
    
    void SetName(const std::string& n) { name = n; }
    const std::string& GetName() const { return name; }

    bool IsActive() const { return active; }
	void Enable() { this->active = true; }
	void Disable() { this->active = false; }

	// Matriz de transformación global (ParentWorld * Local), cacheada en el TransformComponent.
	glm::mat4 GetWorldMatrix() const;

	// Marca como sucia la matriz global de este nodo y de todos sus descendientes
	void InvalidateWorldTransform();

private:
	GameUUID uuid; //Unique Identifier

	bool active = true;
	std::string name;
	std::vector<std::unique_ptr<Component>> objComponents;
    
    GameObject* parent = nullptr;
    std::vector<GameObject*> children;
};
