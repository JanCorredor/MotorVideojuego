#include "ModuleUI.h"
#include "../Engine.h"
#include "../Window/Window.h"
#include "../Input/Input.h"
#include "../Render/Render.h"
#include "../Scene/Scene.h"
#include "../Scene/GameObjects/Components/TransformComponent.h"
#include "../Scene/GameObjects/Components/MeshComponent.h"
#include "../Scene/GameObjects/Components/MaterialComponent.h"
#include "../Scene/GameObjects/Components/CameraComponent.h"
#include "../Audio/Audio.h"

#include <glm/gtc/type_ptr.hpp>

// ImGui headers (adjust paths as needed depending on your project structure)
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <SDL3/SDL.h>

#include "../../Utils/Log.h"
#include <algorithm>

#include <tracy/Tracy.hpp>

// Helper para alinear etiquetas y campos vec3 uniformemente en tablas
static void DrawVec3Control(const char* label, glm::vec3& values, float speed = 0.1f)
{
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);

	ImGui::TableSetColumnIndex(1);
	ImGui::PushItemWidth(-1.0f);
	std::string id = std::string("##") + label;
	ImGui::DragFloat3(id.c_str(), glm::value_ptr(values), speed);
	ImGui::PopItemWidth();
}

ModuleUI::ModuleUI() : Module()
{
	name = "UI";
}

ModuleUI::~ModuleUI()
{
}

bool ModuleUI::Awake()
{
	return true;
}

bool ModuleUI::Start()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForOpenGL(Engine::GetInstance().window->GetWindow(), Engine::GetInstance().window->GetGLContext());
	ImGui_ImplOpenGL3_Init("#version 130");

	LOG("[ImGui] Inicializando Dear ImGui versión: %s", IMGUI_VERSION);
	LOG("[ImGui] Backends inicializados: SDL3 + OpenGL3 (#version 130), Docking habilitado");

	int initW = 0, initH = 0;
	Engine::GetInstance().window->GetWindowSize(initW, initH);
	configWindowWidth = initW;
	configWindowHeight = initH;
	const char* initTitle = Engine::GetInstance().window->GetTitle();
	if (initTitle != nullptr)
	{
		strncpy_s(windowTitleBuffer, sizeof(windowTitleBuffer), initTitle, _TRUNCATE);
	}

	return true;
}

bool ModuleUI::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	
	return true;
}

