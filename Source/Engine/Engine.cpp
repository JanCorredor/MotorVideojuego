#include <iostream>

#include "Engine.h"
#include "Window/Window.h"
#include "Input/Input.h"
#include "Render/Render.h"
#include "Audio/Audio.h"
#include "Render/Texture/TextureManager.h"
#include "Scene/Scene.h"
#include "UI/ModuleUI.h"
#include "System/SystemInfo.h"
#include "../Utils/Log.h"
#include "../Utils/Timer.h"
#include "../Utils/PerfTimer.h"

#include <tracy/Tracy.hpp>
#include <glad/glad.h>
#include <IL/il.h>
#include <assimp/version.h>
#include <imgui.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

// Constructor
Engine::Engine() 
{
    LOG("Constructor Engine::Engine");

    // Measure the amount of ms that takes to execute the Engine constructor and LOG the result
	Timer timer = Timer();
    startupTime = Timer();
    frameTime = PerfTimer();
    lastSecFrameTime = PerfTimer();
    frames = 0;

    
    // Modules
    window = std::make_shared<Window>();
    input = std::make_shared<Input>();
    render = std::make_shared<Render>();
    audio = std::make_shared<Audio>();
    textures = std::make_shared<TextureManager>();
    scene = std::make_shared<Scene>();
    ui = std::make_shared<ModuleUI>();


    // Ordered for awake / Start / Update
    // Reverse order of CleanUp
    AddModule(std::static_pointer_cast<Module>(window));
    AddModule(std::static_pointer_cast<Module>(input));
    AddModule(std::static_pointer_cast<Module>(audio));
    AddModule(std::static_pointer_cast<Module>(textures));
    AddModule(std::static_pointer_cast<Module>(scene));

    // UI module
    AddModule(std::static_pointer_cast<Module>(ui));

    // Render last 
    AddModule(std::static_pointer_cast<Module>(render));

    // Log the result of the timer
	LOG("Timer App Constructor: %f", timer.ReadMSec());
}

