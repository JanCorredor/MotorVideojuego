#include "MaterialComponent.h"
#include "../../../Engine.h"
#include "../../../ResourceManagement/Importers/TextureImporter.h"
#include "../../../FileSystem/FileSystem.h"

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

void MaterialComponent::Save(SceneFile& file) const
{
	file.SetBool("active", active);
	file.SetVec4("color", color);
	file.SetBool("use_texture", useTexture);
	file.SetBool("use_checkerboard", useCheckerboard);
	file.SetBool("using_missing_texture", usingMissingTexture);

	std::string texPath = (diffuse != nullptr && diffuse->path.find("(procedural)") == std::string::npos)
		? FileSystem::EnsureAssetInProject(diffuse->path, "Textures")
		: "";
	file.SetString("texture_path", texPath);

	if (diffuse != nullptr && !texPath.empty())
	{
		file.SetInt("min_filter", diffuse->importSettings.minFilter);
		file.SetInt("mag_filter", diffuse->importSettings.magFilter);
		file.SetInt("wrap_mode", diffuse->importSettings.wrapMode);
		file.SetBool("flip_x", diffuse->importSettings.flipX);
		file.SetBool("flip_y", diffuse->importSettings.flipY);
	}
}

void MaterialComponent::Load(const SceneFile& file)
{
	active = file.GetBool("active", true);
	color = file.GetVec4("color", glm::vec4(1.0f));
	useTexture = file.GetBool("use_texture", false);
	useCheckerboard = file.GetBool("use_checkerboard", false);
	bool savedMissing = file.GetBool("using_missing_texture", false);
	std::string texPath = file.GetString("texture_path", "");

	if (!texPath.empty() && texPath.find("(procedural)") == std::string::npos)
	{
		std::string resolvedTex = FileSystem::ResolveExistingPath(texPath);
		if (file.HasKey("min_filter"))
		{
			TextureSettings ts;
			ts.minFilter = file.GetInt("min_filter", 0);
			ts.magFilter = file.GetInt("mag_filter", 0);
			ts.wrapMode = file.GetInt("wrap_mode", 0);
			ts.flipX = file.GetBool("flip_x", false);
			ts.flipY = file.GetBool("flip_y", false);
			diffuse = TextureImporter::ImportTexture(resolvedTex, ts, false);
			usingMissingTexture = (diffuse == nullptr);
			if (usingMissingTexture && Engine::GetInstance().textures)
			{
				diffuse = Engine::GetInstance().textures->GetMissingTexture();
			}
		}
		else
		{
			SetTexture(resolvedTex);
		}
	}
	else if (savedMissing && Engine::GetInstance().textures)
	{
		usingMissingTexture = true;
		diffuse = Engine::GetInstance().textures->GetMissingTexture();
	}
	else
	{
		diffuse = nullptr;
		usingMissingTexture = false;
	}
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
	NotifyOwnerModified();
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
