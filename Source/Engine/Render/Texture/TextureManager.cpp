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
	return true;
}

bool TextureManager::CleanUp()
{
	size_t freed = 0;
	for (auto& entry : cache)
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

	LOG("[Texture] %zu texturas OpenGL liberadas", freed);

	if (devilInitialized)
	{
		ilShutDown();
		devilInitialized = false;
		LOG("[DevIL] Librería DevIL cerrada");
	}
	return true;
}

void TextureManager::ApplyParameters(unsigned int textureId, bool hasMipmaps) const
{
	glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, ToGLWrap(settings.wrapMode));
	glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, ToGLWrap(settings.wrapMode));
	GLint minFilter = ToGLMinFilter(settings.minFilter);
	// Sin mipmaps un filtro mipmap dejaría la textura incompleta
	if (!hasMipmaps && minFilter == GL_LINEAR_MIPMAP_LINEAR) minFilter = GL_LINEAR;
	glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, minFilter);
	glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(settings.magFilter));
}

void TextureManager::SetSettings(const TextureSettings& newSettings)
{
	settings = newSettings;
	for (auto& entry : cache)
	{
		ApplyParameters(entry.second->id, true);
	}
	LOG("[Texture] Parámetros por defecto actualizados (min=%d, mag=%d, wrap=%d) y aplicados a %zu texturas",
		settings.minFilter, settings.magFilter, settings.wrapMode, cache.size());
}

const TextureInfo* TextureManager::Load(const std::string& path)
{
	auto it = cache.find(path);
	if (it != cache.end())
	{
		LOG("[Texture] '%s' ya estaba en caché (OpenGL ID: %u)", path.c_str(), it->second->id);
		return it->second.get();
	}

	ILuint imageID;
	ilGenImages(1, &imageID);
	ilBindImage(imageID);

	if (!ilLoadImage(path.c_str()))
	{
		ILenum err = ilGetError();
		LOG_ERROR("[DevIL] Error cargando la textura '%s' (Código de error DevIL: 0x%X)", path.c_str(), (unsigned int)err);
		ilDeleteImages(1, &imageID);
		return nullptr;
	}

	ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

	int width = ilGetInteger(IL_IMAGE_WIDTH);
	int height = ilGetInteger(IL_IMAGE_HEIGHT);
	int bpp = ilGetInteger(IL_IMAGE_BPP);

	unsigned int textureID = 0;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ilGetData());
	glGenerateMipmap(GL_TEXTURE_2D);
	ApplyParameters(textureID, true);
	glBindTexture(GL_TEXTURE_2D, 0);

	ilDeleteImages(1, &imageID);

	auto info = std::make_unique<TextureInfo>();
	info->id = textureID;
	info->width = width;
	info->height = height;
	info->path = path;
	const TextureInfo* result = info.get();
	cache[path] = std::move(info);

	LOG("[DevIL] Textura cargada con éxito: %s (%dx%d, %d bpp, OpenGL ID: %u)", path.c_str(), width, height, bpp, textureID);
	return result;
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
