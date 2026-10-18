#include "SystemInfo.h"
#include "../../Utils/Log.h"

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <IL/il.h>
#include <assimp/version.h>

SystemInfo SystemInfo::Query()
{
	SystemInfo info;

	// CPU
	info.cpuCores = SDL_GetNumLogicalCPUCores();
	info.systemRamMB = SDL_GetSystemRAM();
	info.cpuCacheLineSize = SDL_GetCPUCacheLineSize();
	info.hasAVX = SDL_HasAVX();
	info.hasAVX2 = SDL_HasAVX2();
	info.hasAVX512 = SDL_HasAVX512F();
	info.hasSSE = SDL_HasSSE();
	info.hasSSE2 = SDL_HasSSE2();
	info.hasSSE3 = SDL_HasSSE3();
	info.hasSSE41 = SDL_HasSSE41();
	info.hasSSE42 = SDL_HasSSE42();

	// GPU
	auto glString = [](GLenum name) -> std::string {
		const char* s = (const char*)glGetString(name);
		return s ? s : "Desconocido";
	};
	info.gpuVendor = glString(GL_VENDOR);
	info.gpuRenderer = glString(GL_RENDERER);
	info.glVersion = glString(GL_VERSION);
	info.glslVersion = glString(GL_SHADING_LANGUAGE_VERSION);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &info.maxTextureSize);
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, info.maxViewportDims);

	// SO y librerías
	const char* platform = SDL_GetPlatform();
	info.osPlatform = platform ? platform : "Desconocido";

	int sdlVer = SDL_GetVersion();
	info.sdlVersion = std::to_string(SDL_VERSIONNUM_MAJOR(sdlVer)) + "." +
		std::to_string(SDL_VERSIONNUM_MINOR(sdlVer)) + "." +
		std::to_string(SDL_VERSIONNUM_MICRO(sdlVer));

	// IL_VERSION_NUM = MMm p (p.ej. 180 -> 1.8.0)
	int ilVer = ilGetInteger(IL_VERSION_NUM);
	info.devilVersion = std::to_string(ilVer / 100) + "." + std::to_string((ilVer / 10) % 10) + "." + std::to_string(ilVer % 10);

	info.assimpVersion = std::to_string(aiGetVersionMajor()) + "." +
		std::to_string(aiGetVersionMinor()) + "." +
		std::to_string(aiGetVersionRevision());

	LOG("[System] CPU: %d núcleos lógicos, %d MB RAM | GPU: %s (%s) | OpenGL %s",
		info.cpuCores, info.systemRamMB, info.gpuRenderer.c_str(), info.gpuVendor.c_str(), info.glVersion.c_str());
	LOG("[System] Librerías: SDL3 %s, DevIL %s, Assimp %s", info.sdlVersion.c_str(), info.devilVersion.c_str(), info.assimpVersion.c_str());

	return info;
}
