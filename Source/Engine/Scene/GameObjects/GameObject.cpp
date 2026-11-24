#include "GameObject.h"
#include "../Scene.h"
#include "../../Engine.h"
#include "./Components/Component.h"
#include "./Components/TransformComponent.h"
#include "./Components/MeshComponent.h"
#include "./Components/MaterialComponent.h"
#include "./Components/CameraComponent.h"

#include "AABB/AABB.h"

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
    : uuid(GameUUID()), name(other.name), active(other.active), isStatic(other.isStatic), componentOrder(other.componentOrder), parent(nullptr)
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
    isStatic = other.isStatic;
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

void GameObject::Save(SceneFile& file) const
{
    file.SetUInt64("uuid", (uint64_t)uuid);
    uint64_t parentUuid = (parent != nullptr && parent->GetParent() != nullptr) ? (uint64_t)parent->GetUUID() : 0;
    file.SetUInt64("parent_uuid", parentUuid);
    file.SetString("name", name);
    file.SetBool("active", active);
    file.SetBool("static", isStatic);

    // 1. Pasar Save(file) a todos sus componentes en su orden actual
    SceneFile componentsArray = file.AddArray("components");
    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
    {
        ComponentType type = componentOrder[i];
        const std::unique_ptr<Component>& comp = components[type];
        if (comp)
        {
            SceneFile compFile = componentsArray.PushObject();
            compFile.SetInt("type", (int)type);
            comp->Save(compFile);
        }
    }

    // 2. Pasar Save(file) a todos sus hijos
    SceneFile childrenArray = file.AddArray("children");
    for (size_t i = 0; i < children.size(); ++i)
    {
        GameObject* child = children[i];
        if (child)
        {
            SceneFile childFile = childrenArray.PushObject();
            child->Save(childFile);
        }
    }
}

void GameObject::Load(const SceneFile& file)
{
    uuid = GameUUID(file.GetUInt64("uuid", (uint64_t)uuid));
    name = file.GetString("name", name);
    active = file.GetBool("active", true);
    isStatic = file.GetBool("static", false);

    for (size_t i = 0; i < COMPONENT_COUNT; ++i)
    {
        components[i].reset();
    }

    // 1. Cargar todos sus componentes pasando Load(file) a cada uno
    SceneFile componentsArray = file.GetArray("components");
    std::array<ComponentType, COMPONENT_COUNT> loadedOrder = {
        COMPONENT_TRANSFORM, COMPONENT_MESH, COMPONENT_MATERIAL, COMPONENT_CAMERA
    };
    std::array<bool, COMPONENT_COUNT> usedInOrder = { false, false, false, false };
    size_t orderIdx = 0;

    for (size_t i = 0; i < componentsArray.Size(); ++i)
    {
        SceneFile compFile = componentsArray.GetElement(i);
        int typeInt = compFile.GetInt("type", -1);
        if (typeInt < 0 || typeInt >= COMPONENT_COUNT) continue;
        ComponentType type = static_cast<ComponentType>(typeInt);

        std::unique_ptr<Component> comp = nullptr;
        switch (type)
        {
        case COMPONENT_TRANSFORM:
            comp = std::make_unique<TransformComponent>(this);
            break;
        case COMPONENT_MESH:
            comp = std::make_unique<MeshComponent>(this);
            break;
        case COMPONENT_MATERIAL:
            comp = std::make_unique<MaterialComponent>(this);
            break;
        case COMPONENT_CAMERA:
            comp = std::make_unique<CameraComponent>(this);
            break;
        default:
            break;
        }

        if (comp)
        {
            comp->Load(compFile);
            AddComponent(std::move(comp));
            if (!usedInOrder[type] && orderIdx < COMPONENT_COUNT)
            {
                loadedOrder[orderIdx++] = type;
                usedInOrder[type] = true;
            }
        }
    }

    for (int t = 0; t < COMPONENT_COUNT; ++t)
    {
        if (!usedInOrder[t] && orderIdx < COMPONENT_COUNT)
        {
            loadedOrder[orderIdx++] = static_cast<ComponentType>(t);
        }
    }
    componentOrder = loadedOrder;

    // 2. Cargar todos sus hijos pasando Load(file) a cada hijo
    Scene* scene = Engine::GetInstance().scene.get();
    SceneFile childrenArray = file.GetArray("children");
    for (size_t i = 0; i < childrenArray.Size(); ++i)
    {
        SceneFile childFile = childrenArray.GetElement(i);
        uint64_t childUuid = childFile.GetUInt64("uuid", 0);
        std::string childName = childFile.GetString("name", "GameObject");

        auto childObj = std::make_unique<GameObject>(childName, childUuid != 0 ? GameUUID(childUuid) : GameUUID());
        GameObject* childPtr = childObj.get();
        if (scene)
        {
            scene->Register(std::move(childObj), this);
        }
        else
        {
            AddChild(childPtr);
        }
        childPtr->Load(childFile);
    }
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
        NotifySceneModified();
        return;
    }

    components[type] = std::move(component);
    NotifySceneModified();
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
    NotifySceneModified();
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
        NotifySceneModified();
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
            NotifySceneModified();
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
            NotifySceneModified();
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

    if (Scene* scene = Engine::GetInstance().scene.get())
    {
        GameObject* sel = scene->GetSelected();
        if (sel != nullptr && (sel == this || sel->IsDescendantOf(this)))
        {
            scene->NotifySelectionHierarchyChanged();
        }
    }
    NotifySceneModified();
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

void GameObject::SetStatic(bool value, bool applyToChildren)
{
    bool changed = (isStatic != value);
    isStatic = value;
    if (!applyToChildren)
    {
        if (changed) NotifySceneModified();
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

        if (current->isStatic != value) changed = true;
        current->isStatic = value;
        const std::vector<GameObject*>& currentChildren = current->GetChildren();
        for (size_t i = currentChildren.size(); i > 0; --i)
        {
            stack.push(currentChildren[i - 1]);
        }
    }
    if (changed) NotifySceneModified();
}

void GameObject::NotifySceneModified() const
{
    Scene* scene = Engine::GetInstance().scene.get();
    if (scene && scene->OwnsObject(this))
    {
        scene->MarkSceneDirty();
    }
}

void Component::NotifyOwnerModified() const
{
    if (owner)
    {
        owner->NotifySceneModified();
    }
}

bool GameObject::HasWorldAABB() const
{
    const MeshComponent* mesh = GetComponent<MeshComponent>();
    return mesh != nullptr && mesh->IsActive() && mesh->GetPrimitive() != nullptr;
}

AABB GameObject::GetWorldAABB() const
{
    const MeshComponent* mesh = GetComponent<MeshComponent>();
    if (mesh && mesh->GetPrimitive())
    {
        const Primitive* prim = mesh->GetPrimitive();
        return AABB::FromTransformedLocalBox(prim->GetBoundsMin(), prim->GetBoundsMax(), GetWorldMatrix());
    }
    glm::vec3 pos = glm::vec3(GetWorldMatrix()[3]);
    return AABB(pos - glm::vec3(0.1f), pos + glm::vec3(0.1f));
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
