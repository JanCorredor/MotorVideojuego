#include "PerformanceMonitor.h"
#include "../../Utils/Log.h"

#include <glad/glad.h>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")
#endif

void PerformanceMonitor::Init()
{
	// La extensión NVX solo se consulta si el driver la anuncia (evita GL_INVALID_ENUM)
	GLint numExt = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
	for (GLint i = 0; i < numExt; ++i)
	{
		const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
		if (ext && std::strcmp(ext, "GL_NVX_gpu_memory_info") == 0)
		{
			hasNvxMemoryInfo = true;
			break;
		}
	}

	InitDxgi();

	if (hasNvxMemoryInfo) data.vramSource = "NVX_gpu_memory_info";
	else if (dxgiReady)   data.vramSource = "DXGI";
	else                  data.vramSource = "N/D";
	LOG("[Monitor] Fuente de VRAM: %s", data.vramSource);

	SampleMemory();
}

void PerformanceMonitor::CleanUp()
{
#ifdef _WIN32
	if (dxgiAdapter)
	{
		static_cast<IDXGIAdapter3*>(dxgiAdapter)->Release();
		dxgiAdapter = nullptr;
	}
#endif
	dxgiReady = false;
}

void PerformanceMonitor::InitDxgi()
{
#ifdef _WIN32
	IDXGIFactory4* factory = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory)) || !factory) return;

	const char* glRenderer = (const char*)glGetString(GL_RENDERER);
	std::string renderer = glRenderer ? glRenderer : "";

	IDXGIAdapter3* best = nullptr;
	SIZE_T bestMem = 0;
	bool matchedByName = false;

	IDXGIAdapter1* adapter = nullptr;
	for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		IDXGIAdapter3* adapter3 = nullptr;
		bool ok = !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
			SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&adapter3)) && adapter3;
		adapter->Release();
		if (!ok) continue;

		// Preferimos el adaptador cuyo nombre aparece en GL_RENDERER (portátiles híbridos)
		char name[256] = { 0 };
		WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
		bool nameMatch = !renderer.empty() && renderer.find(name) != std::string::npos;

		if (nameMatch && !matchedByName)
		{
			if (best) best->Release();
			best = adapter3; bestMem = desc.DedicatedVideoMemory; matchedByName = true;
		}
		else if (!matchedByName && desc.DedicatedVideoMemory >= bestMem)
		{
			if (best) best->Release();
			best = adapter3; bestMem = desc.DedicatedVideoMemory;
		}
		else
		{
			adapter3->Release();
		}
	}
	factory->Release();

	if (best)
	{
		dxgiAdapter = best;
		dxgiReady = true;
	}
#endif
}

void PerformanceMonitor::Sample(float dtMs, size_t totalFrames, float uptimeSeconds)
{
	data.totalFrames = totalFrames;
	data.uptimeSeconds = uptimeSeconds;
	data.currentMs = dtMs;
	data.currentFps = (dtMs > 0.0f) ? (1000.0f / dtMs) : 0.0f;

	// Ring buffer de tamaño fijo
	data.fpsHistory[data.historyIndex] = data.currentFps;
	data.msHistory[data.historyIndex] = data.currentMs;
	data.historyIndex = (data.historyIndex + 1) % PerformanceData::HISTORY_CAPACITY;
	if (data.historyCount < PerformanceData::HISTORY_CAPACITY) data.historyCount++;

	// Media y máximos de la ventana (100 elementos: coste despreciable)
	float sumFps = 0.0f, sumMs = 0.0f, maxFps = 0.0f, maxMs = 0.0f;
	for (size_t i = 0; i < data.historyCount; ++i)
	{
		sumFps += data.fpsHistory[i];
		sumMs += data.msHistory[i];
		maxFps = std::max(maxFps, data.fpsHistory[i]);
		maxMs = std::max(maxMs, data.msHistory[i]);
	}
	const float n = (float)std::max<size_t>(data.historyCount, 1);
	data.avgFps = sumFps / n;
	data.avgMs = sumMs / n;
	data.historyMaxFps = maxFps;
	data.historyMaxMs = maxMs;

	// Mínimo/máximo de sesión (se ignoran los primeros frames de calentamiento)
	if (totalFrames > 10)
	{
		if (data.minFps == 0.0f || (data.currentFps > 0.0f && data.currentFps < data.minFps)) data.minFps = data.currentFps;
		if (data.currentFps > data.maxFps) data.maxFps = data.currentFps;
		if (data.minMs == 0.0f || (data.currentMs > 0.0f && data.currentMs < data.minMs)) data.minMs = data.currentMs;
		if (data.currentMs > data.maxMs) data.maxMs = data.currentMs;
	}

	// La memoria es costosa de consultar: cada 30 frames
	if (totalFrames % 30 == 0)
	{
		SampleMemory();
	}
}

void PerformanceMonitor::ResetMinMax()
{
	data.minFps = data.maxFps = 0.0f;
	data.minMs = data.maxMs = 0.0f;
}

void PerformanceMonitor::SampleMemory()
{
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
	{
		data.processMemoryMB = (size_t)(pmc.WorkingSetSize / (1024 * 1024));
		data.processPeakMemoryMB = (size_t)(pmc.PeakWorkingSetSize / (1024 * 1024));
	}

	MEMORYSTATUSEX mem;
	mem.dwLength = sizeof(mem);
	if (GlobalMemoryStatusEx(&mem))
	{
		data.systemRamTotalMB = (size_t)(mem.ullTotalPhys / (1024 * 1024));
		data.systemRamUsedMB = (size_t)((mem.ullTotalPhys - mem.ullAvailPhys) / (1024 * 1024));
	}
#endif
	QueryVram();
}

void PerformanceMonitor::QueryVram()
{
	if (hasNvxMemoryInfo)
	{
		GLint totalKb = 0, availKb = 0;
		glGetIntegerv(0x9048, &totalKb); // GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
		glGetIntegerv(0x9049, &availKb); // GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
		data.vramTotalMB = totalKb / 1024;
		data.vramAvailableMB = availKb / 1024;
		data.vramProcessMB = std::max(0, data.vramTotalMB - data.vramAvailableMB); // aproximación: uso total del dispositivo
	}

#ifdef _WIN32
	if (dxgiReady && dxgiAdapter)
	{
		IDXGIAdapter3* adapter = static_cast<IDXGIAdapter3*>(dxgiAdapter);
		DXGI_QUERY_VIDEO_MEMORY_INFO local = {};
		if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local)))
		{
			if (!hasNvxMemoryInfo)
			{
				DXGI_ADAPTER_DESC1 desc;
				static_cast<IDXGIAdapter1*>(adapter)->GetDesc1(&desc);
				data.vramTotalMB = (int)(desc.DedicatedVideoMemory / (1024 * 1024));
				data.vramAvailableMB = (int)(((local.Budget > local.CurrentUsage) ? (local.Budget - local.CurrentUsage) : 0) / (1024 * 1024));
				data.vramProcessMB = (int)(local.CurrentUsage / (1024 * 1024));
			}
		}
	}
#endif
}
