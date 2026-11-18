#include "TextureManager.h"
#include "../../../Utils/Log.h"

#include <IL/il.h>
#include <glad/glad.h>
#include <vector>

static GLint ToGLMinFilter(int idx)
{
	switch (idx)
	{
	case 1: return GL_LINEAR;
	case 2: return GL_NEAREST;
	default: return GL_LINEAR_MIPMAP_LINEAR;
	}
}

static GLint ToGLMagFilter(int idx) { return idx == 1 ? GL_NEAREST : GL_LINEAR; }

static GLint ToGLWrap(int idx)
{
	switch (idx)
	{
	case 1: return GL_CLAMP_TO_EDGE;
	case 2: return GL_MIRRORED_REPEAT;
	default: return GL_REPEAT;
	}
}

// Apllies Flips (X &/or Y)
static void FlipRGBAImage(uint32_t* pixels, int width, int height, bool flipX, bool flipY)
{
	if (!pixels || width <= 0 || height <= 0 || (!flipX && !flipY)) return;

	if (flipY)
	{
		std::vector<uint32_t> rowBuffer(width);
		const size_t rowBytes = static_cast<size_t>(width) * sizeof(uint32_t);
		for (int y = 0; y < height / 2; ++y)
		{
			uint32_t* topRow = pixels + static_cast<size_t>(y) * width;
			uint32_t* bottomRow = pixels + static_cast<size_t>(height - 1 - y) * width;
			std::memcpy(rowBuffer.data(), topRow, rowBytes);
			std::memcpy(topRow, bottomRow, rowBytes);
			std::memcpy(bottomRow, rowBuffer.data(), rowBytes);
		}
	}

	if (flipX)
	{
		for (int y = 0; y < height; ++y)
		{
			uint32_t* row = pixels + static_cast<size_t>(y) * width;
			std::reverse(row, row + width);
		}
	}
}


TextureManager::TextureManager() : Module()
{
	name = "textures";
}

TextureManager::~TextureManager()
{
}

bool TextureManager::Start()
{
	ilInit();
	devilInitialized = true;
	LOG("[DevIL] Inicializada librería DevIL (Versión interna: %d)", (int)ilGetInteger(IL_VERSION_NUM));

	CreateCheckerboard();
	CreateMissingTexture();
	return true;
}

bool TextureManager::CleanUp()
{
	size_t freed = 0;
	for (std::pair<const std::string, std::unique_ptr<TextureInfo>>& entry : cache)
	{
		if (entry.second->id != 0)
		{
			glDeleteTextures(1, &entry.second->id);
			freed++;
		}
	}
	cache.clear();

	if (checkerboard && checkerboard->id != 0)
	{
		glDeleteTextures(1, &checkerboard->id);
		freed++;
	}
	checkerboard.reset();

	if (missingTexture && missingTexture->id != 0)
	{
		glDeleteTextures(1, &missingTexture->id);
		freed++;
	}
	missingTexture.reset();

	LOG("[Texture] %zu texturas OpenGL liberadas", freed);

	if (devilInitialized)
	{
		ilShutDown();
		devilInitialized = false;
		LOG("[DevIL] Librería DevIL cerrada");
	}
	return true;
}

void TextureManager::ApplyParameters(unsigned int textureId, const TextureSettings& texSettings, bool hasMipmaps) const
{
	glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, ToGLWrap(texSettings.wrapMode));
	glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, ToGLWrap(texSettings.wrapMode));
	GLint minFilter = ToGLMinFilter(texSettings.minFilter);
	// Sin mipmaps un filtro mipmap dejaría la textura incompleta
	if (!hasMipmaps && minFilter == GL_LINEAR_MIPMAP_LINEAR) minFilter = GL_LINEAR;
	glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(texSettings.magFilter));
}

void TextureManager::SetSettings(const TextureSettings& newSettings)
{
	settings = newSettings;
	for (std::pair<const std::string, std::unique_ptr<TextureInfo>>& entry : cache)
	{
		entry.second->importSettings.minFilter = settings.minFilter;
		entry.second->importSettings.magFilter = settings.magFilter;
		entry.second->importSettings.wrapMode = settings.wrapMode;
		ApplyParameters(entry.second->id, entry.second->importSettings, true);
	}
	LOG("[Texture] Parámetros por defecto actualizados (min=%d, mag=%d, wrap=%d, flipX=%d, flipY=%d)",
		settings.minFilter, settings.magFilter, settings.wrapMode, (int)settings.flipX, (int)settings.flipY);
}

const TextureInfo* TextureManager::Load(const std::string& path)
{
	return ImportWithSettings(path, settings, false);
}

