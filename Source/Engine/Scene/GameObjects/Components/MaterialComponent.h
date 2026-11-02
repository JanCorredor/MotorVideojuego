#pragma once

#include "Component.h"

#include "../../../Render/Texture/TextureManager.h"
#include <glm/glm.hpp>
#include <string>
#include <memory>

class GameObject;

// El material NO es dueño de las texturas GL (las gestiona el TextureManager, con caché y
// liberación garantizada). Guarda un puntero a la textura difusa original y un flag para
// mostrar la checkerboard sin perder esa textura.
class MaterialComponent : public Component
{
public:
	MaterialComponent(GameObject* owner) : Component(COMPONENT_MATERIAL, owner) {}
	
	virtual ~MaterialComponent() {}

	std::unique_ptr<Component> Clone(GameObject* newOwner) const override;

	glm::vec4 GetColor() const { return color; }
	void SetColor(glm::vec4 newColor) { color = newColor; }

	bool GetUseTexture() const { return useTexture; }
	void SetUseTexture(bool _useTexture) { useTexture = _useTexture; }

	// Textura difusa original (nullptr si no hay). Incluye path y tamaño.
	const TextureInfo* GetTexture() const { return diffuse; }
	// Devuelve false si la carga falla (la textura anterior se conserva)
	bool SetTexture(const std::string& filePath);

	bool HasTransparentAlpha() const
	{
		if (color.a < 0.99f) return true;
		return false;
	}


	// Alterna la checkerboard encima de la textura original, sin descartarla
	bool GetUseCheckerboard() const { return useCheckerboard; }
	void SetUseCheckerboard(bool enabled) { useCheckerboard = enabled; }

	bool GetUsingMissingTexture() const { return usingMissingTexture; }
	void SetUsingMissingTexture(bool enabled) { usingMissingTexture = enabled; } //Debug

	// ID GL que debe usarse al dibujar (0 = sin textura)
	unsigned int GetActiveTextureID() const;
	unsigned int GetTextureID() const { return GetActiveTextureID(); }

private:
	const TextureInfo* diffuse = nullptr;
	glm::vec4 color = glm::vec4(1.0f);
	bool useTexture = false;
	bool useCheckerboard = false;
	bool usingMissingTexture = false;
};
