#pragma once

#include "Component.h"
#include "../../../ResourceManagement/Resource.h"

#include <glm/glm.hpp>
#include <memory>

class GameObject;
class ResourceTexture;

// Color y textura difusa. Como MeshComponent, no es dueño de la textura: guarda el UID de un
// ResourceTexture y lo pide al ResourceManager. La checkerboard y la "missing" son del TextureManager
class MaterialComponent : public Component
{
public:
	explicit MaterialComponent(GameObject* owner);
	~MaterialComponent() override;

	MaterialComponent(const MaterialComponent&) = delete;
	MaterialComponent& operator=(const MaterialComponent&) = delete;

	std::unique_ptr<Component> Clone(GameObject* newOwner) const override;

	void Save(nlohmann::json& out) const override;
	void Load(const nlohmann::json& in) override;

	glm::vec4 GetColor() const { return color; }
	void SetColor(glm::vec4 newColor) { if (color != newColor) { color = newColor; NotifyOwnerModified(); } }

	bool GetUseTexture() const { return useTexture; }
	void SetUseTexture(bool _useTexture) { if (useTexture != _useTexture) { useTexture = _useTexture; NotifyOwnerModified(); } }

	// Pide la nueva textura (+1 referencia) y suelta la anterior (-1). INVALID_UID = sin textura
	void SetTexture(UID uid);
	UID GetTextureUID() const { return textureUID; }
	const ResourceTexture* GetTexture() const { return texture; } // nullptr si no hay

	// La textura asignada ya no existe (asset borrado): se dibuja la "missing"
	bool IsTextureMissing() const { return missing; }
	void MarkTextureMissing();

	bool HasTransparentAlpha() const { return color.a < 0.99f; }

	// Alterna la checkerboard encima de la textura original, sin descartarla
	bool GetUseCheckerboard() const { return useCheckerboard; }
	void SetUseCheckerboard(bool enabled) { if (useCheckerboard != enabled) { useCheckerboard = enabled; NotifyOwnerModified(); } }

	// Fuerza la textura "missing" para depurar
	bool GetUsingMissingTexture() const { return usingMissingTexture; }
	void SetUsingMissingTexture(bool enabled) { if (usingMissingTexture != enabled) { usingMissingTexture = enabled; NotifyOwnerModified(); } }

	// ID GL que debe usarse al dibujar (0 = sin textura)
	unsigned int GetActiveTextureID() const;
	unsigned int GetTextureID() const { return GetActiveTextureID(); }

private:
	void ReleaseTexture();

	UID textureUID = INVALID_UID;
	ResourceTexture* texture = nullptr; // propiedad del ResourceManager
	glm::vec4 color = glm::vec4(1.0f);
	bool useTexture = false;
	bool useCheckerboard = false;
	bool usingMissingTexture = false;
	bool missing = false;
};
