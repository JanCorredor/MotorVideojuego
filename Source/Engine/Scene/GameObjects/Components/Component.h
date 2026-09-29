#pragma once

class GameObject;

enum ComponentType
{
	COMPONENT_TRANSFORM,
	COMPONENT_MESH,
	COMPONENT_MATERIAL
};

class Component
{
public:
	Component(ComponentType type, GameObject* owner) : type(type), owner(owner) {}
	virtual ~Component() {}

	virtual void Update() {}

	virtual void Enable() { active = true; }
	virtual void Disable() { active = false; }
    
    ComponentType GetType() const { return type; }
    GameObject* GetOwner() const { return owner; }
    bool IsActive() const { return active; }

protected:
	ComponentType type;
	bool active = true;
	GameObject* owner = nullptr;
};