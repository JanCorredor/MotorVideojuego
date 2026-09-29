#pragma once

#include "./Components/Component.h"

#include <vector>
#include <string>
#include <algorithm>
#include <memory>

using namespace std;

class GameObject
{
public:
	GameObject(const string& name = "GameObject") : name(name), parent(nullptr) {}
	~GameObject() {}

	void Update()
    {
        if (!active) return;
        for (auto& comp : objComponents) {
            if (comp->IsActive()) comp->Update();
        }
        for (auto child : children) {
            child->Update();
        }
    }

    void AddComponent(std::unique_ptr<Component> component)
    {
        objComponents.push_back(std::move(component));
    }

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

    void AddChild(GameObject* child)
    {
        if (child->parent)
        {
            child->parent->RemoveChild(child);
        }
        child->parent = this;
        children.push_back(child);
    }

    void RemoveChild(GameObject* child)
    {
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end())
        {
            (*it)->parent = nullptr;
            children.erase(it);
        }
    }

    GameObject* GetParent() const { return parent; }
    const std::vector<GameObject*>& GetChildren() const { return children; }
    
    void SetName(const string& n) { name = n; }
    string GetName() const { return name; }

    bool IsActive() const { return active; }
    void SetActive(bool act) { active = act; }

private:
	bool active = true;
	string name;
	vector<std::unique_ptr<Component>> objComponents;
    
    GameObject* parent = nullptr;
    vector<GameObject*> children;
};
