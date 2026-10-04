#pragma once

#include "Component.h"
#include <glm/glm.hpp>

class MaterialComponent : public Component
{
public:
	MaterialComponent(GameObject* owner) : Component(COMPONENT_MATERIAL, owner) {}
	virtual ~MaterialComponent() {}

	unsigned int textureID = 0;
	glm::vec3 color = glm::vec3(1.0f);
	bool useTexture = false;
};
