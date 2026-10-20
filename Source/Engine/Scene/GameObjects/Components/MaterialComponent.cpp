#include "MaterialComponent.h"
#include "../../../Engine.h"
#include "../../../ResourceManagement/Importers/TextureImporter.h"

bool MaterialComponent::SetTexture(const std::string& filePath)
{
	const TextureInfo* loaded = TextureImporter::ImportTexture(filePath);
	if (loaded == nullptr)
	{
		return false;
	}
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
	if (useTexture && diffuse) return diffuse->id;
	return 0;
}
