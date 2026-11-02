#pragma once

#include <memory>

class GameObject;

enum ComponentType
{
	COMPONENT_TRANSFORM,
	COMPONENT_MESH,
	COMPONENT_MATERIAL,
	COMPONENT_CAMERA,
	COMPONENT_COUNT //Mantener al final
};

class Component
{
public:
	Component(ComponentType type, GameObject* owner) : type(type), owner(owner) {}
	virtual ~Component() {}

	virtual void Update(float dt) {}
    
    ComponentType GetType() const { return type; }
    GameObject* GetOwner() const { return owner; }
    
	bool IsActive() const { return active; }
	virtual void Enable() { this->active = true; }
	virtual void Disable() { this->active = false; }

	virtual std::unique_ptr<Component> Clone(GameObject* newOwner) const = 0;

protected:
	ComponentType type;
	bool active = true;
	GameObject* owner = nullptr;
};
