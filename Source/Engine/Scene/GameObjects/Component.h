#pragma once

#include "GameObject.h"

enum ComponentType
{
	COMPONENT_TRANSFORM,
	COMPONENT_MESH,
	COMPONENT_MATERIAL,
	COMPONENT_CAMERA
};

class Component
{
public:
	Component();
	~Component();

	virtual void Update();

	virtual void Enable();
	virtual void Disable();

private:
	ComponentType type;
	bool active = true;
	GameObject* owner;
};

Component::Component()
{
}

Component::~Component()
{
}