bool ModuleUI::Update(float dt)
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	CheckDroppedFile();

	// --- Sincronizar selección desde la cámara (Raycast) hacia la UI ---
	GameObject* mainCamera = Engine::GetInstance().scene->GetMainCameraObject();
	if (mainCamera) {
		CameraComponent* camComp = mainCamera->GetComponent<CameraComponent>();
		// Si la cámara tiene seleccionado un objeto distinto al de la UI, actualizamos la UI
		if (camComp && camComp->GetSelectedObject() != selectedGameObject) {
			selectedGameObject = camComp->GetSelectedObject();
		}
	}

	// Barra de menú principal
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Scene", nullptr, &showScene);
			ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &showInspector);
			ImGui::MenuItem("Console", nullptr, &showConsole);
			ImGui::MenuItem("Configuration", nullptr, &showConfiguration);
			ImGui::MenuItem("Render Settings", nullptr, &showRenderSettings);
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{
				resetLayout = true;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Enable a DockSpace over the entire main viewport so windows can be docked properly
	ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, mainViewport);

	// Disposición inicial fija mediante DockBuilder (se ejecuta la primera vez o al resetear)
	static bool layoutInitialized = false;
	if (!layoutInitialized || resetLayout)
	{
		ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
		if (resetLayout || node == nullptr || node->ChildNodes[0] == nullptr)
		{
			ImGui::DockBuilderRemoveNode(dockspace_id);
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace_id, mainViewport->WorkSize);

			ImGuiID dock_main_id = dockspace_id;
			ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
			ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
			ImGuiID dock_right_down = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.45f, nullptr, &dock_right);
			ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

			ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
			ImGui::DockBuilderDockWindow("Scene", dock_main_id);
			ImGui::DockBuilderDockWindow("Console", dock_down);
			ImGui::DockBuilderDockWindow("Inspector", dock_right);
			ImGui::DockBuilderDockWindow("Configuration", dock_right_down);
			ImGui::DockBuilderDockWindow("Render Settings", dock_right_down);

			ImGui::DockBuilderFinish(dockspace_id);
		}
		layoutInitialized = true;
		resetLayout = false;
	}

	// Scene panel
	if (showScene)
	{
		ImGui::Begin("Scene", &showScene);
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		if (viewportSize.x > 0 && viewportSize.y > 0) {
			Engine::GetInstance().render->SetViewportSize((int)viewportSize.x, (int)viewportSize.y);
		}
		unsigned int sceneTexture = Engine::GetInstance().render->GetSceneTexture();
		if (sceneTexture != 0 && viewportSize.x > 0 && viewportSize.y > 0) {

			// Guardamos la posición en la pantalla donde empieza la imagen
			ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();

			// Dibujamos la imagen de la escena
			ImGui::Image((ImTextureID)(intptr_t)sceneTexture, viewportSize, ImVec2(0, 1), ImVec2(1, 0));

			// Detectamos si se ha hecho clic IZQUIERDO específicamente dentro de la imagen
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				// Obtenemos la posición global del ratón en ImGui
				ImVec2 mousePos = ImGui::GetMousePos();

				// Calculamos la posición local del ratón restando el inicio de la imagen
				float localMouseX = mousePos.x - cursorScreenPos.x;
				float localMouseY = mousePos.y - cursorScreenPos.y;

				// Disparamos el Raycast desde la cámara con estas coordenadas perfectas
				GameObject* mainCamera = Engine::GetInstance().scene->GetMainCameraObject();
				if (mainCamera) {
					CameraComponent* camComp = mainCamera->GetComponent<CameraComponent>();
					if (camComp) {
						camComp->PerformRaycast(localMouseX, localMouseY, viewportSize.x, viewportSize.y);

						// Sincronizamos la UI con el nuevo objeto seleccionado
						selectedGameObject = camComp->GetSelectedObject();
					}
				}
			}
		}
		ImGui::End();
	}

	// Hierarchy panel
	if (showHierarchy)
	{
		ImGui::Begin("Hierarchy", &showHierarchy);
		if (selectedGameObject)
		{
			if (ImGui::SmallButton("Deseleccionar"))
			{
				selectedGameObject = nullptr;
				GameObject* mainCamera = Engine::GetInstance().scene->GetMainCameraObject();
				if (mainCamera) {
					CameraComponent* camComp = mainCamera->GetComponent<CameraComponent>();
					if (camComp) camComp->SetSelectedObject(nullptr);
				}
			}
			ImGui::Separator();
		}

		if (Engine::GetInstance().scene) {
			GameObject* root = Engine::GetInstance().scene->GetRoot();
			if (root) {
				DrawHierarchyNode(root);
			}
		}
		ImGui::End();
	}

	// Inspector panel
	if (showInspector)
	{
		ImGui::Begin("Inspector", &showInspector);
		if (selectedGameObject) {
			ImGui::TextDisabled("Entidad Seleccionada");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", selectedGameObject->GetName().c_str());
			
			ImGui::Separator();

			TransformComponent* transform = selectedGameObject->GetComponent<TransformComponent>();
			if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) 
			{
				if (ImGui::BeginTable("TransformTable", 2, ImGuiTableFlags_SizingFixedFit)) 
				{
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 75.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

					glm::vec3 pos = transform->GetPosition();
					DrawVec3Control("Position", pos, 0.1f);
					transform->SetPosition(pos);
					
					glm::vec3 rot = transform->GetRotation();
					DrawVec3Control("Rotation", rot, 1.0f);
					transform->SetRotation(rot);
					
					glm::vec3 scl = transform->GetScale();
					DrawVec3Control("Scale", scl, 0.1f);
					transform->SetScale(scl);

					ImGui::EndTable();
				}
			} 
			else if (!transform)
			{
				ImGui::TextDisabled("Sin TransformComponent");
			}

			MeshComponent* mesh = selectedGameObject->GetComponent<MeshComponent>();
			if (mesh && ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) 
			{
				ImGui::BulletText("Primitiva: %s", mesh->GetPrimitive() ? "Malla cargada en GPU" : "Ninguna");
			}
			else if (!mesh)
			{
				ImGui::TextDisabled("Sin MeshComponent");
			}

			MaterialComponent* mat = selectedGameObject->GetComponent<MaterialComponent>();
			if (mat && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool usingTexture = mat->GetUseTexture();
				if (ImGui::Checkbox("Usar Textura", &usingTexture)) mat->SetUseTexture(usingTexture);

				if (usingTexture)
				{
					ImGui::BulletText("ID Textura OpenGL: %u", mat->GetTextureID());
				}

				ImGui::Spacing();
				glm::vec3 color = mat->GetColor();
				ImGui::Text("Color Base:");
				if (ImGui::ColorEdit3("##MatColor", glm::value_ptr(color), ImGuiColorEditFlags_PickerHueWheel)) 
				{
					mat->SetColor(color);
				}
			}
			else if (!mat)
			{
				ImGui::TextDisabled("Sin MaterialComponent");
			}
		} 
		else 
		{
			ImGui::TextDisabled("Selecciona un GameObject en Hierarchy para inspeccionar sus componentes.");
		}
		ImGui::End();
	}

	// Render Options panel
	if (showRenderSettings)
	{
		ImGui::Begin("Render Settings", &showRenderSettings);
		if (ImGui::CollapsingHeader("Ajustes OpenGL", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Depth Test", &Engine::GetInstance().render->depthTestEnabled);
			ImGui::Checkbox("Cull Face", &Engine::GetInstance().render->cullFaceEnabled);
			ImGui::Checkbox("Lighting", &Engine::GetInstance().render->lightingEnabled);
			ImGui::Checkbox("Wireframe Mode", &Engine::GetInstance().render->wireframeMode);
		}
		ImGui::End();
	}

	// Configuration panel
	if (showConfiguration)
	{
		DrawConfigurationPanel();
	}

	// Console panel
	if (showConsole)
	{
		DrawConsolePanel();
	}

	return true;
}

