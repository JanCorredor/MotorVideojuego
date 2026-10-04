#include "GameObject.h"

GameObject::GameObject(const string& name) : name(name), parent(nullptr) 
{
}

GameObject::~GameObject() 
{
}

void GameObject::Update()
{
    if (!active) return;
    for (auto& comp : objComponents) {
        if (comp->IsActive()) comp->Update();
    }
    for (auto child : children) {
        child->Update();
    }
}

void GameObject::AddComponent(std::unique_ptr<Component> component)
{
    objComponents.push_back(std::move(component));
}

void GameObject::AddChild(GameObject* child)
{
    if (child->parent)
    {
        child->parent->RemoveChild(child);
    }
    child->parent = this;
    children.push_back(child);
}

void GameObject::RemoveChild(GameObject* child)
{
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end())
    {
        (*it)->parent = nullptr;
        children.erase(it);
    }
}
