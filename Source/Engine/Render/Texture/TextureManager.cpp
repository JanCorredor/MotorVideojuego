#include "TextureManager.h"
#include "../../../Utils/Log.h"

#include <IL/il.h>
#include <IL/ilu.h>
#include <glad/glad.h>
#include <vector>

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
	iluInit();

	// Todas las imagenes se cargan con la fila 0 ABAJO (convenio de OpenGL), venga del
	// formato que venga (PNG guarda las filas de arriba a abajo y TGA normalmente al reves).
	// Asi las UV de los modelos y de las primitivas se ven siempre bien orientadas.
	ilEnable(IL_ORIGIN_SET);
	ilOriginFunc(IL_ORIGIN_LOWER_LEFT);

	devilInitialized = true;
	LOG("[DevIL] Inicializada librería DevIL (Versión interna: %d)", (int)ilGetInteger(IL_VERSION_NUM));

	CreateCheckerboard();
	CreateMissingTexture();
	return true;
}

bool TextureManager::CleanUp()
{
	size_t freed = 0;

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

	LOG("[Texture] %zu texturas procedurales OpenGL liberadas", freed);

	if (devilInitialized)
	{
		ilShutDown();
		devilInitialized = false;
		LOG("[DevIL] Librería DevIL cerrada");
	}
	return true;
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