const TextureInfo* TextureManager::ImportWithSettings(const std::string& path, const TextureSettings& customSettings, bool forceReload)
{
	auto it = cache.find(path);
	if (it != cache.end() && !forceReload)
	{
		LOG("[Texture] '%s' ya estaba en caché (OpenGL ID: %u)", path.c_str(), it->second->id);
		return it->second.get();
	}

	// If Flip was not changed do not reload
	if (it != cache.end() && forceReload &&
		it->second->importSettings.flipX == customSettings.flipX &&
		it->second->importSettings.flipY == customSettings.flipY)
	{
		it->second->importSettings = customSettings;
		ApplyParameters(it->second->id, customSettings, true);
		LOG("[Texture] Parámetros de muestreo actualizados en GPU para '%s' (min=%d, mag=%d, wrap=%d)",
			path.c_str(), customSettings.minFilter, customSettings.magFilter, customSettings.wrapMode);
		return it->second.get();
	}

	// Si ya está en caché y solo cambió flipX/flipY, volteamos directamente desde VRAM sin tocar disco ni DevIL
	if (it != cache.end() && forceReload)
	{
		TextureInfo* existing = it->second.get();
		const bool flipXChanged = (existing->importSettings.flipX != customSettings.flipX);
		const bool flipYChanged = (existing->importSettings.flipY != customSettings.flipY);

		if (flipXChanged || flipYChanged)
		{
			std::vector<uint32_t> pixels(static_cast<size_t>(existing->width) * existing->height);
			glGetTextureImage(
				existing->id,
				0,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				static_cast<GLsizei>(pixels.size() * sizeof(uint32_t)),
				pixels.data()
			);

			// Solo aplicamos el eje que haya cambiado respecto al estado actual en GPU
			FlipRGBAImage(pixels.data(), existing->width, existing->height, flipXChanged, flipYChanged);

			// glTextureSubImage2D es más rápido que glTexImage2D porque no realoja memoria en GPU
			glTextureSubImage2D(
				existing->id,
				0,
				0, 0,
				existing->width,
				existing->height,
				GL_RGBA,
				GL_UNSIGNED_BYTE,
				pixels.data()
			);
			glGenerateTextureMipmap(existing->id);
		}

		existing->importSettings = customSettings;
		ApplyParameters(existing->id, customSettings, true);
		return existing;
	}


	ILuint imageID;
	ilGenImages(1, &imageID);
	ilBindImage(imageID);

	if (!ilLoadImage(path.c_str()))
	{
		ILenum err = ilGetError();
		LOG_ERROR("[DevIL] Error loading texture '%s' (Código de error DevIL: 0x%X)", path.c_str(), (unsigned int)err);
		ilDeleteImages(1, &imageID);
		return nullptr;
	}

	ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

	int width = ilGetInteger(IL_IMAGE_WIDTH);
	int height = ilGetInteger(IL_IMAGE_HEIGHT);
	int bpp = ilGetInteger(IL_IMAGE_BPP);

	uint32_t* pixelData = reinterpret_cast<uint32_t*>(ilGetData());
	FlipRGBAImage(pixelData, width, height, customSettings.flipX, customSettings.flipY);

	unsigned int textureID = 0;
	if (it != cache.end() && it->second->id != 0)
	{
		// Reuse same OpenGL ID so to not invalidate active references
		textureID = it->second->id;
	}
	else
	{
		glGenTextures(1, &textureID);
	}

	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
	glGenerateMipmap(GL_TEXTURE_2D);
	ApplyParameters(textureID, customSettings, true);
	glBindTexture(GL_TEXTURE_2D, 0);

	ilDeleteImages(1, &imageID);
	ilBindImage(0);

	if (it != cache.end())
	{
		it->second->id = textureID;
		it->second->width = width;
		it->second->height = height;
		it->second->importSettings = customSettings;
		LOG("[DevIL] Texture reimported: %s (%dx%d, flipX=%d, flipY=%d, min=%d, mag=%d, wrap=%d)",
			path.c_str(), width, height, (int)customSettings.flipX, (int)customSettings.flipY,
			customSettings.minFilter, customSettings.magFilter, customSettings.wrapMode);
		return it->second.get();
	}

	auto info = std::make_unique<TextureInfo>();
	info->id = textureID;
	info->width = width;
	info->height = height;
	info->path = path;
	info->importSettings = customSettings;
	const TextureInfo* result = info.get();
	cache[path] = std::move(info);

	LOG("[DevIL] Loaded Texture successfully: %s (%dx%d, %d bpp, OpenGL ID: %u, flipX=%d, flipY=%d)",
		path.c_str(), width, height, bpp, textureID, (int)customSettings.flipX, (int)customSettings.flipY);	return result;
}


void TextureManager::CreateCheckerboard()
{
	constexpr int size = 64;
	constexpr int squares = 8; // casillas por lado
	constexpr int squareSize = size / squares;

	std::vector<unsigned char> pixels(size * size * 3);
	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			bool white = ((x / squareSize) + (y / squareSize)) % 2 == 0;
			unsigned char v = white ? 255 : 0;
			unsigned char* p = &pixels[(y * size + x) * 3];
			p[0] = p[1] = p[2] = v;
		}
	}

	unsigned int id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	GLint prevAlign = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	checkerboard = std::make_unique<TextureInfo>();
	checkerboard->id = id;
	checkerboard->width = size;
	checkerboard->height = size;
	checkerboard->path = "(procedural) checkerboard";

	LOG("[Texture] Checkerboard procedural creada (%dx%d, OpenGL ID: %u)", size, size, id);
}

void TextureManager::CreateMissingTexture()
{
	constexpr int size = 64;
	constexpr int squares = 16; 
	constexpr int squareSize = size / squares;

	std::vector<unsigned char> pixels(size * size * 3);
	for (int y = 0; y < size; ++y)
	{
		for (int x = 0; x < size; ++x)
		{
			bool purple = ((x / squareSize) + (y / squareSize)) % 2 == 0;
			unsigned char* p = &pixels[(y * size + x) * 3];
			if (purple)
			{
				p[0] = 255; p[1] = 0; p[2] = 255;
			}
			else
			{
				p[0] = p[1] = p[2] = 0;
			}
		}
	}

	unsigned int id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	GLint prevAlign = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	missingTexture = std::make_unique<TextureInfo>();
	missingTexture->id = id;
	missingTexture->width = size;
	missingTexture->height = size;
	missingTexture->path = "(procedural) missingTexture";

	LOG("[Texture] Procedural MissingTexture created (%dx%d, OpenGL ID: %u)", size, size, id);
}
