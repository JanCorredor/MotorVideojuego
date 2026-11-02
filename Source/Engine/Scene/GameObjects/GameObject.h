#pragma once

#include "../../../Utils/UUID.h"
#include "./Components/Component.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <array>

class TransformComponent;
class MeshComponent;
class MaterialComponent;
class CameraComponent;

class GameObject
{
public:
	GameObject(const std::string& name = "GameObject", GameUUID uuid = GameUUID());
	// Constructor de copia para clonado profundo de componentes con un nuevo UUID
	GameObject(const GameObject& other);
	~GameObject();

	// Operador de asignación por copia (clonado profundo)
	GameObject& operator=(const GameObject& other);

	// Duplicar el GameObject (clonado profundo de todos sus componentes y nuevo UUID generado)
	std::unique_ptr<GameObject> Duplicate(const std::string& newName = "") const;
	std::unique_ptr<GameObject> Clone(const std::string& newName = "") const;

	GameUUID GetUUID() const { return uuid; }

	void Update(float dt);

	// Añadir componente (transferencia directa)
	void AddComponent(std::unique_ptr<Component> component);

	// Acceso rápido en O(1) directo sin dynamic_cast
	template <typename T>
	T* GetComponent()
	{
		// Obtenemos el ID de tipo en tiempo de compilación mediante especialización implícita
		constexpr ComponentType typeId = GetComponentTypeID<T>();
		return static_cast<T*>(components[typeId].get());
	}

	template <typename T>
	const T* GetComponent() const
	{
		constexpr ComponentType typeId = GetComponentTypeID<T>();
		return static_cast<const T*>(components[typeId].get());
	}

	bool SetParent(GameObject* newParent);
	bool AddChild(GameObject* child);
	void RemoveChild(GameObject* child);

	bool IsDescendantOf(const GameObject* ancestor) const;

	GameObject* GetParent() const { return parent; }
	const std::vector<GameObject*>& GetChildren() const { return children; }

	void SetName(const std::string& n) { name = n; }
	const std::string& GetName() const { return name; }

	bool IsActive() const { return active; }
	void Enable() { this->active = true; }
	void Disable() { this->active = false; }

	glm::mat4 GetWorldMatrix() const;
	void InvalidateWorldTransform();

private:
	// Helper en tiempo de compilación para mapear Tipos C++ a índices del Array
	template<typename T>
	static constexpr ComponentType GetComponentTypeID();

	GameUUID uuid;

	bool active = true;
	std::string name;

	// Array fijo indexado por enum ComponentType (COMPONENT_TRANSFORM=0, COMPONENT_MESH=1, etc.)
	// Acceso en O(1) directo sin bucles ni asignaciones dinámicas extra
	std::array<std::unique_ptr<Component>, COMPONENT_COUNT> components;

	GameObject* parent = nullptr;
	std::vector<GameObject*> children;
};

// Especializaciones del Helper para resolver tipos en tiempo de compilación
template<> constexpr ComponentType GameObject::GetComponentTypeID<TransformComponent>() { return COMPONENT_TRANSFORM; }
template<> constexpr ComponentType GameObject::GetComponentTypeID<MeshComponent>() { return COMPONENT_MESH; }
template<> constexpr ComponentType GameObject::GetComponentTypeID<MaterialComponent>() { return COMPONENT_MATERIAL; }
template<> constexpr ComponentType GameObject::GetComponentTypeID<CameraComponent>() { return COMPONENT_CAMERA; }
