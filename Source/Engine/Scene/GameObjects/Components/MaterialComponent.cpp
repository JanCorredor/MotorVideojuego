#include "MaterialComponent.h"
#include "../../../Engine.h"
#include "../../../Render/Texture/TextureManager.h"
#include "../../../ResourceManagement/ResourceManager.h"
#include "../../../ResourceManagement/ResourceTexture.h"
#include "../../../../Utils/JsonHelpers.h"
#include "../../../../Utils/Log.h"

#include <nlohmann/json.hpp>

MaterialComponent::MaterialComponent(GameObject* owner) : Component(COMPONENT_MATERIAL, owner)
{
}

MaterialComponent::~MaterialComponent()
{
	ReleaseTexture();
}

std::unique_ptr<Component> MaterialComponent::Clone(GameObject* newOwner) const
{
	auto clone = std::make_unique<MaterialComponent>(newOwner);
	clone->color = color;
	clone->useTexture = useTexture;
	clone->useCheckerboard = useCheckerboard;
	clone->usingMissingTexture = usingMissingTexture;
	clone->SetTexture(textureUID); // el clon pide su propia referencia
	if (!active) clone->Disable();
	return clone;
}

void MaterialComponent::SetTexture(UID uid)
{
	if (uid == textureUID && !missing) return;

	ResourceManager* resources = Engine::GetInstance().resources.get();

	// Se pide la nueva antes de soltar la vieja: si fuese la misma no se descarga
	ResourceTexture* newTexture = nullptr;
	bool newMissing = false;
	if (uid != INVALID_UID && resources != nullptr)
	{
		Resource* resource = resources->RequestResource(uid);
		if (resource != nullptr && resource->GetType() == ResourceType::Texture)
		{
			newTexture = static_cast<ResourceTexture*>(resource);
		}
		else
		{
			if (resource != nullptr) resources->ReleaseResource(uid);
			newMissing = true;
			LOG_WARNING("[Material] La textura con UID %llu no existe (¿asset borrado?)", (unsigned long long)uid);
		}
	}

	ReleaseTexture();

	textureUID = uid;
	texture = newTexture;
	missing = newMissing;
	NotifyOwnerModified();
}

void MaterialComponent::ReleaseTexture()
{
	if (texture != nullptr)
	{
		if (ResourceManager* resources = Engine::GetInstance().resources.get())
		{
			resources->ReleaseResource(textureUID);
		}
		texture = nullptr;
	}
}

void MaterialComponent::MarkTextureMissing()
{
	ReleaseTexture();
	missing = (textureUID != INVALID_UID);
}

unsigned int MaterialComponent::GetActiveTextureID() const
{
	const TextureManager* textures = Engine::GetInstance().textures.get();

	if (useCheckerboard && textures && textures->GetCheckerboard())
	{
		return textures->GetCheckerboard()->id;
	}

	// Textura borrada, que no se pudo cargar o forzada para depurar: se ve la "missing"
	const bool broken = textureUID != INVALID_UID && (missing || texture == nullptr || texture->GetGLID() == 0);
	if (usingMissingTexture || (useTexture && broken))
	{
		return (textures && textures->GetMissingTexture()) ? textures->GetMissingTexture()->id : 0;
	}

	if (!useTexture || texture == nullptr) return 0;
	return texture->GetGLID();
}

void MaterialComponent::Save(nlohmann::json& out) const
{
	out["active"] = active;
	out["color"] = JsonHelpers::FromVec4(color);
	out["useTexture"] = useTexture;
	out["checkerboard"] = useCheckerboard;
	out["usingMissingTexture"] = usingMissingTexture;
	out["texture"] = textureUID;
}

void MaterialComponent::Load(const nlohmann::json& in)
{
	color = JsonHelpers::GetVec4(in, "color", glm::vec4(1.0f));
	useTexture = JsonHelpers::Get(in, "useTexture", false);
	useCheckerboard = JsonHelpers::Get(in, "checkerboard", false);
	usingMissingTexture = JsonHelpers::Get(in, "usingMissingTexture", false);
	SetTexture(JsonHelpers::Get(in, "texture", (UID)INVALID_UID));

	if (JsonHelpers::Get(in, "active", true)) Enable();
	else Disable();
}