void ModuleUI::DrawHierarchyNode(GameObject* node)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (selectedGameObject == node) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (node->GetChildren().empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	bool nodeOpen = ImGui::TreeNodeEx(node, flags, node->GetName().c_str());

	if (ImGui::IsItemClicked()) {
		selectedGameObject = node;

		// Sincronizar selección desde la UI hacia la cámara
		GameObject* mainCamera = Engine::GetInstance().scene->GetMainCameraObject();
		if (mainCamera) {
			CameraComponent* camComp = mainCamera->GetComponent<CameraComponent>();
			if (camComp) {
				camComp->SetSelectedObject(node);
			}
		}
	}

	if (nodeOpen && !node->GetChildren().empty()) {
		for (auto child : node->GetChildren()) {
			DrawHierarchyNode(child);
		}
		ImGui::TreePop();
	}
}

bool ModuleUI::PostUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	RenderUI();
	return true;
}

bool ModuleUI::CleanUp()
{
	LOG("[ImGui] Apagando Dear ImGui y liberando recursos...");
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	return true;
}

void ModuleUI::CheckDroppedFile()
{
	// Check if file has been dropped in the window
	if (Engine::GetInstance().input->HasDroppedFile())
	{
		std::string filePath = Engine::GetInstance().input->GetDroppedFilePath();

		if (filePath.find(".png") != std::string::npos ||
			filePath.find(".dds") != std::string::npos ||
			filePath.find(".jpg") != std::string::npos ||
			filePath.find(".jpeg") != std::string::npos ||
			filePath.find(".tga") != std::string::npos)
		{
			if (selectedGameObject != nullptr)
			{
				MaterialComponent* mat = selectedGameObject->GetComponent<MaterialComponent>();
				if (mat != nullptr)
				{
					mat->SetTexture(filePath.c_str());
					mat->SetUseTexture(true);
					LOG("[UI] Textura '%s' asignada exitosamente al GameObject '%s'", filePath.c_str(), selectedGameObject->GetName().c_str());
				}
				else
				{
					LOG_WARNING("[UI] El GameObject '%s' seleccionado no tiene un MaterialComponent para asignar la textura '%s'",
						selectedGameObject->GetName().c_str(), filePath.c_str());
				}
			}
			else
			{
				LOG_WARNING("[UI] Se soltó la textura '%s' pero no hay ningún GameObject seleccionado en la jerarquía", filePath.c_str());
			}
		}
	}
}

void ModuleUI::RenderUI()
{
	ImGui::Render();
	
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
	}
}

