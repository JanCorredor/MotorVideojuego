#pragma once

#include "../Module.h"
#include "../../Utils/Log.h"
#include "../Scene/GameObjects/Components/Component.h"
#include <cstdint>
#include <vector>
#include <memory>

class GameObject;

class ModuleUI : public Module
{
public:
	ModuleUI();
	virtual ~ModuleUI();

	bool Awake() override;
	bool Start() override;
	bool PreUpdate() override;
	bool Update(float dt) override;
	bool PostUpdate() override;
	bool CleanUp() override;

	void ResetLayout() { resetLayout = true; }

private:
	void RenderUI();
	bool DrawMainMenuBar();
	void DrawScenePanel();
	void DrawGamePanel();
	void DrawHierarchyPanel();
	bool DrawHierarchyNode(GameObject* node, GameObject* selected, bool hasChildren);
	void DrawInspectorPanel();
	void DrawConsolePanel();
	void DrawConfigurationPanel();
	void DrawAboutPanel();
	void DrawDepthBufferPanel();

	bool resetLayout = false;

	// Visibilidad de ventanas del Editor
	bool showScene = true;
	bool showGame = true;
	bool showHierarchy = true;
	bool showInspector = true;
	bool showConsole = true;
	bool showConfiguration = true;
	bool showAbout = false;

	// Foco para atajos de teclado (Supr / Delete)
	bool sceneOrHierarchyFocused = false;

	// Estado del panel Console
	struct ConsoleDisplayItem
	{
		int logIndex = -1;
		int collapsedCount = 1;
	};
	std::vector<ConsoleDisplayItem> consoleViewCache;
	uint64_t lastConsoleRevision = UINT64_MAX;
	bool consoleFilterDirty = true;
	bool scrollToBottom = false;

	bool showInfo = true;
	bool showWarnings = true;
	bool showErrors = true;
	bool collapseLogs = false;
	bool autoScroll = true;
	char consoleFilter[128] = "";
	int selectedLogIndex = -1;

	// Estado del panel Configuration
	char windowTitleBuffer[128] = "Motor Videojuego";

	// Portapapeles para copiar y pegar componentes en el Editor
	std::unique_ptr<Component> copiedComponent;
};
