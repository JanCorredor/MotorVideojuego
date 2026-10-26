#pragma once

#include <string>

// Información estática del sistema (CPU, GPU, versiones de librerías).
// Se consulta UNA sola vez (SystemInfo::Query) tras inicializar los módulos y se cachea;
// después solo se lee por referencia constante.
struct SystemInfo
{
	// CPU
	int cpuCores = 0;
	int systemRamMB = 0;
	int cpuCacheLineSize = 0;
	bool hasAVX = false;
	bool hasAVX2 = false;
	bool hasAVX512 = false;
	bool hasSSE = false;
	bool hasSSE2 = false;
	bool hasSSE3 = false;
	bool hasSSE41 = false;
	bool hasSSE42 = false;

	// GPU
	std::string gpuVendor;
	std::string gpuRenderer;
	std::string glVersion;   // GL_VERSION tal cual lo reporta el driver
	std::string glslVersion;
	int maxTextureSize = 0;
	int maxViewportDims[2] = { 0, 0 };

	// SO y librerías
	std::string osPlatform;
	std::string sdlVersion;
	std::string devilVersion;
	std::string assimpVersion;

	// Requiere contexto OpenGL activo y DevIL inicializado
	static SystemInfo Query();
};
