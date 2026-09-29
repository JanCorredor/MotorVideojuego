#pragma once

#include "./Components/Component.h"

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
	vector<std::unique_ptr<Component>> objComponents;

};

GameObject::GameObject()
{
}

GameObject::~GameObject()
{
}
