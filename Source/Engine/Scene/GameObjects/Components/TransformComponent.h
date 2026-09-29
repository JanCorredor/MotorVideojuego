#pragma once

#include "Component.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class TransformComponent : public Component
{
public:
	TransformComponent(GameObject* owner) : Component(COMPONENT_TRANSFORM, owner), 
		position(0.0f), rotation(0.0f), scale(1.0f) {}

	virtual ~TransformComponent() {}

	void SetPosition(const glm::vec3& pos) { position = pos; }
	void SetRotation(const glm::vec3& rot) { rotation = rot; }
	void SetScale(const glm::vec3& scl) { scale = scl; }

	glm::vec3 GetPosition() const { return position; }
	glm::vec3 GetRotation() const { return rotation; }
	glm::vec3 GetScale() const { return scale; }

	glm::mat4 GetLocalModelMatrix() const
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, position);
		model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
		model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
		model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
		model = glm::scale(model, scale);
		return model;
	}

private:
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
};
