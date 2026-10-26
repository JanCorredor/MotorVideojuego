#pragma once

#include <cstddef>

struct PerformanceData
{
	static constexpr size_t HISTORY_CAPACITY = 100;
	float fpsHistory[HISTORY_CAPACITY] = { 0.0f };
	float msHistory[HISTORY_CAPACITY] = { 0.0f };
	size_t historyIndex = 0;   // siguiente posición a escribir (offset para PlotLines)
	size_t historyCount = 0;   // muestras válidas (<= HISTORY_CAPACITY)

	float currentFps = 0.0f;
	float currentMs = 0.0f;

	float minFps = 0.0f;
	float maxFps = 0.0f;
	float avgFps = 0.0f;   // media de la ventana del historial

	float minMs = 0.0f;
	float maxMs = 0.0f;
	float avgMs = 0.0f;

	// Máximos de la ventana del historial (para escalar los gráficos automáticamente)
	float historyMaxFps = 0.0f;
	float historyMaxMs = 0.0f;

	size_t totalFrames = 0;
	float uptimeSeconds = 0.0f;

	// Memoria RAM
	size_t processMemoryMB = 0;
	size_t processPeakMemoryMB = 0;
	size_t systemRamTotalMB = 0;
	size_t systemRamUsedMB = 0;

	// Memoria de vídeo (vramSource == "N/D" si no hay forma de consultarla)
	const char* vramSource = "N/D";
	int vramTotalMB = 0;       // total dedicada
	int vramAvailableMB = 0;   // libre / disponible según presupuesto
	int vramProcessMB = 0;     // usada por este proceso (solo DXGI)
};

// Monitoriza rendimiento y memoria. El historial es un ring buffer de tamaño fijo:
// no hay ninguna asignación dinámica por frame.
class PerformanceMonitor
{
public:
	// Requiere contexto OpenGL activo
	void Init();
	void CleanUp();

	void Sample(float dtMs, size_t totalFrames, float uptimeSeconds);
	void SampleMemory();
	void ResetMinMax();

	const PerformanceData& Get() const { return data; }

private:
	void QueryVram();
	void InitDxgi();

	PerformanceData data;
	bool hasNvxMemoryInfo = false;
	bool dxgiReady = false;
	void* dxgiAdapter = nullptr; // IDXGIAdapter3*
};
