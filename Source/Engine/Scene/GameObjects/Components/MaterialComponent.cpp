#include "MaterialComponent.h"
#include "../../../Engine.h"
#include "../../../ResourceManagement/Importers/TextureImporter.h"

std::unique_ptr<Component> MaterialComponent::Clone(GameObject* newOwner) const
{
	auto clone = std::make_unique<MaterialComponent>(newOwner);
	clone->diffuse = this->diffuse;
	clone->color = this->color;
	clone->useTexture = this->useTexture;
	clone->useCheckerboard = this->useCheckerboard;
	clone->usingMissingTexture = this->usingMissingTexture;
	if (!active) clone->Disable();
	return clone;
}

bool MaterialComponent::SetTexture(const std::string& filePath)
{
	const TextureInfo* loaded = TextureImporter::ImportTexture(filePath);
	if (loaded == nullptr)
	{
		usingMissingTexture = true;
		diffuse = Engine::GetInstance().textures->GetMissingTexture();
		return false;
	}
	usingMissingTexture = false;
	diffuse = loaded;
	return true;
}

unsigned int MaterialComponent::GetActiveTextureID() const
{
	if (useCheckerboard)
	{
		const TextureInfo* checker = Engine::GetInstance().textures->GetCheckerboard();
		if (checker) return checker->id;
	}
	if (usingMissingTexture)
	{
		const TextureInfo* missingTexture = Engine::GetInstance().textures->GetMissingTexture();
		if (missingTexture) return missingTexture->id;
	}
	if (useTexture && diffuse) return diffuse->id;
	return 0;
}
