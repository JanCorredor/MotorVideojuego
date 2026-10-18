#include "GameObject.h"

#include "./Components/Component.h"
#include "./Components/TransformComponent.h"

#include <algorithm>

#include <tracy/Tracy.hpp>

GameObject::GameObject(const std::string& name, GameUUID id) : uuid(id), name(name), parent(nullptr)
{
}

GameObject::~GameObject() 
{
}

void GameObject::Update(float dt)
{
#ifdef _DEBUG
    ZoneScoped;
#endif

    if (!active) return;

    for (auto& comp : objComponents) 
    {
        if (comp->IsActive())
        {
            comp->Update(dt);
        }
    }
    for (auto child : children) 
    {
        if (child->IsActive())
        {
            child->Update(dt);
        }
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

glm::mat4 GameObject::GetWorldMatrix() const
{
    // Se recorre la cadena de padres de forma iterativa (de la hoja a la raíz)
    glm::mat4 world(1.0f);
    for (const GameObject* node = this; node != nullptr; node = node->parent)
    {
        const TransformComponent* t = node->GetComponent<TransformComponent>();
        if (t) world = t->GetLocalModelMatrix() * world;
    }
    return world;
}
