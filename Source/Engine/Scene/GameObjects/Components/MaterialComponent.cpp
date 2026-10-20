#include "MaterialComponent.h"
#include "../../../Engine.h"

bool MaterialComponent::SetTexture(const std::string& filePath)
{
	const TextureInfo* loaded = Engine::GetInstance().textures->Load(filePath);
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
