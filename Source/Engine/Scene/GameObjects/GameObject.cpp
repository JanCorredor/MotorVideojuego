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

bool GameObject::SetParent(GameObject* newParent)
{
    if (newParent == parent) return true;

    // Evitar ciclos: no se puede ser hijo de uno mismo ni de un descendiente propio
    if (newParent == this || (newParent && newParent->IsDescendantOf(this)))
    {
        return false;
    }

    if (parent)
    {
        auto& siblings = parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    parent = newParent;
    if (parent)
    {
        parent->children.push_back(this);
    }

    // La matriz global depende del padre: invalidar este nodo y todo su subárbol
    InvalidateWorldTransform();
    return true;
}

bool GameObject::AddChild(GameObject* child)
{
    return child != nullptr && child->SetParent(this);
}

void GameObject::RemoveChild(GameObject* child)
{
    if (child != nullptr && child->parent == this)
    {
        child->SetParent(nullptr);
    }
}

bool GameObject::IsDescendantOf(const GameObject* ancestor) const
{
    if (ancestor == nullptr) return false;
    for (const GameObject* node = parent; node != nullptr; node = node->parent)
    {
        if (node == ancestor) return true;
    }
    return false;
}

glm::mat4 GameObject::GetWorldMatrix() const
{
    if (const TransformComponent* t = GetComponent<TransformComponent>())
    {
        return t->GetWorldMatrix();
    }
    return parent ? parent->GetWorldMatrix() : glm::mat4(1.0f);
}

void GameObject::InvalidateWorldTransform()
{
    if (TransformComponent* t = GetComponent<TransformComponent>())
    {
        // El Transform marca su worldDirty y propaga a los hijos
        t->InvalidateWorld();
        return;
    }

    // Nodo sin Transform (p.ej. la raíz): propagar directamente a los hijos
    for (GameObject* child : children)
    {
        child->InvalidateWorldTransform();
    }
}
