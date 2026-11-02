#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>

class TransformComponent : public Component
{
public:
	TransformComponent(GameObject* owner) : Component(COMPONENT_TRANSFORM, owner), 
		position(0.0f), rotation(0.0f), scale(1.0f) {}

	virtual ~TransformComponent() {}

	std::unique_ptr<Component> Clone(GameObject* newOwner) const override;

	// Los setters solo invalidan la matriz si el valor cambia realmente
	void SetPosition(const glm::vec3& pos) { if (pos != position) { position = pos; MarkDirty(); } }
	void SetRotation(const glm::vec3& rot) { if (rot != rotation) { rotation = rot; MarkDirty(); } }
	void SetScale(const glm::vec3& scl) { if (scl != scale) { scale = scl; MarkDirty(); } }

	const glm::vec3& GetPosition() const { return position; }
	const glm::vec3& GetRotation() const { return rotation; } // grados (Euler XYZ)
	const glm::vec3& GetScale() const { return scale; }

	// Matriz local cacheada: solo se recalcula cuando cambia posición/rotación/escala
	const glm::mat4& GetLocalModelMatrix() const
	{
		if (dirty)
		{
			localMatrix = glm::mat4(1.0f);
			localMatrix = glm::translate(localMatrix, position);
			localMatrix = glm::rotate(localMatrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
			localMatrix = glm::rotate(localMatrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
			localMatrix = glm::rotate(localMatrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
			localMatrix = glm::scale(localMatrix, scale);
			dirty = false;
		}
		return localMatrix;
	}

	// Matriz global cacheada: WorldMatrix = ParentWorldMatrix * LocalMatrix
	const glm::mat4& GetWorldMatrix() const;

	// Invalida la matriz local y la global (este nodo y todos sus descendientes)
	void MarkDirty() { dirty = true; InvalidateWorld(); }

	// Invalida solo la matriz global de este nodo y sus descendientes 
	void InvalidateWorld();

private:
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;

	mutable glm::mat4 localMatrix = glm::mat4(1.0f);
	mutable glm::mat4 worldMatrix = glm::mat4(1.0f);
	mutable bool dirty = true;
	mutable bool worldDirty = true;
};
