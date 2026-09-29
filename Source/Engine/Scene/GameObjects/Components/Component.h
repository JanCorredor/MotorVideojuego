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
	Component();
	virtual ~Component();

	virtual void Update() {};

	virtual void Enable() {};
	virtual void Disable() {};

private:
	ComponentType type;
	bool active = true;
	GameObject* owner = nullptr;
};

Component::Component()
{
}

Component::~Component()
{
}