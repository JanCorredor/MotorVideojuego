#pragma once

#include <memory>
#include <list>
#include "Module.h"
#include "../Utils/Timer.h"
#include "../Utils/PerfTimer.h"
//#include "pugixml.hpp"

// Modules
class Window;
class Input;
class Render;
class Audio;
class Scene;
class ModuleUI;

struct PerformanceData
{
	static constexpr size_t HISTORY_CAPACITY = 100;
	float fpsHistory[HISTORY_CAPACITY] = { 0.0f };
	float msHistory[HISTORY_CAPACITY] = { 0.0f };
	size_t historyIndex = 0;

	float currentFps = 0.0f;
	float currentMs = 0.0f;

	float minFps = 0.0f;
	float maxFps = 0.0f;
	float avgFps = 0.0f;

	float minMs = 0.0f;
	float maxMs = 0.0f;
	float avgMs = 0.0f;

	size_t totalFrames = 0;
	float uptimeSeconds = 0.0f;

	size_t memoryUsageMB = 0;
	size_t peakMemoryUsageMB = 0;
};

struct HardwareMetrics
{
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

	std::string gpuVendor;
	std::string gpuRenderer;
	std::string gpuVersion;
	std::string glslVersion;
	int maxTextureSize = 0;
	int maxViewportDims[2] = { 0, 0 };

	int vramTotalMB = 0;
	int vramAvailableMB = 0;

	std::string osPlatform;
	std::string sdlVersion;
	std::string openglVersion;
	std::string devilVersion;
	std::string assimpVersion;
	std::string imguiVersion;
};

class Engine
{
public:

	// Public method to get the instance of the Singleton
	static Engine& GetInstance();

	//	
	void AddModule(std::shared_ptr<Module> module);

	// Called before render is available
	bool Awake();

	// Called before the first frame
	bool Start();

	// Called each loop iteration
	bool Update();

	// Called before quitting
	bool CleanUp();

	float GetDt() const {
		return dt;
	}

	const PerformanceData& GetPerformanceData() const { return performanceData; }
	const HardwareMetrics& GetHardwareMetrics() const { return hardwareMetrics; }

	void SetTargetFrameRate(int fps) { targetFrameRate = fps; }
	int GetTargetFrameRate() const { return targetFrameRate; }

private:

	// Private constructor to prevent instantiation
	// Constructor
	Engine();

	// Delete copy constructor and assignment operator to prevent copying
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	// Call modules before each loop iteration
	void PrepareUpdate();

	// Call modules before each loop iteration
	void FinishUpdate();

	// Call modules before each loop iteration
	bool PreUpdate();

	// Call modules on each loop iteration
	bool DoUpdate();

	// Call modules after each loop iteration
	bool PostUpdate();

	// Load config file
	bool LoadConfig();

	std::list<std::shared_ptr<Module>> moduleList;

public:

	enum EngineState
	{
		CREATE = 1,
		AWAKE,
		START,
		LOOP,
		CLEAN,
		FAIL,
		EXIT
	};

	// Modules
	std::shared_ptr<Window> window;
	std::shared_ptr<Input> input;
	std::shared_ptr<Render> render;
	std::shared_ptr<Audio> audio;
	std::shared_ptr<Scene> scene;
	std::shared_ptr<ModuleUI> ui;

private:

	// Delta time
	float dt;
	//Frames since startup
	int frames;

	// Calculate timing measures
	// required variables are provided:
	Timer startupTime;
	PerfTimer frameTime;
	PerfTimer lastSecFrameTime;

	int frameCount = 0;
	int framesPerSecond = 0;
	int lastSecFrameCount = 0;

	float averageFps = 0.0f;
	int secondsSinceStartup = 0;

	//Maximun frame duration in miliseconds.
	int targetFrameRate = 60;

	std::string windowTitle = "Motor Videojuego";

	PerformanceData performanceData;
	HardwareMetrics hardwareMetrics;
	void QueryHardwareMetrics();
	void UpdateMemoryMetrics();
};

//extern ENGINE_API Engine* Engine; TODO