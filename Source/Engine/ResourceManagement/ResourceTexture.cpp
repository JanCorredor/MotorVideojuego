#include "ResourceTexture.h"
#include "Importers/TextureImporter.h"
#include "../../Utils/Log.h"

#include <glad/glad.h>

ResourceTexture::~ResourceTexture()
{
	// El ResourceManager descarga todo en su CleanUp (con el contexto OpenGL aun vivo).
	// Esto es solo una red de seguridad.
	if (glId != 0)
	{
		UnloadFromMemory();
	}
}

bool ResourceTexture::LoadInMemory()
{
	TextureImporter::GPUTexture gpuTexture;
	if (!TextureImporter::LoadToGPU(libraryPath, gpuTexture))
	{
		LOG_ERROR("[Resources] No se pudo cargar la textura '%s' desde '%s'", name.c_str(), libraryPath.c_str());
		return false;
	}

	glId = gpuTexture.id;
	width = gpuTexture.width;
	height = gpuTexture.height;
	channels = gpuTexture.channels;
	gpuBytes = gpuTexture.bytes;
	return true;
}

void ResourceTexture::UnloadFromMemory()
{
	if (glId != 0)
	{
		glDeleteTextures(1, &glId);
		glId = 0;
	}
	gpuBytes = 0;
}
