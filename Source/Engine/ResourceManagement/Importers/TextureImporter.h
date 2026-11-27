#pragma once

#include "../ImportSettings.h"

#include <cstddef>
#include <string>

// Formato propio .tex (binario): cabecera "MTEX" (versión, tamaño, canales y filtros) y píxeles de
// abajo arriba, ya volteados y reducidos: cargarlo es leer el fichero y subirlo a la GPU
class TextureImporter
{
public:
	// .png .jpg .jpeg .tga .dds .bmp
	static bool IsSupportedExtension(const std::string& extension);

	// Asset (ruta de proyecto) -> fichero .tex en Library
	static bool Import(const std::string& assetPath, const TextureImportSettings& settings, const std::string& libraryPath);

	struct GPUTexture
	{
		unsigned int id = 0;
		int width = 0;
		int height = 0;
		int channels = 0;
		size_t bytes = 0; // memoria aproximada en GPU
	};

	// Fichero .tex -> textura OpenGL lista para usar
	static bool LoadToGPU(const std::string& libraryPath, GPUTexture& outTexture);
};
