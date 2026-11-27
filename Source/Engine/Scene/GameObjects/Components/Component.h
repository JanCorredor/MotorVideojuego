#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

class GameObject;

enum ComponentType
{
	COMPONENT_TRANSFORM,
	COMPONENT_MESH,
	COMPONENT_MATERIAL,
	COMPONENT_CAMERA,
	COMPONENT_COUNT //Mantener al final
};

// Nombre del componente en los ficheros de escena ("Transform", "Mesh"...)
const char* ComponentTypeToString(ComponentType type);
bool ComponentTypeFromString(const std::string& name, ComponentType& outType);

class Component
{
public:
	Component(ComponentType type, GameObject* owner) : type(type), owner(owner) {}
	virtual ~Component() {}

	virtual void Update(float dt) {}
    
    ComponentType GetType() const { return type; }
    GameObject* GetOwner() const { return owner; }
    
	bool IsActive() const { return active; }
	virtual void Enable() { if (!this->active) { this->active = true; NotifyOwnerModified(); } }
	virtual void Disable() { if (this->active) { this->active = false; NotifyOwnerModified(); } }

	void NotifyOwnerModified() const;

	virtual std::unique_ptr<Component> Clone(GameObject* newOwner) const = 0;

	// Serialización del Componente (Scene Serialization)
	virtual void Save(nlohmann::json& out) const = 0;
	virtual void Load(const nlohmann::json& in) = 0;

protected:
	ComponentType type;
	bool active = true;
	GameObject* owner = nullptr;
};