// Static method to get the instance of the Engine class, following the singleton pattern
Engine& Engine::GetInstance() {
    static Engine instance; // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

void Engine::AddModule(std::shared_ptr<Module> module){
    module->Init();
    moduleList.push_back(module);
}

// Called before render is available
bool Engine::Awake() {

    // Measure the amount of ms that takes to execute the Awake and LOG the result
    Timer timer = Timer();

    LOG("Engine::Awake");

    // Add the LoadConfig() method here
    LoadConfig();
    
    // Read the title from the config file and set the variable gameTitle, read targetFrameRate and set the variables
    //gameTitle = configFile.child("config").child("engine").child("title").child_value();
    //targetFrameRate = configFile.child("config").child("engine").child("targetFrameRate").attribute("value").as_int();

    //Iterates the module list and calls Awake on each module
    bool result = true;
    for (const auto& module : moduleList) 
    {
        LOG("[Engine] Inicializando módulo: %s (Awake)", module->name.c_str());
        result = module->Awake();

        if (!result) {
            LOG_ERROR("[Engine] Falló la inicialización (Awake) del módulo: %s", module->name.c_str());
            break;
        }
    }

    // Log the result of the timer
    LOG("Timer App Awake(): %f ms", timer.ReadMSec());

    return result;

}

// Called before the first frame
bool Engine::Start() {

    // Measure the amount of ms that takes to execute the Start() and LOG the result
    Timer timer = Timer();

    LOG("Engine::Start");

    // Iterates the module list and calls Start on each module
    bool result = true;
    for (const auto& module : moduleList) {
        LOG("[Engine] Arrancando módulo: %s (Start)", module->name.c_str());
        result = module->Start();
        if (!result) {
            LOG_ERROR("[Engine] Falló el arranque (Start) del módulo: %s", module->name.c_str());
            break;
        }
    }

    // Log the result of the timer
    LOG("Timer App Start(): %f ms", timer.ReadMSec());
	
    QueryHardwareMetrics();
    UpdateMemoryMetrics();

    return result;
}

// Called each loop iteration
bool Engine::Update() 
{
#ifdef _DEBUG
    ZoneScoped;
#endif
    bool ret = true;
    PrepareUpdate();

    if (input->GetWindowEvent(WE_QUIT) == true)
        ret = false;

    if (ret == true)
        ret = PreUpdate();

    if (ret == true)
        ret = DoUpdate();

    if (ret == true)
        ret = PostUpdate();

    FinishUpdate();
    return ret;
}

// Called before quitting
bool Engine::CleanUp() {

    // Measure the amount of ms that takes to execute the Start() and LOG the result
    Timer timer = Timer();

    LOG("Engine::CleanUp");

    // Iterates the module list in reverse order and calls CleanUp on each module
    bool result = true;
    for (auto it = moduleList.rbegin(); it != moduleList.rend(); ++it) {
        LOG("[Engine] Limpiando módulo: %s (CleanUp)", (*it)->name.c_str());
        if (!(*it)->CleanUp()) {
            LOG_ERROR("[Engine] Falló la limpieza (CleanUp) del módulo: %s", (*it)->name.c_str());
            result = false;
        }
    }

    // Log the result of the timer
    LOG("Timer App CleanUp(): %f ms", timer.ReadMSec());

    return result;
}

// ---------------------------------------------
void Engine::PrepareUpdate()
{
#ifdef _DEBUG
    ZoneScoped;
#endif
    frameTime.Start();
}

// ---------------------------------------------
void Engine::FinishUpdate()
{
#ifdef _DEBUG
    ZoneScoped;
#endif
    //  Cap the framerate of the gameloop
    double currentDt = frameTime.ReadMs();
	float maxFrameDuration = 1000.0f / targetFrameRate;
    if (targetFrameRate > 0 && currentDt < maxFrameDuration) {
        Uint32 delay = (Uint32)(maxFrameDuration - currentDt);

        // Measure accurately the amount of time SDL_Delay() actually waits compared to what was expected
        PerfTimer delayTimer = PerfTimer();
        SDL_Delay(delay);
        //LOG("We waited for %I32u ms and got back in %f ms",delay,delayTimer.ReadMs()); // Uncomment this line to see the results
    }

	// Calculate:
	
    // Amount of frames since startup
    frameCount++;

    // Amount of time since game start (use a low resolution timer)
    secondsSinceStartup = startupTime.ReadSec();

    // Amount of ms took the last update (dt)
    dt = (float)frameTime.ReadMs();

    // Amount of frames during the last second
    lastSecFrameCount++;

    // Average FPS for the whole game life
    if (lastSecFrameTime.ReadMs() > 1000) {
        lastSecFrameTime.Start();
        averageFps = (averageFps + lastSecFrameCount) / 2;
        framesPerSecond = lastSecFrameCount;
        lastSecFrameCount = 0;
    }

    // Update performance data for editor telemetry & monitoring
    performanceData.totalFrames = frameCount;
    performanceData.uptimeSeconds = (float)secondsSinceStartup;
    performanceData.currentMs = dt;
    performanceData.currentFps = (dt > 0.0f) ? (1000.0f / dt) : 0.0f;

    // Push into circular buffer
    performanceData.fpsHistory[performanceData.historyIndex] = performanceData.currentFps;
    performanceData.msHistory[performanceData.historyIndex] = performanceData.currentMs;
    performanceData.historyIndex = (performanceData.historyIndex + 1) % PerformanceData::HISTORY_CAPACITY;

    // Calculate min, max, avg (skipping startup warmup frames)
    if (frameCount > 10)
    {
        if (performanceData.minFps == 0.0f || (performanceData.currentFps > 0.0f && performanceData.currentFps < performanceData.minFps))
            performanceData.minFps = performanceData.currentFps;
        if (performanceData.currentFps > performanceData.maxFps)
            performanceData.maxFps = performanceData.currentFps;

        if (performanceData.minMs == 0.0f || (performanceData.currentMs > 0.0f && performanceData.currentMs < performanceData.minMs))
            performanceData.minMs = performanceData.currentMs;
        if (performanceData.currentMs > performanceData.maxMs)
            performanceData.maxMs = performanceData.currentMs;
    }
    performanceData.avgFps = averageFps;
    performanceData.avgMs = (averageFps > 0.0f) ? (1000.0f / averageFps) : 0.0f;

    // Periodically update process memory (every 60 frames to minimize OS call overhead)
    if (frameCount % 60 == 0)
    {
        UpdateMemoryMetrics();
    }
}


// Call modules before each loop iteration
bool Engine::PreUpdate()
{
#ifdef _DEBUG
    ZoneScoped;
#endif
    //Iterates the module list and calls PreUpdate on each module
    bool result = true;
    for (const auto& module : moduleList) {
        result = module->PreUpdate();
        if (!result) {
            break;
        }
    }

    return result;
}

// Call modules on each loop iteration
bool Engine::DoUpdate()
{
#ifdef _DEBUG
    ZoneScoped;
#endif
    //Iterates the module list and calls Update on each module
    bool result = true;
    for (const auto& module : moduleList) {
        result = module->Update(dt);
        if (!result) {
            break;
        }
    }

    return result;
}

// Call modules after each loop iteration
bool Engine::PostUpdate()
{
#ifdef _DEBUG
    ZoneScoped;
#endif
    //Iterates the module list and calls PostUpdate on each module
    bool result = true;
    for (const auto& module : moduleList) {
        result = module->PostUpdate();
        if (!result) {
            break;
        }
    }

    return result;
}

// Load config from XML file
bool Engine::LoadConfig()
{
    bool ret = true;

    // Load config.xml file using load_file() method from the xml_document class
    // If the result is ok get the main node of the XML
    // else, log the error
    // check https://pugixml.org/docs/quickstart.html#loading

    //pugi::xml_parse_result result = configFile.load_file("config.xml");
    //if (result)
    //{
    //    LOG("config.xml parsed without errors");
    //}
    //else
    //{
    //    LOG("Error loading config.xml: %s", result.description());
    //}

    return ret;
}

void Engine::QueryHardwareMetrics()
{
	SystemInfo sys = SystemInfo::Query();

	hardwareMetrics.cpuCores = sys.cpuCores;
	hardwareMetrics.systemRamMB = sys.systemRamMB;
	hardwareMetrics.cpuCacheLineSize = sys.cpuCacheLineSize;
	hardwareMetrics.hasAVX = sys.hasAVX;
	hardwareMetrics.hasAVX2 = sys.hasAVX2;
	hardwareMetrics.hasAVX512 = sys.hasAVX512;
	hardwareMetrics.hasSSE = sys.hasSSE;
	hardwareMetrics.hasSSE2 = sys.hasSSE2;
	hardwareMetrics.hasSSE3 = sys.hasSSE3;
	hardwareMetrics.hasSSE41 = sys.hasSSE41;
	hardwareMetrics.hasSSE42 = sys.hasSSE42;

	hardwareMetrics.gpuVendor = sys.gpuVendor;
	hardwareMetrics.gpuRenderer = sys.gpuRenderer;
	hardwareMetrics.gpuVersion = sys.glVersion;
	hardwareMetrics.glslVersion = sys.glslVersion;
	hardwareMetrics.maxTextureSize = sys.maxTextureSize;
	hardwareMetrics.maxViewportDims[0] = sys.maxViewportDims[0];
	hardwareMetrics.maxViewportDims[1] = sys.maxViewportDims[1];

	hardwareMetrics.osPlatform = sys.osPlatform;
	hardwareMetrics.sdlVersion = sys.sdlVersion;
	hardwareMetrics.openglVersion = sys.glVersion;
	hardwareMetrics.devilVersion = sys.devilVersion;
	hardwareMetrics.assimpVersion = sys.assimpVersion;
	hardwareMetrics.imguiVersion = IMGUI_VERSION;
}

void Engine::UpdateMemoryMetrics()
{
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
	{
		performanceData.memoryUsageMB = (size_t)(pmc.WorkingSetSize / (1024 * 1024));
		performanceData.peakMemoryUsageMB = (size_t)(pmc.PeakWorkingSetSize / (1024 * 1024));
	}
#endif

	// Dedicated VRAM query (NVIDIA NVX / AMD ATI extension)
	GLint totalMemKb = 0;
	GLint curAvailMemKb = 0;
	if (glGetString != nullptr && glGetString(GL_VERSION) != nullptr)
	{
		// GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX 0x9048
		glGetIntegerv(0x9048, &totalMemKb);
		// GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
		glGetIntegerv(0x9049, &curAvailMemKb);
		// Limpiar posible GL_INVALID_ENUM en GPUs Intel/AMD sin extensión NVX
		while (glGetError() != GL_NO_ERROR) {}

		if (totalMemKb > 0)
		{
			hardwareMetrics.vramTotalMB = totalMemKb / 1024;
			hardwareMetrics.vramAvailableMB = curAvailMemKb / 1024;
		}
		else
		{
			// AMD ATI extension: GL_VBO_FREE_MEMORY_ATI 0x87FB
			GLint atiMem[4] = { 0 };
			glGetIntegerv(0x87FB, atiMem);
			while (glGetError() != GL_NO_ERROR) {}
			if (atiMem[0] > 0)
			{
				hardwareMetrics.vramAvailableMB = atiMem[0] / 1024;
				if (hardwareMetrics.vramTotalMB == 0)
					hardwareMetrics.vramTotalMB = hardwareMetrics.vramAvailableMB;
			}
		}
	}
}


