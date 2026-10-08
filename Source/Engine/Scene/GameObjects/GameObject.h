#pragma once

#include "./Components/Component.h"

#include <vector>
#include <string>
#include <memory>

using namespace std;

class GameObject
{
public:
	GameObject(const string& name = "GameObject");
	~GameObject();

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

    void AddChild(GameObject* child);
    void RemoveChild(GameObject* child);

    GameObject* GetParent() const { return parent; }
    const std::vector<GameObject*>& GetChildren() const { return children; }
    
    void SetName(const string& n) { name = n; }
    string GetName() const { return name; }

    bool IsActive() const { return active; }
	void Enable() { this->active = true; }
	void Disable() { this->active = false; }

private:
	bool active = true;
	string name;
	vector<std::unique_ptr<Component>> objComponents;
    
    GameObject* parent = nullptr;
    vector<GameObject*> children;
};
