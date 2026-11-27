#include "Resource.h"
#include "../FileSystem/FileSystem.h"
#include "../../Utils/UUID.h"

const char* ResourceTypeToString(ResourceType type)
{
	switch (type)
	{
	case ResourceType::Texture: return "Texture";
	case ResourceType::Mesh:    return "Mesh";
	case ResourceType::Model:   return "Model";
	default:                    return "Unknown";
	}
}

ResourceType ResourceTypeFromString(const std::string& text)
{
	if (text == "Texture") return ResourceType::Texture;
	if (text == "Mesh") return ResourceType::Mesh;
	if (text == "Model") return ResourceType::Model;
	return ResourceType::Unknown;
}

UID GenerateUID()
{
	// Los UID 1..999 quedan reservados para recursos internos (primitivas)
	constexpr UID FIRST_FREE_UID = 1000;
	UID uid = INVALID_UID;
	while (uid < FIRST_FREE_UID)
	{
		uid = static_cast<UID>(static_cast<uint64_t>(GameUUID()));
	}
	return uid;
}

std::string GetLibraryPathFor(ResourceType type, UID uid)
{
	const std::string fileName = std::to_string(uid);
	switch (type)
	{
	case ResourceType::Texture: return FileSystem::Join(FileSystem::LIBRARY_TEXTURES_DIR, fileName + FileSystem::TEXTURE_EXTENSION);
	case ResourceType::Mesh:    return FileSystem::Join(FileSystem::LIBRARY_MESHES_DIR, fileName + FileSystem::MESH_EXTENSION);
	case ResourceType::Model:   return FileSystem::Join(FileSystem::LIBRARY_MODELS_DIR, fileName + FileSystem::MODEL_EXTENSION);
	default:                    return std::string();
	}
}
