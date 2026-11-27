#pragma once

#include "../../Module.h"

#include <memory>
#include <string>

// Datos de una textura procedural residente en GPU. La propiedad es del TextureManager.
struct TextureInfo
{
	unsigned int id = 0;
	int width = 0;
	int height = 0;
	std::string path;
};

// Modulo de texturas:
//   - Inicializa y cierra la libreria DevIL (la usa el TextureImporter para leer png/jpg/tga...).
//   - Crea las texturas procedurales del editor: checkerboard y "missing texture".
// Las texturas de los assets NO viven aqui: son recursos (ResourceTexture) con conteo de
// referencias, gestionados por el ResourceManager.
// Debe registrarse DESPUÉS de Window (necesita contexto GL) y ANTES del ResourceManager.
class TextureManager : public Module
{
public:
	TextureManager();
	virtual ~TextureManager();

	bool Start() override;
	bool CleanUp() override;

	// Textura 64x64 con casillas blancas/negras (siempre disponible tras Start)
	const TextureInfo* GetCheckerboard() const { return checkerboard.get(); }
	// Textura rosa/negra que se ve cuando la textura asignada ya no existe
	const TextureInfo* GetMissingTexture() const { return missingTexture.get(); }

private:
	void CreateCheckerboard();
	std::unique_ptr<TextureInfo> checkerboard;

	void CreateMissingTexture();
	std::unique_ptr<TextureInfo> missingTexture;

	bool devilInitialized = false;
};
