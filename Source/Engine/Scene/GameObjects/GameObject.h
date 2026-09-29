#pragma once

#include "Component.h"

#include <vector>
#include <string>

using namespace std;

class GameObject
{
public:
	GameObject();
	~GameObject();

	void Update();
	Component& CreateComponent(ComponentType type);


private:
	bool active = true;
	string name;
	vector<Component> objComponents;

};

GameObject::GameObject()
{
}

GameObject::~GameObject()
{
}
