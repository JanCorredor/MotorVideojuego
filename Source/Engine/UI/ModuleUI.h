#pragma once

#include "../Module.h"
#include <memory>
#include "../Scene/GameObjects/GameObject.h"

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

	void CheckDroppedFile();

	void ResetLayout() { resetLayout = true; }

private:
	void RenderUI();
	void DrawHierarchyNode(GameObject* node);
	void DrawConsolePanel();
	void DrawConfigurationPanel();

	GameObject* selectedGameObject = nullptr;
	bool resetLayout = false;

	// Visibilidad de ventanas
	bool showScene = true;
	bool showHierarchy = true;
	bool showInspector = true;
	bool showConsole = true;
	bool showConfiguration = true;
	bool showRenderSettings = false;

	// Estado del panel Console
	bool showInfo = true;
	bool showWarnings = true;
	bool showErrors = true;
	bool collapseLogs = false;
	bool autoScroll = true;
	char consoleFilter[128] = "";
	int selectedLogIndex = -1;

	// Estado del panel Configuration
	char windowTitleBuffer[128] = "Motor Videojuego";
	int configWindowWidth = 1280;
	int configWindowHeight = 780;
};
