#include "GameObject.h"
#include "./Components/Component.h"
#include "./Components/TransformComponent.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    // Iterar directamente sobre el array fijo (cero indirectación dinámicas)
    for (auto& comp : components)
    {
        if (comp && comp->IsActive())
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
    if (!component) return;

    // Asignar directamente en el índice del enum correspondiente
    ComponentType type = component->GetType();
    components[type] = std::move(component);
}

bool GameObject::SetParent(GameObject* newParent)
{
    if (newParent == parent) return true;

    if (newParent == this || (newParent && newParent->IsDescendantOf(this)))
    {
        return false;
    }

    TransformComponent* trans = GetComponent<TransformComponent>();

    // Guardamos la matriz de mundo actual antes de cambiar de padre
    glm::mat4 oldWorldMatrix = GetWorldMatrix();

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

    // Recalculamos el Transform local para que mantenga su posición, rotación y tamaño en el mundo
    if (trans)
    {
        glm::mat4 newParentWorld = parent ? parent->GetWorldMatrix() : glm::mat4(1.0f);
        glm::mat4 newLocalMatrix = glm::inverse(newParentWorld) * oldWorldMatrix;

        glm::vec3 newPos = glm::vec3(newLocalMatrix[3]);

        glm::vec3 col0 = glm::vec3(newLocalMatrix[0]);
        glm::vec3 col1 = glm::vec3(newLocalMatrix[1]);
        glm::vec3 col2 = glm::vec3(newLocalMatrix[2]);

        glm::vec3 newScale;
        newScale.x = glm::length(col0);
        newScale.y = glm::length(col1);
        newScale.z = glm::length(col2);

        glm::mat3 rotMat(1.0f);
        if (newScale.x > 1e-6f) rotMat[0] = col0 / newScale.x;
        if (newScale.y > 1e-6f) rotMat[1] = col1 / newScale.y;
        if (newScale.z > 1e-6f) rotMat[2] = col2 / newScale.z;

        float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
        glm::extractEulerAngleXYZ(glm::mat4(rotMat), rotX, rotY, rotZ);
        glm::vec3 newRot = glm::degrees(glm::vec3(rotX, rotY, rotZ));

        trans->SetPosition(newPos);
        trans->SetRotation(newRot);
        trans->SetScale(newScale);
    }

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
        t->InvalidateWorld();
        return;
    }

    for (GameObject* child : children)
    {
        child->InvalidateWorldTransform();
    }
}