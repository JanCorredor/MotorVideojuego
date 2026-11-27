#include "TextureImporter.h"
#include "../../FileSystem/FileSystem.h"
#include "../../../Utils/Log.h"

#include <IL/il.h>
#include <IL/ilu.h>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
	constexpr char TEX_MAGIC[4] = { 'M', 'T', 'E', 'X' };
	constexpr uint32_t TEX_VERSION = 1;

	// Cabecera del formato .tex (todo campos de 4 bytes: sin relleno entre ellos)
	struct TexFileHeader
	{
		char magic[4];
		uint32_t version;
		uint32_t width;
		uint32_t height;
		uint32_t channels;
		uint32_t minFilter;
		uint32_t magFilter;
		uint32_t wrapMode;
	};
	static_assert(sizeof(TexFileHeader) == 32, "La cabecera .tex debe ocupar 32 bytes");

	// Tipo de imagen de DevIL segun la extension (para cargar desde memoria)
	ILenum ExtensionToILType(const std::string& extension)
	{
		if (extension == ".png") return IL_PNG;
		if (extension == ".jpg" || extension == ".jpeg") return IL_JPG;
		if (extension == ".tga") return IL_TGA;
		if (extension == ".dds") return IL_DDS;
		if (extension == ".bmp") return IL_BMP;
		return IL_TYPE_UNKNOWN;
	}

	GLint ToGLMinFilter(uint32_t filter)
	{
		switch (filter)
		{
		case TextureImportSettings::FILTER_NEAREST: return GL_NEAREST;
		case TextureImportSettings::FILTER_LINEAR:  return GL_LINEAR;
		default:                                    return GL_LINEAR_MIPMAP_LINEAR;
		}
	}

	GLint ToGLMagFilter(uint32_t filter)
	{
		return (filter == TextureImportSettings::FILTER_NEAREST) ? GL_NEAREST : GL_LINEAR;
	}

	GLint ToGLWrap(uint32_t wrap)
	{
		switch (wrap)
		{
		case TextureImportSettings::WRAP_CLAMP:    return GL_CLAMP_TO_EDGE;
		case TextureImportSettings::WRAP_MIRRORED: return GL_MIRRORED_REPEAT;
		default:                                   return GL_REPEAT;
		}
	}
}

bool TextureImporter::IsSupportedExtension(const std::string& extension)
{
	const std::string ext = FileSystem::ToLower(extension);
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".bmp";
}

bool TextureImporter::Import(const std::string& assetPath, const TextureImportSettings& settings, const std::string& libraryPath)
{
	// 1) Leemos el fichero original a memoria (asi no dependemos de como DevIL interpreta las rutas)
	std::vector<char> fileData;
	if (!FileSystem::ReadBinary(assetPath, fileData) || fileData.empty())
	{
		LOG_ERROR("[TextureImporter] No se pudo leer '%s'", assetPath.c_str());
		return false;
	}

	ILuint image = 0;
	ilGenImages(1, &image);
	ilBindImage(image);

	// 2) DevIL decodifica la imagen (PNG, JPG, TGA, DDS...)
	const ILenum type = ExtensionToILType(FileSystem::GetExtensionLower(assetPath));
	ILboolean loaded = ilLoadL(type, fileData.data(), static_cast<ILuint>(fileData.size()));
	if (!loaded)
	{
		// Ultimo intento: dejar que DevIL abra el fichero por su cuenta
		const std::string diskPath = FileSystem::ToDiskPath(assetPath);
		loaded = ilLoadImage(diskPath.c_str());
	}
	if (!loaded)
	{
		LOG_ERROR("[TextureImporter] DevIL no pudo decodificar '%s' (error DevIL 0x%X)", assetPath.c_str(), (unsigned int)ilGetError());
		ilDeleteImages(1, &image);
		return false;
	}

	// 3) Guardamos RGB o RGBA segun si la imagen original tiene canal alfa
	const ILint format = ilGetInteger(IL_IMAGE_FORMAT);
	bool hasAlpha = (format == IL_RGBA || format == IL_BGRA || format == IL_LUMINANCE_ALPHA);
	if (format == IL_COLOUR_INDEX)
	{
		const ILint palette = ilGetInteger(IL_PALETTE_TYPE);
		hasAlpha = (palette == IL_PAL_RGBA32 || palette == IL_PAL_BGRA32);
	}
	const int channels = hasAlpha ? 4 : 3;

	if (!ilConvertImage(hasAlpha ? IL_RGBA : IL_RGB, IL_UNSIGNED_BYTE))
	{
		LOG_ERROR("[TextureImporter] No se pudo convertir '%s' a %s", assetPath.c_str(), hasAlpha ? "RGBA" : "RGB");
		ilDeleteImages(1, &image);
		return false;
	}

	int width = ilGetInteger(IL_IMAGE_WIDTH);
	int height = ilGetInteger(IL_IMAGE_HEIGHT);
	const int originalWidth = width;
	const int originalHeight = height;

	// 4) Opcion "Max Size": las imagenes enormes se reducen (manteniendo la proporcion)
	const int largestSide = std::max(width, height);
	if (settings.maxSize > 0 && largestSide > settings.maxSize)
	{
		const double factor = static_cast<double>(settings.maxSize) / static_cast<double>(largestSide);
		const int newWidth = std::max(1, static_cast<int>(std::lround(width * factor)));
		const int newHeight = std::max(1, static_cast<int>(std::lround(height * factor)));

		iluImageParameter(ILU_FILTER, ILU_BILINEAR);
		if (iluScale(static_cast<ILuint>(newWidth), static_cast<ILuint>(newHeight), 1))
		{
			width = ilGetInteger(IL_IMAGE_WIDTH);
			height = ilGetInteger(IL_IMAGE_HEIGHT);
		}
		else
		{
			LOG_WARNING("[TextureImporter] No se pudo reducir '%s' a %dx%d", assetPath.c_str(), newWidth, newHeight);
		}
	}

	// 5) Opciones de volteo
	if (settings.flipX) iluMirror();
	if (settings.flipY) iluFlipImage();

	const ILubyte* pixels = ilGetData();
	const size_t pixelBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
	const size_t devilBytes = static_cast<size_t>(ilGetInteger(IL_IMAGE_SIZE_OF_DATA));
	if (pixels == nullptr || width <= 0 || height <= 0 || devilBytes < pixelBytes)
	{
		LOG_ERROR("[TextureImporter] Datos de imagen invalidos en '%s'", assetPath.c_str());
		ilDeleteImages(1, &image);
		return false;
	}

	// 6) Escribimos nuestro formato: cabecera + pixeles
	TexFileHeader header{};
	std::memcpy(header.magic, TEX_MAGIC, sizeof(TEX_MAGIC));
	header.version = TEX_VERSION;
	header.width = static_cast<uint32_t>(width);
	header.height = static_cast<uint32_t>(height);
	header.channels = static_cast<uint32_t>(channels);
	header.minFilter = static_cast<uint32_t>(settings.minFilter);
	header.magFilter = static_cast<uint32_t>(settings.magFilter);
	header.wrapMode = static_cast<uint32_t>(settings.wrapMode);

	std::vector<char> output(sizeof(TexFileHeader) + pixelBytes);
	std::memcpy(output.data(), &header, sizeof(TexFileHeader));
	std::memcpy(output.data() + sizeof(TexFileHeader), pixels, pixelBytes);

	ilDeleteImages(1, &image);
	ilBindImage(0);

	if (!FileSystem::WriteBinary(libraryPath, output.data(), output.size()))
	{
		LOG_ERROR("[TextureImporter] No se pudo escribir '%s'", libraryPath.c_str());
		return false;
	}

	if (width != originalWidth || height != originalHeight)
	{
		LOG("[TextureImporter] '%s' importada como %s (%dx%d -> %dx%d, %d canales)",
			assetPath.c_str(), libraryPath.c_str(), originalWidth, originalHeight, width, height, channels);
	}
	else
	{
		LOG("[TextureImporter] '%s' importada como %s (%dx%d, %d canales)",
			assetPath.c_str(), libraryPath.c_str(), width, height, channels);
	}
	return true;
}

