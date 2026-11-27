#pragma once

#include "Resource.h"

// Textura en Library/Textures/<UID>.tex; mientras alguien la usa también vive en la GPU
class ResourceTexture : public Resource
{
public:
	explicit ResourceTexture(UID uid) : Resource(uid, ResourceType::Texture) {}
	~ResourceTexture() override;

	unsigned int GetGLID() const { return glId; } // 0 si no esta cargada
	int GetWidth() const { return width; }
	int GetHeight() const { return height; }
	int GetChannels() const { return channels; }  // 3 = RGB, 4 = RGBA

	size_t GetMemoryUsage() const override { return gpuBytes; }

protected:
	bool LoadInMemory() override;
	void UnloadFromMemory() override;

private:
	unsigned int glId = 0;
	int width = 0;
	int height = 0;
	int channels = 0;
	size_t gpuBytes = 0;
};
