#pragma once

#include "Component.h"

#include "../../../Render/Texture/TextureLoader.h"
#include <glm/glm.hpp>

class GameObject;

class MaterialComponent : public Component
{
public:
	MaterialComponent(GameObject* owner) : Component(COMPONENT_MATERIAL, owner) {}
	
	virtual ~MaterialComponent() 
	{

	}

	glm::vec3 GetColor() { return color; }
	void SetColor(glm::vec3 newColor) { color = newColor; }

	bool GetUseTexture() { return useTexture; }
	void SetUseTexture(bool _useTexture) { useTexture = _useTexture; }

	unsigned int GetTextureID() { return textureID; }
	void SetTexture(std::string filePath) { textureID = LoadTexture(filePath.c_str()); }

private:
	unsigned int textureID = 0;
	glm::vec3 color = glm::vec3(1.0f);
	bool useTexture = false;
};