void ModuleUI::DrawConsolePanel()
{
	if (!showConsole) return;

	ImGui::Begin("Console", &showConsole);

	// 1. Barra de herramientas superior
	if (ImGui::Button("Clear"))
	{
		LogSystem::GetInstance().Clear();
		selectedLogIndex = -1;
	}
	ImGui::SameLine();

	ImGui::Checkbox("Collapse", &collapseLogs);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine();

	// Filtro de búsqueda por texto
	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputTextWithHint("##ConsoleSearch", "Buscar...", consoleFilter, sizeof(consoleFilter));
	if (consoleFilter[0] != '\0')
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("x"))
		{
			consoleFilter[0] = '\0';
		}
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// Filtros por Categoría con Contadores
	LogSystem& logSys = LogSystem::GetInstance();

	// Info
	ImVec4 infoColor = showInfo ? ImVec4(0.35f, 0.75f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, infoColor);
	std::string infoLabel = "[i] Info (" + std::to_string(logSys.GetInfoCount()) + ")";
	if (ImGui::SmallButton(infoLabel.c_str()))
	{
		showInfo = !showInfo;
	}
	ImGui::PopStyleColor();
	ImGui::SameLine();

	// Warning
	ImVec4 warnColor = showWarnings ? ImVec4(1.0f, 0.82f, 0.25f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, warnColor);
	std::string warnLabel = "[!] Warn (" + std::to_string(logSys.GetWarningCount()) + ")";
	if (ImGui::SmallButton(warnLabel.c_str()))
	{
		showWarnings = !showWarnings;
	}
	ImGui::PopStyleColor();
	ImGui::SameLine();

	// Error
	ImVec4 errColor = showErrors ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, errColor);
	std::string errLabel = "[X] Error (" + std::to_string(logSys.GetErrorCount()) + ")";
	if (ImGui::SmallButton(errLabel.c_str()))
	{
		showErrors = !showErrors;
	}
	ImGui::PopStyleColor();

	ImGui::Separator();

	// 2. Cálculo de altura para panel de detalles inferior
	float detailsHeight = (selectedLogIndex >= 0) ? 95.0f : 0.0f;
	float listHeight = ImGui::GetContentRegionAvail().y - detailsHeight;
	if (detailsHeight > 0.0f) listHeight -= ImGui::GetStyle().ItemSpacing.y;

	ImGui::BeginChild("LogScrollRegion", ImVec2(0, listHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

	const std::vector<LogEntry> logs = logSys.GetLogs();
	std::string searchLower = consoleFilter;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

	for (int i = 0; i < (int)logs.size(); ++i)
	{
		const auto& entry = logs[i];

		// Filtro por categoría
		if (entry.type == LogType::Info && !showInfo) continue;
		if (entry.type == LogType::Warning && !showWarnings) continue;
		if (entry.type == LogType::Error && !showErrors) continue;

		// Filtro de búsqueda por texto
		if (!searchLower.empty())
		{
			std::string textLower = entry.text;
			std::transform(textLower.begin(), textLower.end(), textLower.begin(), ::tolower);
			if (textLower.find(searchLower) == std::string::npos) continue;
		}

		ImVec4 textColor;
		const char* typeTag = "[INFO]";
		switch (entry.type)
		{
		case LogType::Info:
			textColor = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
			typeTag = "[INFO]";
			break;
		case LogType::Warning:
			textColor = ImVec4(1.0f, 0.82f, 0.25f, 1.0f);
			typeTag = "[WARN]";
			break;
		case LogType::Error:
			textColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			typeTag = "[ERR ]";
			break;
		}

		ImGui::PushID(i);

		bool isSelected = (selectedLogIndex == i);

		// Timestamp
		ImGui::TextDisabled("[%s]", entry.timestamp.c_str());
		ImGui::SameLine();

		// Tag con color
		ImGui::PushStyleColor(ImGuiCol_Text, textColor);
		ImGui::TextUnformatted(typeTag);
		ImGui::PopStyleColor();
		ImGui::SameLine();

		// Badge de Collapse
		if (collapseLogs && entry.count > 1)
		{
			std::string countStr = "[" + std::to_string(entry.count) + "]";
			ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%s", countStr.c_str());
			ImGui::SameLine();
		}

		// Texto del log seleccionable
		if (ImGui::Selectable(entry.text.c_str(), isSelected))
		{
			selectedLogIndex = (selectedLogIndex == i) ? -1 : i;
		}

		ImGui::PopID();
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();

	// 3. Panel inferior de detalle del mensaje seleccionado
	if (selectedLogIndex >= 0 && selectedLogIndex < (int)logs.size())
	{
		ImGui::Separator();
		const auto& sel = logs[selectedLogIndex];

		ImGui::BeginChild("LogDetailsRegion", ImVec2(0, detailsHeight), true);

		ImGui::TextDisabled("Detalle:");
		ImGui::SameLine();
		if (ImGui::SmallButton("Copiar Mensaje"))
		{
			std::string fullText = "[" + sel.timestamp + "] " + sel.text;
			if (!sel.file.empty()) fullText += " (" + sel.file + ":" + std::to_string(sel.line) + ")";
			ImGui::SetClipboardText(fullText.c_str());
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Cerrar Detalle"))
		{
			selectedLogIndex = -1;
		}

		if (!sel.file.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("| Origen: %s:%d", sel.file.c_str(), sel.line);
		}

		ImGui::TextWrapped("%s", sel.text.c_str());

		ImGui::EndChild();
	}

	ImGui::End();
}

void ModuleUI::DrawConfigurationPanel()
{
	if (!showConfiguration) return;

	if (ImGui::Begin("Configuration", &showConfiguration))
	{
		if (ImGui::BeginTabBar("ConfigurationTabBar", ImGuiTabBarFlags_None))
		{
			// =========================================================================
			// 1. MONITORIZACIÓN DEL RENDIMIENTO (APPLICATION)
			// =========================================================================
			if (ImGui::BeginTabItem("Application"))
			{
				const auto& perf = Engine::GetInstance().GetPerformanceData();

				ImGui::Text("Motor: Motor Videojuego 3D");
				ImGui::BulletText("Tiempo Activo (Uptime): %.1f s", perf.uptimeSeconds);
				ImGui::BulletText("Frames Totales: %zu", perf.totalFrames);
				ImGui::BulletText("Consumo RAM del Proceso: %zu MB (Pico: %zu MB)", perf.memoryUsageMB, perf.peakMemoryUsageMB);

				ImGui::Spacing();
				ImGui::SeparatorText("Control de Framerate");

				int targetFps = Engine::GetInstance().GetTargetFrameRate();
				if (ImGui::SliderInt("Límite FPS (0=Ilimitado)", &targetFps, 0, 240))
				{
					Engine::GetInstance().SetTargetFrameRate(targetFps);
				}

				bool vsync = Engine::GetInstance().render->GetVSync();
				if (ImGui::Checkbox("V-Sync (Sincronización Vertical)", &vsync))
				{
					Engine::GetInstance().render->SetVSync(vsync);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Gráficos de Rendimiento en Tiempo Real");

				// Gráfico FPS
				char fpsOverlay[128];
				sprintf_s(fpsOverlay, sizeof(fpsOverlay), "FPS: %.1f (Min: %.1f, Max: %.1f, Med: %.1f)",
					perf.currentFps, perf.minFps, perf.maxFps, perf.avgFps);
				ImGui::PlotLines("##FPSGraph", perf.fpsHistory, (int)PerformanceData::HISTORY_CAPACITY,
					(int)perf.historyIndex, fpsOverlay, 0.0f, 140.0f, ImVec2(0, 80));

				// Gráfico Frame Time (ms)
				char msOverlay[128];
				sprintf_s(msOverlay, sizeof(msOverlay), "Frame Time: %.2f ms (Min: %.2f, Max: %.2f, Med: %.2f)",
					perf.currentMs, perf.minMs, perf.maxMs, perf.avgMs);
				ImGui::PlotLines("##MSGraph", perf.msHistory, (int)PerformanceData::HISTORY_CAPACITY,
					(int)perf.historyIndex, msOverlay, 0.0f, 35.0f, ImVec2(0, 80));

				ImGui::Spacing();
				ImGui::SeparatorText("Resumen Estadístico");

				if (ImGui::BeginTable("PerfMetricsSummary", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
				{
					ImGui::TableSetupColumn("Métrica");
					ImGui::TableSetupColumn("Mínimo");
					ImGui::TableSetupColumn("Actual");
					ImGui::TableSetupColumn("Máximo");
					ImGui::TableHeadersRow();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("FPS");
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", perf.minFps);
					ImGui::TableSetColumnIndex(2); ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%.1f", perf.currentFps);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f", perf.maxFps);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("Tiempo Frame (ms)");
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f ms", perf.minMs);
					ImGui::TableSetColumnIndex(2); ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%.2f ms", perf.currentMs);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f ms", perf.maxMs);

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 2. CONFIGURACIÓN DEL MÓDULO WINDOW
			// =========================================================================
			if (ImGui::BeginTabItem("Window"))
			{
				auto win = Engine::GetInstance().window;
				int curW = 0, curH = 0;
				win->GetWindowSize(curW, curH);

				ImGui::SeparatorText("Propiedades en Tiempo Real");

				ImGui::InputText("Título", windowTitleBuffer, sizeof(windowTitleBuffer));
				ImGui::SameLine();
				if (ImGui::Button("Aplicar Título"))
				{
					win->SetTitle(windowTitleBuffer);
				}

				ImGui::DragInt("Ancho (px)", &configWindowWidth, 5.0f, 640, 3840);
				ImGui::DragInt("Alto (px)", &configWindowHeight, 5.0f, 480, 2160);

				if (ImGui::Button("Aplicar Tamaño"))
				{
					win->SetWindowSize(configWindowWidth, configWindowHeight);
				}
				ImGui::SameLine();
				if (ImGui::Button("Restablecer Valores"))
				{
					configWindowWidth = curW;
					configWindowHeight = curH;
				}

				ImGui::Spacing();
				bool isFull = win->IsFullscreen();
				if (ImGui::Checkbox("Pantalla Completa (Fullscreen)", &isFull))
				{
					win->SetFullscreen(isFull);
				}

				bool isRes = win->IsResizable();
				if (ImGui::Checkbox("Ventana Redimensionable (Resizable)", &isRes))
				{
					win->SetResizable(isRes);
				}

				bool isBrd = win->IsBorderless();
				if (ImGui::Checkbox("Sin Bordes (Borderless)", &isBrd))
				{
					win->SetBorderless(isBrd);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Parámetros Fijos [Requiere Reinicio]");
				ImGui::TextDisabled("• API Gráfica: OpenGL 4.6 Core Profile");
				ImGui::TextDisabled("• Doble Búfer (Double Buffer): Habilitado");
				ImGui::TextDisabled("• Profundidad Z-Buffer: 24 bits");
				ImGui::TextDisabled("• Stencil Buffer: 8 bits");
				ImGui::TextDisabled("• Posición Inicial: Centrada en pantalla");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 3. CONFIGURACIÓN DEL MÓDULO RENDERER
			// =========================================================================
			if (ImGui::BeginTabItem("Renderer"))
			{
				auto rnd = Engine::GetInstance().render;

				ImGui::SeparatorText("Opciones Gráficas en Tiempo Real");

				bool vsync = rnd->GetVSync();
				if (ImGui::Checkbox("V-Sync (Sincronización Vertical)", &vsync))
				{
					rnd->SetVSync(vsync);
				}

				ImGui::Checkbox("Depth Test (Prueba de Profundidad)", &rnd->depthTestEnabled);
				ImGui::Checkbox("Cull Face (Descarte de Caras Traseras)", &rnd->cullFaceEnabled);
				ImGui::Checkbox("Lighting (Iluminación básica)", &rnd->lightingEnabled);
				ImGui::Checkbox("Wireframe Mode (Modo Alambre)", &rnd->wireframeMode);

				ImGui::Spacing();
				SDL_Color bg = rnd->GetBackgroundColor();
				float bgFloat[3] = { bg.r / 255.0f, bg.g / 255.0f, bg.b / 255.0f };
				if (ImGui::ColorEdit3("Color de Fondo (Clear Color)", bgFloat, ImGuiColorEditFlags_PickerHueWheel))
				{
					SDL_Color newBg = {
						(Uint8)(bgFloat[0] * 255.0f),
						(Uint8)(bgFloat[1] * 255.0f),
						(Uint8)(bgFloat[2] * 255.0f),
						255
					};
					rnd->SetBackgroundColor(newBg);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Estado del Framebuffer (FBO)");
				ImGui::BulletText("Resolución Render: %d x %d px", rnd->GetViewportWidth(), rnd->GetViewportHeight());
				ImGui::BulletText("ID Framebuffer (FBO): %u", rnd->GetFramebufferID());
				ImGui::BulletText("ID Textura Color: %u", rnd->GetSceneTexture());
				ImGui::BulletText("ID Renderbuffer (RBO Profundidad): %u", rnd->GetRboID());
				ImGui::BulletText("Estado OpenGL: GL_FRAMEBUFFER_COMPLETE");

				ImGui::Spacing();
				ImGui::SeparatorText("Arquitectura de Shaders [Requiere Reinicio]");
				ImGui::TextDisabled("• Pipeline: Forward Rendering");
				ImGui::TextDisabled("• Versión GLSL: #version 460 core");
				ImGui::TextDisabled("• Formato Colorbuffer: GL_RGB 8-bit");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 4. CONFIGURACIÓN DEL MÓDULO INPUT
			// =========================================================================
			if (ImGui::BeginTabItem("Input"))
			{
				auto inp = Engine::GetInstance().input;

				ImGui::SeparatorText("Ratón (Mouse)");
				ImGui::BulletText("Posición Pantalla: (%d, %d)", inp->GetMouseX(), inp->GetMouseY());
				ImGui::BulletText("Movimiento Delta: (%d, %d)", inp->GetMouseMotionX(), inp->GetMouseMotionY());
				ImGui::BulletText("Rueda / Scroll Delta: %.2f", inp->GetMouseScrollDelta());

				ImGui::Spacing();
				ImGui::Text("Botones del Ratón:");
				ImGui::SameLine();
				auto drawButtonBadge = [](const char* name, bool pressed) {
					if (pressed) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
						ImGui::SmallButton(name);
						ImGui::PopStyleColor();
					} else {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
						ImGui::SmallButton(name);
						ImGui::PopStyleColor();
					}
					ImGui::SameLine();
				};

				drawButtonBadge("Izquierdo", inp->GetMouseButton(1) == KEY_REPEAT || inp->GetMouseButton(1) == KEY_DOWN);
				drawButtonBadge("Central", inp->GetMouseButton(2) == KEY_REPEAT || inp->GetMouseButton(2) == KEY_DOWN);
				drawButtonBadge("Derecho", inp->GetMouseButton(3) == KEY_REPEAT || inp->GetMouseButton(3) == KEY_DOWN);
				ImGui::NewLine();

				ImGui::Spacing();
				ImGui::SeparatorText("Mando / Gamepad");
				bool gpConnected = inp->IsGamepadConnected();
				ImGui::BulletText("Estado: %s", gpConnected ? "Conectado" : "No detectado");
				if (gpConnected)
				{
					ImGui::BulletText("Stick Izquierdo: (%.2f, %.2f)",
						inp->GetGamepadAxis(GAMEPAD_AXIS_LSTICK_X), inp->GetGamepadAxis(GAMEPAD_AXIS_LSTICK_Y));
					ImGui::BulletText("Stick Derecho: (%.2f, %.2f)",
						inp->GetGamepadAxis(GAMEPAD_AXIS_RSTICK_X), inp->GetGamepadAxis(GAMEPAD_AXIS_RSTICK_Y));
					ImGui::BulletText("Gatillos (LT / RT): (%.2f / %.2f)",
						inp->GetGamepadAxis(GAMEPAD_AXIS_LT), inp->GetGamepadAxis(GAMEPAD_AXIS_RT));
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Drag & Drop (Archivos Arrastrados)");
				if (inp->HasDroppedFile())
				{
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Archivo detectado: %s", inp->GetDroppedFilePath().c_str());
				}
				else
				{
					ImGui::TextDisabled("Ningún archivo arrastrado en este frame.");
				}

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 5. CONFIGURACIÓN DEL MÓDULO TEXTURES (DEVIL / OPENGL)
			// =========================================================================
			if (ImGui::BeginTabItem("Textures"))
			{
				const auto& hw = Engine::GetInstance().GetHardwareMetrics();

				ImGui::SeparatorText("Subsistema DevIL");
				ImGui::BulletText("Librería de Carga: Developer's Image Library (DevIL)");
				ImGui::BulletText("Versión Interna DevIL: %s", hw.devilVersion.c_str());
				ImGui::BulletText("Formatos Compatibles: PNG, JPG, JPEG, DDS, TGA, BMP");

				ImGui::Spacing();
				ImGui::SeparatorText("Parámetros por Defecto para Nuevas Texturas");

				static const char* minFilters[] = {
					"GL_LINEAR_MIPMAP_LINEAR (Máxima Calidad)",
					"GL_LINEAR (Bilineal sin Mipmaps)",
					"GL_NEAREST (Pixel Art / Más rápido)"
				};
				static int currentMinFilter = 0;
				ImGui::Combo("Filtro Minificación", &currentMinFilter, minFilters, IM_ARRAYSIZE(minFilters));

				static const char* magFilters[] = {
					"GL_LINEAR (Suave)",
					"GL_NEAREST (Pixelado)"
				};
				static int currentMagFilter = 0;
				ImGui::Combo("Filtro Magnificación", &currentMagFilter, magFilters, IM_ARRAYSIZE(magFilters));

				static const char* wrapModes[] = {
					"GL_REPEAT (Repetición)",
					"GL_CLAMP_TO_EDGE (Abrazar bordes)",
					"GL_MIRRORED_REPEAT (Espejado)"
				};
				static int currentWrap = 0;
				ImGui::Combo("Modo Wrap (S/T)", &currentWrap, wrapModes, IM_ARRAYSIZE(wrapModes));

				ImGui::Spacing();
				ImGui::SeparatorText("Gestión de Memoria de Texturas [Requiere Reinicio]");
				ImGui::TextDisabled("• Las texturas instanciadas en GPU residen en la VRAM de OpenGL.");
				ImGui::TextDisabled("• Para re-muestrear texturas existentes con nuevos filtros globales,");
				ImGui::TextDisabled("  es necesario reasignar la textura al material o reiniciar la escena.");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 6. CONFIGURACIÓN DEL MÓDULO AUDIO
			// =========================================================================
			if (ImGui::BeginTabItem("Audio"))
			{
				auto aud = Engine::GetInstance().audio;

				ImGui::SeparatorText("Volúmenes Maestros en Tiempo Real");

				float musicVol = aud->GetMusicVolume();
				if (ImGui::SliderFloat("Volumen Música", &musicVol, 0.0f, 1.0f, "%.2f (%.0f%%)"))
				{
					aud->SetMusicVolume(musicVol);
				}

				float sfxVol = aud->GetSFXVolume();
				if (ImGui::SliderFloat("Volumen Efectos (SFX)", &sfxVol, 0.0f, 1.0f, "%.2f (%.0f%%)"))
				{
					aud->SetSFXVolume(sfxVol);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Dispositivo de Salida de Audio");
				ImGui::BulletText("Driver de Audio: SDL3 Audio Subsystem");
				ImGui::BulletText("Estado: Inicializado correctamente");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 7. INFORMACIÓN DEL SISTEMA Y HARDWARE
			// =========================================================================
			if (ImGui::BeginTabItem("Hardware"))
			{
				const auto& hw = Engine::GetInstance().GetHardwareMetrics();
				const auto& perf = Engine::GetInstance().GetPerformanceData();

				// CPU
				ImGui::SeparatorText("Procesador (CPU)");
				ImGui::BulletText("Núcleos Lógicos: %d", hw.cpuCores);
				ImGui::BulletText("Memoria RAM del Sistema: %d MB (%.1f GB)", hw.systemRamMB, hw.systemRamMB / 1024.0f);
				ImGui::BulletText("Tamaño Línea Caché L1/L2: %d bytes", hw.cpuCacheLineSize);

				ImGui::Text("Conjuntos de Instrucciones:");
				ImGui::SameLine();
				auto drawInstructionBadge = [](const char* name, bool supported) {
					if (supported) {
						ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[%s]", name);
						ImGui::SameLine();
					}
				};
				drawInstructionBadge("AVX", hw.hasAVX);
				drawInstructionBadge("AVX2", hw.hasAVX2);
				drawInstructionBadge("AVX512", hw.hasAVX512);
				drawInstructionBadge("SSE", hw.hasSSE);
				drawInstructionBadge("SSE2", hw.hasSSE2);
				drawInstructionBadge("SSE3", hw.hasSSE3);
				drawInstructionBadge("SSE4.1", hw.hasSSE41);
				drawInstructionBadge("SSE4.2", hw.hasSSE42);
				ImGui::NewLine();

				// GPU
				ImGui::Spacing();
				ImGui::SeparatorText("Tarjeta Gráfica (GPU)");
				ImGui::BulletText("Fabricante (Vendor): %s", hw.gpuVendor.c_str());
				ImGui::BulletText("Modelo / Renderizador: %s", hw.gpuRenderer.c_str());
				ImGui::BulletText("Versión Driver OpenGL: %s", hw.gpuVersion.c_str());
				ImGui::BulletText("Versión de Shading (GLSL): %s", hw.glslVersion.c_str());
				ImGui::BulletText("Tamaño Máx. de Textura: %d x %d px", hw.maxTextureSize, hw.maxTextureSize);
				ImGui::BulletText("Tamaño Máx. de Viewport: %d x %d px", hw.maxViewportDims[0], hw.maxViewportDims[1]);

				if (hw.vramTotalMB > 0)
				{
					ImGui::BulletText("VRAM Dedicada Total: %d MB", hw.vramTotalMB);
					ImGui::BulletText("VRAM Disponible: %d MB", hw.vramAvailableMB);
				}

				// Memoria del proceso
				ImGui::Spacing();
				ImGui::SeparatorText("Consumo de Memoria del Proceso (RAM)");
				ImGui::BulletText("Memoria de Trabajo Actual (Working Set): %zu MB", perf.memoryUsageMB);
				ImGui::BulletText("Pico Máximo de Memoria (Peak Working Set): %zu MB", perf.peakMemoryUsageMB);

				// Software y Bibliotecas
				ImGui::Spacing();
				ImGui::SeparatorText("Software & Bibliotecas Integradas");
				ImGui::BulletText("Sistema Operativo: %s", hw.osPlatform.c_str());
				ImGui::BulletText("SDL3: v%s", hw.sdlVersion.c_str());
				ImGui::BulletText("OpenGL: 4.6 Core Profile");
				ImGui::BulletText("DevIL: v%s", hw.devilVersion.c_str());
				ImGui::BulletText("Assimp: v%s", hw.assimpVersion.c_str());
				ImGui::BulletText("Dear ImGui: v%s", hw.imguiVersion.c_str());

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}

