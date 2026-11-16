#include "GameObject.h"
#include "./Components/Component.h"
#include "./Components/TransformComponent.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <stack>

#ifdef _DEBUG
#include <tracy/Tracy.hpp>
#endif

GameObject::GameObject(const std::string& name, GameUUID id) : uuid(id), name(name), parent(nullptr)
{
}

GameObject::GameObject(const GameObject& other)
    : uuid(GameUUID()), name(other.name), active(other.active), componentOrder(other.componentOrder), parent(nullptr)
{
    for (const std::unique_ptr<Component>& comp : other.components)
    {
        if (comp)
        {
            AddComponent(comp->Clone(this));
        }
    }
    componentOrder = other.componentOrder;
}

GameObject::~GameObject()
{
}

GameObject& GameObject::operator=(const GameObject& other)
{
    if (this == &other) return *this;

    name = other.name;
    active = other.active;
    componentOrder = other.componentOrder;
    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
    {
        if (other.components[i])
        {
            components[i] = other.components[i]->Clone(this);
        }
        else
        {
            components[i].reset();
        }
    }
    return *this;
}

std::unique_ptr<GameObject> GameObject::Duplicate(const std::string& newName) const
{
    auto clone = std::make_unique<GameObject>(*this);
    if (!newName.empty())
    {
        clone->SetName(newName);
    }
    return clone;
}

void GameObject::Update(float dt)
{
#ifdef _DEBUG
    ZoneScoped;
#endif

    if (!active) return;

    std::stack<GameObject*> stack;
    stack.push(this);

    while (!stack.empty())
    {
        GameObject* current = stack.top();
        stack.pop();

        if (!current || !current->IsActive()) continue;

        for (ComponentType type : current->componentOrder)
        {
            std::unique_ptr<Component>& comp = current->components[type];
            if (comp && comp->IsActive())
            {
                comp->Update(dt);
            }
        }

        const std::vector<GameObject*>& currentChildren = current->GetChildren();
        for (size_t i = currentChildren.size(); i > 0; --i)
        {
            GameObject* child = currentChildren[i - 1];
            if (child && child->IsActive())
            {
                stack.push(child);
            }
        }
    }
}

void GameObject::AddComponent(std::unique_ptr<Component> component)
{
    if (!component) return;

    ComponentType type = component->GetType();
    if (type < 0 || type >= COMPONENT_COUNT) return;

    if (type == COMPONENT_TRANSFORM && components[COMPONENT_TRANSFORM] != nullptr)
    {
        TransformComponent* existingTrans = static_cast<TransformComponent*>(components[COMPONENT_TRANSFORM].get());
        TransformComponent* incomingTrans = static_cast<TransformComponent*>(component.get());
        existingTrans->SetPosition(incomingTrans->GetPosition());
        existingTrans->SetRotation(incomingTrans->GetRotation());
        existingTrans->SetScale(incomingTrans->GetScale());
        if (incomingTrans->IsActive()) existingTrans->Enable();
        else existingTrans->Disable();
        return;
    }

    components[type] = std::move(component);
}

bool GameObject::RemoveComponent(ComponentType type)
{
    if (type <= COMPONENT_TRANSFORM || type >= COMPONENT_COUNT)
    {
        return false;
    }

    if (!components[type])
    {
        return false;
    }

    components[type].reset();
    return true;
}

Component* GameObject::GetComponentByType(ComponentType type)
{
    if (type < 0 || type >= COMPONENT_COUNT) return nullptr;
    return components[type].get();
}

const Component* GameObject::GetComponentByType(ComponentType type) const
{
    if (type < 0 || type >= COMPONENT_COUNT) return nullptr;
    return components[type].get();
}

bool GameObject::SwapComponentOrder(ComponentType typeA, ComponentType typeB)
{
    if (typeA == typeB) return false;
    if (typeA < 0 || typeA >= COMPONENT_COUNT || typeB < 0 || typeB >= COMPONENT_COUNT) return false;

    size_t idxA = COMPONENT_COUNT;
    size_t idxB = COMPONENT_COUNT;
    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
    {
        if (componentOrder[i] == typeA) idxA = i;
        if (componentOrder[i] == typeB) idxB = i;
    }

    if (idxA < COMPONENT_COUNT && idxB < COMPONENT_COUNT)
    {
        std::swap(componentOrder[idxA], componentOrder[idxB]);
        return true;
    }
    return false;
}

bool GameObject::MoveComponentUp(ComponentType type)
{
    size_t idx = COMPONENT_COUNT;
    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
    {
        if (componentOrder[i] == type)
        {
            idx = i;
            break;
        }
    }
    if (idx == 0 || idx >= COMPONENT_COUNT) return false;

    for (size_t i = idx; i > 0; --i)
    {
        ComponentType prevType = componentOrder[i - 1];
        if (components[prevType] != nullptr)
        {
            std::swap(componentOrder[idx], componentOrder[i - 1]);
            return true;
        }
    }
    return false;
}

bool GameObject::MoveComponentDown(ComponentType type)
{
    size_t idx = COMPONENT_COUNT;
    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
    {
        if (componentOrder[i] == type)
        {
            idx = i;
            break;
        }
    }
    if (idx >= COMPONENT_COUNT - 1) return false;

    for (size_t i = idx + 1; i < COMPONENT_COUNT; ++i)
    {
        ComponentType nextType = componentOrder[i];
        if (components[nextType] != nullptr)
        {
            std::swap(componentOrder[idx], componentOrder[i]);
            return true;
        }
    }
    return false;
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
    const GameObject* curr = parent;
    while (curr != nullptr)
    {
        if (const TransformComponent* t = curr->GetComponent<TransformComponent>())
        {
            return t->GetWorldMatrix();
        }
        curr = curr->parent;
    }
    return glm::mat4(1.0f);
}

void GameObject::InvalidateWorldTransform()
{
    if (TransformComponent* t = GetComponent<TransformComponent>())
    {
        t->InvalidateWorld();
        return;
    }

    std::stack<GameObject*> stack;
    for (size_t i = children.size(); i > 0; --i)
    {
        stack.push(children[i - 1]);
    }

    while (!stack.empty())
    {
        GameObject* current = stack.top();
        stack.pop();
        if (!current) continue;

        if (TransformComponent* t = current->GetComponent<TransformComponent>())
        {
            if (t->IsWorldDirty()) continue;
            t->SetWorldDirty();
        }

        const std::vector<GameObject*>& currentChildren = current->GetChildren();
        for (size_t i = currentChildren.size(); i > 0; --i)
        {
            stack.push(currentChildren[i - 1]);
        }
    }
}