bool TextureImporter::LoadToGPU(const std::string& libraryPath, GPUTexture& outTexture)
{
	std::vector<char> data;
	if (!FileSystem::ReadBinary(libraryPath, data) || data.size() < sizeof(TexFileHeader))
	{
		LOG_ERROR("[TextureImporter] No se pudo leer '%s'", libraryPath.c_str());
		return false;
	}

	TexFileHeader header{};
	std::memcpy(&header, data.data(), sizeof(TexFileHeader));

	// Validaciones para no leer basura si el fichero esta corrupto o es de otra version
	if (std::memcmp(header.magic, TEX_MAGIC, sizeof(TEX_MAGIC)) != 0 || header.version != TEX_VERSION)
	{
		LOG_ERROR("[TextureImporter] '%s' no es un fichero .tex valido", libraryPath.c_str());
		return false;
	}
	if ((header.channels != 3 && header.channels != 4) || header.width == 0 || header.height == 0)
	{
		LOG_ERROR("[TextureImporter] Cabecera invalida en '%s'", libraryPath.c_str());
		return false;
	}
	const size_t pixelBytes = static_cast<size_t>(header.width) * header.height * header.channels;
	if (data.size() < sizeof(TexFileHeader) + pixelBytes)
	{
		LOG_ERROR("[TextureImporter] '%s' esta incompleto", libraryPath.c_str());
		return false;
	}

	const bool useMipmaps = (header.minFilter == TextureImportSettings::FILTER_TRILINEAR);
	const GLenum pixelFormat = (header.channels == 4) ? GL_RGBA : GL_RGB;
	const GLint internalFormat = (header.channels == 4) ? GL_RGBA8 : GL_RGB8;

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);

	// Las filas RGB no siempre miden un multiplo de 4 bytes: alineacion 1 para leerlas bien
	GLint previousAlignment = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, static_cast<GLsizei>(header.width), static_cast<GLsizei>(header.height),
		0, pixelFormat, GL_UNSIGNED_BYTE, data.data() + sizeof(TexFileHeader));
	glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

	if (useMipmaps)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGLWrap(header.wrapMode));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGLWrap(header.wrapMode));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(header.minFilter));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ToGLMagFilter(header.magFilter));
	glBindTexture(GL_TEXTURE_2D, 0);

	outTexture.id = id;
	outTexture.width = static_cast<int>(header.width);
	outTexture.height = static_cast<int>(header.height);
	outTexture.channels = static_cast<int>(header.channels);
	// Aproximacion: la GPU suele guardar 4 bytes por pixel; los mipmaps suman 1/3 mas
	const size_t gpuBytes = static_cast<size_t>(header.width) * header.height * 4;
	outTexture.bytes = useMipmaps ? gpuBytes + gpuBytes / 3 : gpuBytes;
	return true;
}
