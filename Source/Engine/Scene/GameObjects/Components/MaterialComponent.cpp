#include "MaterialComponent.h"
#include "../../../Engine.h"
#include "../../../ResourceManagement/Importers/TextureImporter.h"
#include "../../../FileSystem/FileSystem.h"
#include "../../../../Utils/JsonHelpers.h"

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

void MaterialComponent::Save(nlohmann::json& out) const
{
	out["active"] = active;
	out["color"] = JsonHelpers::FromVec4(color);
	out["useTexture"] = useTexture;
	out["checkerboard"] = useCheckerboard;
	out["usingMissingTexture"] = usingMissingTexture;

	std::string texPath = (diffuse != nullptr && diffuse->path.find("(procedural)") == std::string::npos)
		? FileSystem::EnsureAssetInProject(diffuse->path, "Textures")
		: "";
	out["texturePath"] = texPath;

	if (diffuse != nullptr && !texPath.empty())
	{
		out["minFilter"] = diffuse->importSettings.minFilter;
		out["magFilter"] = diffuse->importSettings.magFilter;
		out["wrapMode"] = diffuse->importSettings.wrapMode;
		out["flipX"] = diffuse->importSettings.flipX;
		out["flipY"] = diffuse->importSettings.flipY;
	}
}

void MaterialComponent::Load(const nlohmann::json& in)
{
	active = JsonHelpers::Get(in, "active", true);
	color = JsonHelpers::GetVec4(in, "color", glm::vec4(1.0f));
	useTexture = JsonHelpers::Get(in, "useTexture", false);
	useCheckerboard = JsonHelpers::Get(in, "checkerboard", false);
	bool savedMissing = JsonHelpers::Get(in, "usingMissingTexture", false);
	std::string texPath = JsonHelpers::Get(in, "texturePath", std::string());

	if (!texPath.empty() && texPath.find("(procedural)") == std::string::npos)
	{
		std::string resolvedTex = FileSystem::ResolveExistingPath(texPath);
		if (in.contains("minFilter"))
		{
			TextureSettings ts;
			ts.minFilter = JsonHelpers::Get(in, "minFilter", 0);
			ts.magFilter = JsonHelpers::Get(in, "magFilter", 0);
			ts.wrapMode = JsonHelpers::Get(in, "wrapMode", 0);
			ts.flipX = JsonHelpers::Get(in, "flipX", false);
			ts.flipY = JsonHelpers::Get(in, "flipY", false);
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
