#pragma once

#include "../../Module.h"

#include <memory>
#include <string>
#include <unordered_map>

// Datos de una textura residente en GPU. La propiedad es del TextureManager.
struct TextureInfo
{
	unsigned int id = 0;
	int width = 0;
	int height = 0;
	std::string path;
};

// Parámetros por defecto de las texturas. Los valores son índices (los traduce a GL el TextureManager).
struct TextureSettings
{
	int minFilter = 0; // 0 LINEAR_MIPMAP_LINEAR, 1 LINEAR, 2 NEAREST
	int magFilter = 0; // 0 LINEAR, 1 NEAREST
	int wrapMode = 0;  // 0 REPEAT, 1 CLAMP_TO_EDGE, 2 MIRRORED_REPEAT
};

// Único dueño de las texturas GL y de DevIL: carga con caché por ruta, textura
// checkerboard procedural (se crea una sola vez) y liberación garantizada en CleanUp.
// Debe registrarse DESPUÉS de Window (necesita contexto GL) y ANTES de Scene.
class TextureManager : public Module
{
public:
	TextureManager();
	virtual ~TextureManager();

	bool Start() override;
	bool CleanUp() override;

	// Devuelve nullptr si la carga falla. Si la ruta ya estaba cargada reutiliza la textura.
	const TextureInfo* Load(const std::string& path);

	// Textura 64x64 con casillas blancas/negras (siempre disponible tras Start)
	const TextureInfo* GetCheckerboard() const { return checkerboard.get(); }

	const TextureSettings& GetSettings() const { return settings; }
	// Aplica los parámetros a las texturas ya cargadas y a las futuras
	void SetSettings(const TextureSettings& newSettings);

	size_t GetLoadedCount() const { return cache.size(); }

private:
	void ApplyParameters(unsigned int textureId, bool hasMipmaps) const;
	void CreateCheckerboard();

	std::unordered_map<std::string, std::unique_ptr<TextureInfo>> cache;
	std::unique_ptr<TextureInfo> checkerboard;
	TextureSettings settings;
	bool devilInitialized = false;
};
