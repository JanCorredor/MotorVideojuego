#include "ModuleUI.h"
#include "../Engine.h"
#include "../Window/Window.h"
#include "../Input/Input.h"

#include "../Render/Render.h"
#include "../Render/Texture/TextureManager.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include "../Scene/Scene.h"
#include "../Scene/GameObjects/GameObject.h"
#include "../Scene/GameObjects/Components/TransformComponent.h"
#include "../Scene/GameObjects/Components/MeshComponent.h"
#include "../Scene/GameObjects/Components/MaterialComponent.h"
#include "../ResourceManagement/ResourceManager.h"
#include "../ResourceManagement/ResourceMesh.h"
#include "../ResourceManagement/ResourceTexture.h"
#include "../Scene/GameObjects/Components/CameraComponent.h"


#include "../Audio/Audio.h"

#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <imnodes.h>

#include <SDL3/SDL.h>

#include "../../Utils/Log.h"
#include "../FileSystem/FileSystem.h"
#include <algorithm>
#include <cctype>
#include <string>

#include <functional>
#include <cfloat>
#include <stack>
#include <filesystem>

#ifdef _DEBUG
	#include <tracy/Tracy.hpp>
#endif

namespace
{
	constexpr const char* REPO_URL_DOCS     = "https://github.com/JanCorredor/MotorVideojuego";
	constexpr const char* REPO_URL_ISSUES   = "https://github.com/JanCorredor/MotorVideojuego/issues";
	constexpr const char* REPO_URL_RELEASES = "https://github.com/JanCorredor/MotorVideojuego/releases";

	struct ComponentDragPayload
	{
		GameObject* sourceObject = nullptr;
		ComponentType type = COMPONENT_TRANSFORM;
	};

	const char* GetComponentTypeName(ComponentType type)
	{
		switch (type)
		{
		case COMPONENT_TRANSFORM: return "Transform";
		case COMPONENT_MESH:      return "Mesh";
		case COMPONENT_MATERIAL:  return "Texture & Material";
		case COMPONENT_CAMERA:    return "Camera";
		default:                  return "Component";
		}
	}

	// Helper para alinear etiquetas y campos vec3 uniformemente en tablas
	bool DrawVec3Control(const char* label, glm::vec3& values, float speed = 0.1f)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);

		ImGui::TableSetColumnIndex(2);
		ImGui::PushItemWidth(-1.0f);
		std::string id = std::string("##") + label;
		bool changed = ImGui::DragFloat3(id.c_str(), glm::value_ptr(values), speed);
		ImGui::PopItemWidth();
		return changed;
	}

	bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needleLower)
	{
		if (needleLower.empty()) return true;
		if (needleLower.size() > haystack.size()) return false;
		auto it = std::search(
			haystack.begin(), haystack.end(),
			needleLower.begin(), needleLower.end(),
			[](unsigned char a, unsigned char b) { return std::tolower(a) == b; }
		);
		return it != haystack.end();
	}

	void OpenWebURL(const char* url)
	{
		if (!SDL_OpenURL(url))
		{
			LOG_ERROR("[UI] No se pudo abrir la URL '%s': %s", url, SDL_GetError());
		}
		else
		{
			LOG("[UI] Abriendo navegador en: %s", url);
		}
	}

	std::string ResolveProjectRootDirectory()
	{
		std::error_code ec;
		std::filesystem::path assetsPath(FileSystem::ToDiskPath(FileSystem::ASSETS_DIR));
		std::filesystem::path absAssets = std::filesystem::weakly_canonical(assetsPath, ec);
		if (!ec && absAssets.has_parent_path())
		{
			return absAssets.parent_path().string();
		}
		std::filesystem::path cwd = std::filesystem::current_path(ec);
		return cwd.string();
	}

	void OpenFolderInFileExplorer(const std::string& folderPath)
	{
		std::error_code ec;
		std::filesystem::path absPath = std::filesystem::weakly_canonical(folderPath, ec);
		std::string target = (!ec && !absPath.empty()) ? absPath.string() : folderPath;

		if (!SDL_OpenURL(target.c_str()))
		{
			LOG_ERROR("[UI] No se pudo abrir el explorador de archivos en '%s': %s", target.c_str(), SDL_GetError());
		}
		else
		{
			LOG("[UI] Abriendo explorador de archivos en: %s", target.c_str());
		}
	}
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
	ImNodes::CreateContext();
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

	const std::string& initTitle = Engine::GetInstance().window->GetTitle();
	strncpy_s(windowTitleBuffer, sizeof(windowTitleBuffer), initTitle.c_str(), _TRUNCATE);

	return true;
}

bool ModuleUI::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	SDL_Window* sdlWindow = Engine::GetInstance().window->GetWindow();
	bool relativeMouse = (sdlWindow != nullptr) && SDL_GetWindowRelativeMouseMode(sdlWindow);

	ImGuiIO& io = ImGui::GetIO();

	if (relativeMouse)
	{
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse;

	}
	else
	{
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	return true;
}

bool ModuleUI::Update(float dt)
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	(void)dt;
	bool ret = DrawMainMenuBar();

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
			//ImGuiID dock_right_down = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.45f, nullptr, &dock_right);
			ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);
			ImGuiID dock_game = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.50f, nullptr, &dock_main_id);

			ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
			ImGui::DockBuilderDockWindow("Scene", dock_main_id);
			ImGui::DockBuilderDockWindow("Game", dock_game);
			ImGui::DockBuilderDockWindow("Console", dock_down);
			ImGui::DockBuilderDockWindow("Inspector", dock_right);
			ImGui::DockBuilderDockWindow("Configuration", dock_right);

			ImGui::DockBuilderFinish(dockspace_id);
		}
		layoutInitialized = true;
		resetLayout = false;
	}

	sceneOrHierarchyFocused = false;

	DrawScenePanel();
	DrawGamePanel();
	DrawHierarchyPanel();
	DrawInspectorPanel();
	DrawConfigurationPanel();
	DrawConsolePanel();
	DrawDepthBufferPanel();
	DrawAboutPanel();
	DrawSceneFileModals();

	// Atajos de teclado globales para Escena (Ctrl+N, Ctrl+S, Ctrl+Shift+S, Ctrl+O)
	if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl)
	{
		Scene* scene = Engine::GetInstance().scene.get();
		if (ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			if (ImGui::GetIO().KeyShift)
			{
				TriggerSaveSceneAs();
			}
			else
			{
				TriggerSaveScene();
			}
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_O, false))
		{
			openLoadScenePopup = true;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_N, false))
		{
			if (scene)
			{
				if (!Engine::GetInstance().IsStopped())
				{
					Engine::GetInstance().Stop();
				}
				scene->RequestNewScene();
				saveSceneFileNameBuffer[0] = '\0';
			}
		}
	}

	// Atajo de teclado: Ctrl+D para duplicar la selección si Scene o Hierarchy tienen el foco
	if (sceneOrHierarchyFocused && !ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
	{
		Scene* scene = Engine::GetInstance().scene.get();
		if (scene && scene->HasSelection())
		{
			scene->DuplicateSelection();
		}
	}

	// Atajo de teclado: Supr (Delete) para eliminar la selección si Scene o Hierarchy tienen el foco
	if (sceneOrHierarchyFocused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
	{
		Scene* scene = Engine::GetInstance().scene.get();
		if (scene && scene->HasSelection())
		{
			scene->DestroySelection();
		}
	}

	return ret;
}

bool ModuleUI::DrawMainMenuBar()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	bool continueRunning = true;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			Scene* scene = Engine::GetInstance().scene.get();

			if (ImGui::MenuItem("New Scene", "Ctrl+N"))
			{
				if (scene)
				{
					if (!Engine::GetInstance().IsStopped())
					{
						Engine::GetInstance().Stop();
					}
					scene->RequestNewScene();
					saveSceneFileNameBuffer[0] = '\0';
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
			{
				TriggerSaveScene();
			}
			if (ImGui::IsItemHovered() && scene)
			{
				if (scene->HasSavedScenePath())
				{
					ImGui::SetTooltip("Guardar cambios en: %s", scene->GetCurrentScenePath().c_str());
				}
				else
				{
					ImGui::SetTooltip("Escena aún sin guardar: se pedirá un nombre de archivo");
				}
			}

			if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
			{
				TriggerSaveSceneAs();
			}

			if (ImGui::MenuItem("Load Scene...", "Ctrl+O"))
			{
				if (scene && !scene->GetCurrentScenePath().empty())
				{
					strncpy_s(loadSceneFilePathBuffer, sizeof(loadSceneFilePathBuffer), scene->GetCurrentScenePath().c_str(), _TRUNCATE);
				}
				openLoadScenePopup = true;
			}

			if (ImGui::BeginMenu("Load Saved Scene"))
			{
				std::vector<std::string> savedScenes = FileSystem::ListSavedSceneFiles();
				if (savedScenes.empty())
				{
					ImGui::TextDisabled("No hay escenas guardadas en Assets/Scenes");
				}
				else
				{
					for (size_t i = 0; i < savedScenes.size(); ++i)
					{
						std::filesystem::path sp(savedScenes[i]);
						std::string label = sp.filename().string();
						bool isCurrent = (scene && scene->GetCurrentScenePath() == savedScenes[i]);
						if (ImGui::MenuItem(label.c_str(), nullptr, isCurrent))
						{
							if (scene)
							{
								if (!Engine::GetInstance().IsStopped())
								{
									Engine::GetInstance().Stop();
								}
								scene->RequestLoadScene(savedScenes[i]);
							}
						}
					}
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Open Project in Explorer"))
			{
				OpenFolderInFileExplorer(ResolveProjectRootDirectory());
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Abrir la carpeta raíz del proyecto en el Explorador de Archivos");
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				LOG("[UI] Solicitud de salida desde el menú File -> Exit");
				continueRunning = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Scene", nullptr, &showScene);
			ImGui::MenuItem("Game", nullptr, &showGame);
			ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &showInspector);
			ImGui::MenuItem("Console", nullptr, &showConsole);
			ImGui::MenuItem("Configuration", nullptr, &showConfiguration);
			ImGui::Separator();

			Render* render = Engine::GetInstance().render.get();
			if (render)
			{
				RenderSettings rs = render->GetSettings();
				// Use a local bool for ImGui to manipulate directly
				bool showDepth = rs.showDepthBuffer;
				if (ImGui::MenuItem("Z-Buffer (Depth)", nullptr, &showDepth))
				{
					rs.showDepthBuffer = showDepth;
					render->SetSettings(rs); // Force immediate update back to Engine
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{
				resetLayout = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("GameObject"))
		{
			Scene* scene = Engine::GetInstance().scene.get();
			const bool hasSelection = scene && scene->HasSelection();
			if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D", false, hasSelection))
			{
				if (hasSelection)
				{
					scene->DuplicateSelection();
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Create Empty"))
			{
				if (scene)
				{
					GameObject* created = scene->CreateEmptyGameObject();
					scene->SetSelected(created);
				}
			}
			ImGui::SeparatorText("3D Primitives");
			const PrimitiveType types[] = {
				PrimitiveType::Cube,
				PrimitiveType::Sphere,
				PrimitiveType::Plane,
				PrimitiveType::Cylinder,
				PrimitiveType::Pyramid
			};
			for (PrimitiveType type : types)
			{
				if (ImGui::MenuItem(GeometryGenerator::GetName(type)))
				{
					if (scene)
					{
						GameObject* created = scene->CreatePrimitive(type);
						scene->SetSelected(created);
					}
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("GitHub Documentation"))
			{
				OpenWebURL(REPO_URL_DOCS);
			}
			if (ImGui::MenuItem("Report a Bug"))
			{
				OpenWebURL(REPO_URL_ISSUES);
			}
			if (ImGui::MenuItem("Download Latest"))
			{
				OpenWebURL(REPO_URL_RELEASES);
			}
			ImGui::Separator();
			ImGui::MenuItem("About", nullptr, &showAbout);
			ImGui::EndMenu();
		}

		// =====================================================================
		// BARRA DE TIME MANAGEMENT (PLAY / PAUSE / STEP / STOP + 2 CRONÓMETROS)
		// =====================================================================
		ImGui::Separator();
		Engine& eng = Engine::GetInstance();
		const SimulationState simState = eng.GetSimulationState();

		if (simState == SimulationState::Playing)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.65f, 0.25f, 1.0f));
		}
		if (ImGui::SmallButton(simState == SimulationState::Paused ? "> Resume" : "> Play"))
		{
			eng.Play();
		}
		if (simState == SimulationState::Playing)
		{
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(simState == SimulationState::Stopped);
		if (simState == SimulationState::Paused)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.55f, 0.15f, 1.0f));
		}
		if (ImGui::SmallButton("|| Pause"))
		{
			if (simState == SimulationState::Paused) eng.Play();
			else eng.Pause();
		}
		if (simState == SimulationState::Paused)
		{
			ImGui::PopStyleColor();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(simState != SimulationState::Paused);
		if (ImGui::SmallButton(">| Step"))
		{
			eng.Step();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(simState == SimulationState::Stopped);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
		if (ImGui::SmallButton("[] Stop"))
		{
			eng.Stop();
		}
		ImGui::PopStyleColor();
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::SetNextItemWidth(75.0f);
		float timeScale = eng.GetTimeScale();
		if (ImGui::SliderFloat("##TimeScaleBar", &timeScale, 0.0f, 4.0f, "%.1fx"))
		{
			eng.SetTimeScale(timeScale);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Time Scale del reloj de juego");

		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "Editor: %.2f s", eng.GetEditorTimeSec());
		ImGui::SameLine();
		ImVec4 gameTimerColor = (simState == SimulationState::Playing)
			? ImVec4(0.30f, 1.0f, 0.45f, 1.0f)
			: ((simState == SimulationState::Paused) ? ImVec4(1.0f, 0.85f, 0.25f, 1.0f) : ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
		ImGui::TextColored(gameTimerColor, "| Game: %.2f s", eng.GetGameTimeSec());

		if (Scene* activeScene = eng.scene.get())
		{
			ImGui::SameLine();
			std::string sceneBadge = "| Scene: " + activeScene->GetSceneDisplayName();
			if (activeScene->IsSceneDirty())
			{
				sceneBadge += "*";
				ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.28f, 1.0f), "%s", sceneBadge.c_str());
			}
			else
			{
				ImGui::TextColored(ImVec4(0.65f, 0.90f, 0.65f, 1.0f), "%s", sceneBadge.c_str());
			}
		}

		ImGui::EndMainMenuBar();
	}

	return continueRunning;
}

void ModuleUI::DrawScenePanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	Render* render = Engine::GetInstance().render.get();
	Scene* scene = Engine::GetInstance().scene.get();
	GameObject* mainCamera = scene ? scene->GetMainCameraObject() : nullptr;
	CameraComponent* camComp = mainCamera ? mainCamera->GetComponent<CameraComponent>() : nullptr;

	if (!showScene)
	{
		if (render) render->SetViewportVisible(false);
		if (camComp) camComp->SetViewportHovered(false);
		return;
	}

	bool visible = ImGui::Begin("Scene", &showScene);
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		sceneOrHierarchyFocused = true;
	}

	if (!visible)
	{
		if (render) render->SetViewportVisible(false);
		if (camComp) camComp->SetViewportHovered(false);
		ImGui::End();
		return;
	}

	if (render) render->SetViewportVisible(true);

	if (render)
	{
		RenderSettings rs = render->GetSettings();
		bool barChanged = false;

		if (ImGui::Checkbox("Frustum Culling", &rs.cullFromGameCameraInScene))
		{
			if (rs.cullFromGameCameraInScene)
			{
				rs.frustumCulling = true;
			}
			barChanged = true;
		}
		ImGui::SameLine();
		barChanged |= ImGui::Checkbox("AABB", &rs.drawAABB);
		ImGui::SameLine();
		barChanged |= ImGui::Checkbox("Octree", &rs.drawOctree);

		if (barChanged)
		{
			render->SetSettings(rs);
		}

		const CullingStats& cStats = render->GetCullingStats();
		ImGui::SameLine();
		ImGui::TextDisabled("| Visibles: %d/%d (Culled: %d)", cStats.visibleObjects, cStats.totalMeshObjects, cStats.culledObjects);
		ImGui::Separator();
	}

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	if (render && viewportSize.x > 0 && viewportSize.y > 0)
	{
		render->SetViewportSize((int)viewportSize.x, (int)viewportSize.y);
	}

	unsigned int sceneTexture = render ? render->GetSceneTexture() : 0;
	bool imageHovered = false;
	if (sceneTexture != 0 && viewportSize.x > 0 && viewportSize.y > 0)
	{
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		ImGui::Image((ImTextureID)(intptr_t)sceneTexture, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
		imageHovered = ImGui::IsItemHovered();

		Input* input = Engine::GetInstance().input.get();
		bool altHeld = input && (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT ||
			input->GetKey(SDL_SCANCODE_RALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_RALT) == KEY_REPEAT);

		// Selección por Raycast al hacer clic izquierdo sobre el viewport (sin Alt para no interferir con la órbita)
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !altHeld && camComp)
		{
			ImVec2 mousePos = ImGui::GetMousePos();
			float localMouseX = mousePos.x - cursorScreenPos.x;
			float localMouseY = mousePos.y - cursorScreenPos.y;
			camComp->PerformRaycast(localMouseX, localMouseY, viewportSize.x, viewportSize.y);
		}
	}

	if (camComp)
	{
		bool mouseDisabledForCamera = (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouse) != 0;
		if (camComp->IsControllingCamera())
		{
			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
			camComp->SetViewportHovered(true);
		}
		else if (mouseDisabledForCamera)
		{
			camComp->SetViewportHovered(true);
		}
		else
		{
			camComp->SetViewportHovered(imageHovered || ImGui::IsWindowHovered());
		}
	}

	ImGui::End();
}

void ModuleUI::DrawGamePanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	Render* render = Engine::GetInstance().render.get();

	if (!showGame)
	{
		if (render) render->SetGameViewportVisible(false);
		return;
	}

	bool visible = ImGui::Begin("Game", &showGame);
	if (!visible)
	{
		if (render) render->SetGameViewportVisible(false);
		ImGui::End();
		return;
	}

	if (render) render->SetGameViewportVisible(true);

	Scene* scene = Engine::GetInstance().scene.get();
	GameObject* gameCam = scene ? scene->GetGameCameraObject() : nullptr;

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	if (render && viewportSize.x > 0 && viewportSize.y > 0)
	{
		render->SetGameViewportSize((int)viewportSize.x, (int)viewportSize.y);
	}

	if (!gameCam || !gameCam->IsActive() || !gameCam->GetComponent<CameraComponent>())
	{
		ImGui::TextDisabled("Display 1: No cameras rendering.");
		ImGui::End();
		return;
	}

	unsigned int gameTexture = render ? render->GetGameTexture() : 0;
	if (gameTexture != 0 && viewportSize.x > 0 && viewportSize.y > 0)
	{
		ImGui::Image((ImTextureID)(intptr_t)gameTexture, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
	}

	ImGui::End();
}

void ModuleUI::DrawHierarchyPanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!showHierarchy) return;

	if (ImGui::Begin("Hierarchy", &showHierarchy))
	{
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			sceneOrHierarchyFocused = true;
		}

		Scene* scene = Engine::GetInstance().scene.get();
		GameObject* selected = scene ? scene->GetSelected() : nullptr;

		if (scene)
		{
			// Shift + clic usa el orden de filas del frame anterior (el actual aún se está dibujando)
			previousHierarchyRows.swap(hierarchyRows);
			hierarchyRows.clear();

			const uint64_t currentSelectionRev = scene->GetSelectionRevision();
			if (currentSelectionRev != lastHierarchySelectionRevision)
			{
				lastHierarchySelectionRevision = currentSelectionRev;
				expandToSelectedInHierarchy = (selected != nullptr);
				scrollToSelectedInHierarchy = (selected != nullptr);
			}

			GameObject* root = scene->GetRoot();
			if (root)
			{
				struct HierarchyStackItem
				{
					GameObject* node;
					bool isTreePopMarker;
				};

				std::stack<HierarchyStackItem> stack;
				const std::vector<GameObject*> rootChildren = root->GetChildren();
				for (size_t i = rootChildren.size(); i > 0; --i)
				{
					GameObject* child = rootChildren[i - 1];
					if (child && child->GetParent() == root)
					{
						stack.push({ child, false });
					}
				}

				while (!stack.empty())
				{
					HierarchyStackItem item = stack.top();
					stack.pop();

					if (item.isTreePopMarker)
					{
						ImGui::TreePop();
						continue;
					}

					GameObject* node = item.node;
					if (!node) continue;

					const std::vector<GameObject*> children = node->GetChildren();
					const bool hasChildren = !children.empty();

					bool nodeOpen = DrawHierarchyNode(node, selected, hasChildren);
					if (nodeOpen && hasChildren)
					{
						stack.push({ nullptr, true });
						for (size_t i = children.size(); i > 0; --i)
						{
							GameObject* child = children[i - 1];
							if (child && child->GetParent() == node)
							{
								stack.push({ child, false });
							}
						}
					}
				}
			}

			expandToSelectedInHierarchy = false;
			scrollToSelectedInHierarchy = false;
		}

		// Zona vacía inferior: soltar aquí reemparenta al nodo raíz de la escena
		ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.y > 0.0f)
		{
			ImGui::Dummy(avail);
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_GAMEOBJECT"))
				{
					GameObject* droppedNode = *(GameObject**)payload->Data;
					if (droppedNode && scene)
					{
						droppedNode->SetParent(scene->GetRoot());
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
	}
	ImGui::End();
}

bool ModuleUI::DrawHierarchyNode(GameObject* node, GameObject* selected, bool hasChildren)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!node) return false;

	Scene* currentScene = Engine::GetInstance().scene.get();
	hierarchyRows.push_back((uint64_t)node->GetUUID());

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (currentScene && currentScene->IsSelected(node))
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (!hasChildren)
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	// Si el objeto seleccionado es descendiente de este nodo (hijo, nieto, etc.), desplegar automáticamente esta rama
	if (expandToSelectedInHierarchy && selected != nullptr && hasChildren && selected->IsDescendantOf(node))
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	if (!node->IsActive())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	}

	bool nodeOpen = ImGui::TreeNodeEx((void*)node, flags, "%s", node->GetName().c_str());

	if (!node->IsActive())
	{
		ImGui::PopStyleColor();
	}

	// Si acabamos de seleccionar este objeto (por ejemplo haciendo clic en él en la vista 3D Scene), asegurar que sea visible en Hierarchy
	if (scrollToSelectedInHierarchy && selected == node)
	{
		if (!ImGui::IsItemVisible())
		{
			ImGui::SetScrollHereY(0.5f);
		}
		scrollToSelectedInHierarchy = false;
	}

	// 1. Selección con clic izquierdo (si no se ha pulsado únicamente la flecha para plegar/desplegar)
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		if (Engine::GetInstance().scene)
		{
			HierarchyRowClicked(node);
			lastHierarchySelectionRevision = Engine::GetInstance().scene->GetSelectionRevision();
			expandToSelectedInHierarchy = false;
			scrollToSelectedInHierarchy = false;
		}
	}

	// 2. Drag & Drop: Origen de arrastre
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		ImGui::SetDragDropPayload("HIERARCHY_GAMEOBJECT", &node, sizeof(GameObject*));
		ImGui::Text("Mover '%s'", node->GetName().c_str());
		ImGui::EndDragDropSource();
	}

	// 3. Drag & Drop: Destino de soltado (reemparentar, reordenar entre hermanos o copiar componente)
	if (ImGui::BeginDragDropTarget())
	{
		// Cuarto superior / inferior de la fila = antes / después de 'node'; centro = hijo de 'node'.
		// Abajo de un nodo desplegado con hijos se trata como centro, para no confundirlo con su primer hijo.
		const ImVec2 rowMin = ImGui::GetItemRectMin();
		const ImVec2 rowMax = ImGui::GetItemRectMax();
		const float rowHeight = rowMax.y - rowMin.y;
		const float mouseY = ImGui::GetMousePos().y;
		int dropZone = 0;
		if (mouseY < rowMin.y + rowHeight * 0.25f) dropZone = -1;
		else if (mouseY > rowMax.y - rowHeight * 0.25f && !(nodeOpen && hasChildren)) dropZone = 1;

		ImGuiDragDropFlags dropFlags = ImGuiDragDropFlags_AcceptBeforeDelivery;
		if (dropZone != 0) dropFlags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_GAMEOBJECT", dropFlags))
		{
			GameObject* droppedNode = *(GameObject**)payload->Data;
			const bool validDrop = droppedNode && droppedNode != node && !node->IsDescendantOf(droppedNode);

			if (validDrop && dropZone != 0)
			{
				const float lineY = (dropZone < 0) ? rowMin.y : rowMax.y;
				ImGui::GetWindowDrawList()->AddLine(ImVec2(rowMin.x, lineY), ImVec2(rowMax.x, lineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
			}

			if (validDrop && payload->IsDelivery())
			{
				if (dropZone == 0)
				{
					droppedNode->SetParent(node);
				}
				else if (droppedNode->SetParent(node->GetParent()))
				{
					// Al sacarlo de su posición, los hermanos que tenía detrás suben un puesto
					const int droppedIndex = droppedNode->GetSiblingIndex();
					int insertIndex = node->GetSiblingIndex() + (dropZone > 0 ? 1 : 0);
					if (droppedIndex < insertIndex) insertIndex -= 1;
					droppedNode->SetSiblingIndex(insertIndex);
				}
			}
		}
		if (const ImGuiPayload* compPayload = ImGui::AcceptDragDropPayload("INSPECTOR_COMPONENT"))
		{
			const ComponentDragPayload* data = static_cast<const ComponentDragPayload*>(compPayload->Data);
			if (data && data->sourceObject)
			{
				const Component* srcComp = data->sourceObject->GetComponentByType(data->type);
				if (srcComp)
				{
					node->AddComponent(srcComp->Clone(node));
					LOG("[UI] Componente '%s' copiado a '%s'", GetComponentTypeName(data->type), node->GetName().c_str());
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 4. Menú contextual con clic derecho
	if (ImGui::BeginPopupContextItem())
	{
		Scene* scene = Engine::GetInstance().scene.get();

		if (ImGui::MenuItem("Create Empty Child"))
		{
			if (scene)
			{
				GameObject* child = scene->CreateEmptyGameObject(node);
				scene->SetSelected(child);
			}
		}
		// Click Izquierdo en un objeto
		if (ImGui::BeginMenu("Create Primitive Child"))
		{
			const PrimitiveType types[] = {
				PrimitiveType::Cube,
				PrimitiveType::Sphere,
				PrimitiveType::Plane,
				PrimitiveType::Cylinder,
				PrimitiveType::Pyramid
			};
			for (PrimitiveType type : types)
			{
				if (ImGui::MenuItem(GeometryGenerator::GetName(type)))
				{
					if (scene)
					{
						GameObject* child = scene->CreatePrimitive(type, node);
						scene->SetSelected(child);
					}
				}
			}
			ImGui::EndMenu();
		}

		// Sobre un objeto seleccionado actúan sobre toda la selección; si no, solo sobre él
		if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
		{
			if (scene)
			{
				if (scene->IsSelected(node)) scene->DuplicateSelection();
				else scene->DuplicateGameObject(node);
			}
		}

		if (copiedComponent != nullptr && ImGui::MenuItem("Paste Component"))
		{
			node->AddComponent(copiedComponent->Clone(node));
			LOG("[UI] Componente '%s' pegado en '%s'", GetComponentTypeName(copiedComponent->GetType()), node->GetName().c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Delete", "Supr"))
		{
			if (scene)
			{
				if (scene->IsSelected(node)) scene->DestroySelection();
				else scene->DestroyGameObject(node);
			}
		}

		ImGui::EndPopup();
	}

	return nodeOpen;
}

void ModuleUI::HierarchyRowClicked(GameObject* node)
{
	Scene* scene = Engine::GetInstance().scene.get();
	if (!scene || !node) return;

	const ImGuiIO& io = ImGui::GetIO();
	const uint64_t uid = (uint64_t)node->GetUUID();

	if (io.KeyShift)
	{
		// Rango entre el ancla (último clic sin Shift) y esta fila; con Ctrl se suma a la selección
		const auto anchorIt = std::find(previousHierarchyRows.begin(), previousHierarchyRows.end(), hierarchyRangeAnchorUID);
		const auto nodeIt = std::find(previousHierarchyRows.begin(), previousHierarchyRows.end(), uid);
		if (anchorIt != previousHierarchyRows.end() && nodeIt != previousHierarchyRows.end())
		{
			std::vector<GameObject*> range;
			if (io.KeyCtrl) range = scene->GetSelection();
			for (auto it = std::min(anchorIt, nodeIt); it <= std::max(anchorIt, nodeIt); ++it)
			{
				if (GameObject* object = scene->FindByUID(*it)) range.push_back(object);
			}
			scene->SetSelection(range, node);
			return;
		}
	}

	if (io.KeyCtrl) scene->ToggleSelection(node);
	else scene->Select(node);
	hierarchyRangeAnchorUID = uid;
}

void ModuleUI::DrawInspectorPanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!showInspector) return;

	if (ImGui::Begin("Inspector", &showInspector))
	{
		Scene* scene = Engine::GetInstance().scene.get();
		GameObject* selectedGameObject = scene ? scene->GetSelected() : nullptr;

		if (selectedGameObject)
		{
			// Con varios seleccionados se edita solo el activo (el último en seleccionarse)
			const size_t selectionCount = scene->GetSelection().size();
			if (selectionCount > 1)
			{
				ImGui::TextDisabled("%zu objetos seleccionados: se edita '%s'", selectionCount, selectedGameObject->GetName().c_str());
				ImGui::Separator();
			}

			bool isActive = selectedGameObject->IsActive();
			if (ImGui::Checkbox("##ActiveGO", &isActive))
			{
				if (isActive) selectedGameObject->Enable();
				else selectedGameObject->Disable();
				if (scene) scene->MarkSpacePartitioningDirty();
			}
			ImGui::SameLine();

			char nameBuf[128] = "";
			strncpy_s(nameBuf, sizeof(nameBuf), selectedGameObject->GetName().c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 85.0f);
			if (ImGui::InputText("##GOName", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (nameBuf[0] != '\0')
				{
					selectedGameObject->SetName(nameBuf);
				}
			}
			ImGui::SameLine();

			bool isStatic = selectedGameObject->IsStatic();
			if (ImGui::Checkbox("Static", &isStatic))
			{
				selectedGameObject->SetStatic(isStatic, true);
				if (scene)
				{
					scene->RebuildSpacePartitioning();
				}
			}

			ImGui::Separator();

			// Recolectar Meshes y Materials del subárbol usando std::stack (sin recursión)
			std::vector<MeshComponent*> meshesToInspect;
			std::vector<MaterialComponent*> materialsToInspect;
			{
				std::stack<GameObject*> stack;
				stack.push(selectedGameObject);
				while (!stack.empty())
				{
					GameObject* node = stack.top();
					stack.pop();
					if (!node) continue;

					if (MeshComponent* m = node->GetComponent<MeshComponent>())
					{
						if (m->GetPrimitive()) meshesToInspect.push_back(m);
					}
					if (MaterialComponent* mat = node->GetComponent<MaterialComponent>())
					{
						materialsToInspect.push_back(mat);
					}

					const std::vector<GameObject*>& children = node->GetChildren();
					for (size_t i = children.size(); i > 0; --i)
					{
						stack.push(children[i - 1]);
					}
				}
			}

			// Construir lista ordenada de componentes visibles según el orden del GameObject
			std::vector<ComponentType> visibleComponents;
			const std::array<ComponentType, COMPONENT_COUNT>& order = selectedGameObject->GetComponentOrder();
			for (ComponentType type : order)
			{
				bool present = (selectedGameObject->GetComponentByType(type) != nullptr);
				if (!present && type == COMPONENT_MESH && !meshesToInspect.empty()) present = true;
				if (!present && type == COMPONENT_MATERIAL && !materialsToInspect.empty()) present = true;
				if (present)
				{
					visibleComponents.push_back(type);
				}
			}

			auto resolveTargetComponent = [&](ComponentType type) -> Component*
			{
				if (Component* direct = selectedGameObject->GetComponentByType(type))
				{
					return direct;
				}
				if (type == COMPONENT_MESH && !meshesToInspect.empty()) return meshesToInspect[0];
				if (type == COMPONENT_MATERIAL && !materialsToInspect.empty()) return materialsToInspect[0];
				return nullptr;
			};

			auto removeComponentFromSelected = [&](ComponentType type)
			{
				if (type == COMPONENT_TRANSFORM)
				{
					LOG_WARNING("[UI] No se puede eliminar TransformComponent.");
					return;
				}
				if (selectedGameObject->RemoveComponent(type))
				{
					if (scene) scene->MarkSpacePartitioningDirty();
					LOG("[UI] Componente '%s' eliminado de '%s'", GetComponentTypeName(type), selectedGameObject->GetName().c_str());
					return;
				}
				// Si es un objeto contenedor padre cuyas partes hijas poseen el componente, eliminarlo de los hijos con un stack
				bool removedFromChildren = false;
				std::stack<GameObject*> stack;
				stack.push(selectedGameObject);
				while (!stack.empty())
				{
					GameObject* node = stack.top();
					stack.pop();
					if (!node) continue;
					if (node->RemoveComponent(type))
					{
						removedFromChildren = true;
					}
					const std::vector<GameObject*>& children = node->GetChildren();
					for (size_t i = children.size(); i > 0; --i)
					{
						stack.push(children[i - 1]);
					}
				}
				if (removedFromChildren)
				{
					if (scene) scene->MarkSpacePartitioningDirty();
					LOG("[UI] Componente '%s' eliminado de las sub-partes de '%s'", GetComponentTypeName(type), selectedGameObject->GetName().c_str());
				}
			};

			auto pasteComponentToSelected = [&](const Component* sourceComp)
			{
				if (!sourceComp) return;
				ComponentType cType = sourceComp->GetType();
				if (cType == COMPONENT_MATERIAL && selectedGameObject->GetComponent<MaterialComponent>() == nullptr && !materialsToInspect.empty())
				{
					for (MaterialComponent* m : materialsToInspect)
					{
						if (GameObject* ownerNode = m->GetOwner())
						{
							ownerNode->AddComponent(sourceComp->Clone(ownerNode));
						}
					}
				}
				else
				{
					selectedGameObject->AddComponent(sourceComp->Clone(selectedGameObject));
				}
				if (scene) scene->MarkSpacePartitioningDirty();
				LOG("[UI] Componente '%s' pegado en '%s'", GetComponentTypeName(cType), selectedGameObject->GetName().c_str());
			};

			bool orderChangedOrRemoved = false;

			for (size_t compIdx = 0; compIdx < visibleComponents.size(); ++compIdx)
			{
				if (orderChangedOrRemoved) break;

				ComponentType compType = visibleComponents[compIdx];
				Component* targetComp = resolveTargetComponent(compType);

				ImGui::PushID((int)compType);

				// Checkbox de activación para componentes que no sean Transform
				if (targetComp && compType != COMPONENT_TRANSFORM)
				{
					bool compActive = targetComp->IsActive();
					if (ImGui::Checkbox("##CompActive", &compActive))
					{
						if (compActive) targetComp->Enable();
						else targetComp->Disable();

						if (compType == COMPONENT_MESH)
						{
							for (MeshComponent* m : meshesToInspect)
							{
								if (compActive) m->Enable();
								else m->Disable();
							}
							if (scene) scene->MarkSpacePartitioningDirty();
						}
						else if (compType == COMPONENT_MATERIAL)
						{
							for (MaterialComponent* m : materialsToInspect)
							{
								if (compActive) m->Enable();
								else m->Disable();
							}
						}
					}
					ImGui::SameLine();
				}

				bool headerOpen = ImGui::CollapsingHeader(
					GetComponentTypeName(compType),
					ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap
				);

				// Drag & Drop Source en la cabecera del componente (para mover de orden o arrastrar a otro GameObject)
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
				{
					ComponentDragPayload payload{ selectedGameObject, compType };
					ImGui::SetDragDropPayload("INSPECTOR_COMPONENT", &payload, sizeof(ComponentDragPayload));
					ImGui::Text("Mover / Copiar '%s'", GetComponentTypeName(compType));
					ImGui::EndDragDropSource();
				}

				// Drag & Drop Target en la cabecera del componente (reordenar soltando encima de otro componente)
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("INSPECTOR_COMPONENT"))
					{
						const ComponentDragPayload* data = static_cast<const ComponentDragPayload*>(payload->Data);
						if (data)
						{
							if (data->sourceObject == selectedGameObject && data->type != compType)
							{
								selectedGameObject->SwapComponentOrder(data->type, compType);
								orderChangedOrRemoved = true;
							}
							else if (data->sourceObject && data->sourceObject != selectedGameObject)
							{
								if (const Component* foreignComp = data->sourceObject->GetComponentByType(data->type))
								{
									pasteComponentToSelected(foreignComp);
									orderChangedOrRemoved = true;
								}
							}
						}
					}
					ImGui::EndDragDropTarget();
				}

				// Menú contextual al hacer clic derecho sobre la cabecera del componente
				if (ImGui::BeginPopupContextItem("ComponentHeaderContextMenu"))
				{
					if (ImGui::MenuItem("Move Up", nullptr, false, compIdx > 0))
					{
						selectedGameObject->SwapComponentOrder(compType, visibleComponents[compIdx - 1]);
						orderChangedOrRemoved = true;
					}
					if (ImGui::MenuItem("Move Down", nullptr, false, compIdx + 1 < visibleComponents.size()))
					{
						selectedGameObject->SwapComponentOrder(compType, visibleComponents[compIdx + 1]);
						orderChangedOrRemoved = true;
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Copy Component", nullptr, false, targetComp != nullptr))
					{
						copiedComponent = targetComp->Clone(nullptr);
						LOG("[UI] Componente '%s' copiado al portapapeles", GetComponentTypeName(compType));
					}
					if (ImGui::MenuItem("Paste Component", nullptr, false, copiedComponent != nullptr))
					{
						pasteComponentToSelected(copiedComponent.get());
						orderChangedOrRemoved = true;
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Remove Component", nullptr, false, compType != COMPONENT_TRANSFORM))
					{
						removeComponentFromSelected(compType);
						orderChangedOrRemoved = true;
					}
					ImGui::EndPopup();
				}

				// Botones rápidos alineados a la derecha en la cabecera: [^] [v] [C] [P] [X]
				const bool canRemove = (compType != COMPONENT_TRANSFORM);
				const bool canPasteHere = (copiedComponent != nullptr && copiedComponent->GetType() == compType);
				float buttonsWidth = 72.0f + (canPasteHere ? 22.0f : 0.0f) + (canRemove ? 22.0f : 0.0f);
				float availWidth = ImGui::GetWindowContentRegionMax().x;
				ImGui::SameLine( std::max(140.0f, availWidth - buttonsWidth) );

				ImGui::BeginDisabled(compIdx == 0);
				if (ImGui::SmallButton("^"))
				{
					selectedGameObject->SwapComponentOrder(compType, visibleComponents[compIdx - 1]);
					orderChangedOrRemoved = true;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Mover arriba");
				ImGui::EndDisabled();

				ImGui::SameLine(0.0f, 2.0f);
				ImGui::BeginDisabled(compIdx + 1 >= visibleComponents.size());
				if (ImGui::SmallButton("v"))
				{
					selectedGameObject->SwapComponentOrder(compType, visibleComponents[compIdx + 1]);
					orderChangedOrRemoved = true;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Mover abajo");
				ImGui::EndDisabled();

				ImGui::SameLine(0.0f, 2.0f);
				ImGui::BeginDisabled(targetComp == nullptr);
				if (ImGui::SmallButton("C"))
				{
					if (targetComp)
					{
						copiedComponent = targetComp->Clone(nullptr);
						LOG("[UI] Componente '%s' copiado al portapapeles", GetComponentTypeName(compType));
					}
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Copiar componente");
				ImGui::EndDisabled();

				if (canPasteHere)
				{
					ImGui::SameLine(0.0f, 2.0f);
					if (ImGui::SmallButton("P"))
					{
						pasteComponentToSelected(copiedComponent.get());
						orderChangedOrRemoved = true;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pegar valores del componente");
				}

				if (canRemove)
				{
					ImGui::SameLine(0.0f, 2.0f);
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
					if (ImGui::SmallButton("X"))
					{
						removeComponentFromSelected(compType);
						orderChangedOrRemoved = true;
					}
					ImGui::PopStyleColor(2);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Eliminar componente");
				}

				if (orderChangedOrRemoved)
				{
					ImGui::PopID();
					break;
				}

				if (headerOpen)
				{
					// =====================================================================
					// 1. TRANSFORM COMPONENT
					// =====================================================================
					if (compType == COMPONENT_TRANSFORM)
					{
						TransformComponent* transform = selectedGameObject->GetComponent<TransformComponent>();
						if (transform)
						{
							const bool isObjectStatic = selectedGameObject->IsStatic();
							if (isObjectStatic)
							{
								ImGui::TextDisabled("Objeto Static: Transform bloqueado.");
							}

							ImGui::BeginDisabled(isObjectStatic);
							if (ImGui::BeginTable("TransformTable", 3, ImGuiTableFlags_SizingFixedFit))
							{
								ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 75.0f);
								ImGui::TableSetupColumn("FixAllValue", ImGuiTableColumnFlags_WidthFixed, 25.0f);
								ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

								glm::vec3 pos = transform->GetPosition();
								if (DrawVec3Control("Position", pos, 0.1f))
								{
									transform->SetPosition(pos);
									if (scene) scene->MarkSpacePartitioningDirty();
								}

								glm::vec3 rot = transform->GetRotation();
								if (DrawVec3Control("Rotation", rot, 1.0f))
								{
									transform->SetRotation(rot);
									if (scene) scene->MarkSpacePartitioningDirty();
								}

								////////////////////Scale
								glm::vec3 scl = transform->GetScale();
								glm::vec3 ogscl = transform->GetScale();

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::AlignTextToFramePadding();
								ImGui::TextUnformatted("Scale");

								ImGui::TableSetColumnIndex(1);

								static bool ScaleFixed = false;

								if (ImGui::Button("&", ImVec2(ImGui::GetItemRectSize().x, 0.0f)))
								{
									ScaleFixed = !ScaleFixed;
								}

								ImGui::TableSetColumnIndex(2);

								ImGui::PushItemWidth(-1.0f);
								if (ImGui::DragFloat3("", glm::value_ptr(scl), 0.01f))
								{
									if (ScaleFixed)
									{
										if (scl.x != ogscl.x)
										{
											transform->SetScale(glm::vec3(scl.x, scl.x, scl.x));
										}
										else if (scl.y != ogscl.y)
										{
											transform->SetScale(glm::vec3(scl.y, scl.y, scl.y));
										}
										else
										{
											transform->SetScale(glm::vec3(scl.z, scl.z, scl.z));
										}
									}
									else
									{
										transform->SetScale(scl);
									}
									if (scene) scene->MarkSpacePartitioningDirty();
								}
								ImGui::PopItemWidth();

								ImGui::EndTable();
							}

							if (ImGui::Button("Reset Transform", ImVec2(ImGui::GetItemRectSize().x, 0.0f)))
							{
								transform->SetPosition(glm::vec3(0.0f));
								transform->SetRotation(glm::vec3(0.0f));
								transform->SetScale(glm::vec3(1.0f));
								if (scene) scene->MarkSpacePartitioningDirty();
							}
							ImGui::EndDisabled();
						}
					}
					// =====================================================================
					// 2. MESH COMPONENT
					// =====================================================================
					else if (compType == COMPONENT_MESH && !meshesToInspect.empty())
					{
						size_t totalVertices = 0;
						size_t totalTriangles = 0;
						size_t totalIndices = 0;
						glm::vec3 bMin(FLT_MAX);
						glm::vec3 bMax(-FLT_MAX);

						for (MeshComponent* m : meshesToInspect)
						{
							const Primitive* prim = m->GetPrimitive();
							if (prim)
							{
								totalVertices += prim->GetVertexCount();
								totalTriangles += prim->GetTriangleCount();
								totalIndices += prim->GetIndices().size();
								bMin = glm::min(bMin, prim->GetBoundsMin());
								bMax = glm::max(bMax, prim->GetBoundsMax());
							}
						}

						if (meshesToInspect.size() > 1)
						{
							ImGui::BulletText("Sub-mallas (Partes): %zu", meshesToInspect.size());
						}
						ImGui::BulletText("Vértices: %zu", totalVertices);
						ImGui::BulletText("Triángulos (Caras): %zu", totalTriangles);
						ImGui::BulletText("Índices: %zu", totalIndices);
						ImGui::BulletText("AABB Min: (%.2f, %.2f, %.2f)", bMin.x, bMin.y, bMin.z);
						ImGui::BulletText("AABB Max: (%.2f, %.2f, %.2f)", bMax.x, bMax.y, bMax.z);

						// Las opciones son del asset (su .meta): al reimportar cambian todas sus instancias
						ResourceManager* resources = Engine::GetInstance().resources.get();
						const ResourceMesh* meshRes = meshesToInspect[0]->GetMeshResource();
						if (resources && meshRes && !meshRes->IsBuiltIn() && !meshRes->GetAssetPath().empty())
						{
							const std::string& assetPath = meshRes->GetAssetPath();
							ImGui::Spacing();
							ImGui::SeparatorText("Opciones de Importado (Modelo)");
							ImGui::TextDisabled("%s", assetPath.c_str());

							static ModelImportSettings editModelOpts;
							static std::string lastModelAsset;
							if (lastModelAsset != assetPath)
							{
								editModelOpts = ModelImportSettings();
								resources->GetModelImportSettings(assetPath, editModelOpts);
								lastModelAsset = assetPath;
							}

							ImGui::DragFloat("Escala Global", &editModelOpts.globalScale, 0.01f, 0.001f, 100.0f, "%.3f");
							ImGui::Checkbox("Convertir unidades del FBX a metros", &editModelOpts.convertUnits);

							static const char* upAxes[] = { "+Y arriba", "+Z arriba", "+X arriba", "-Z arriba", "-X arriba", "-Y arriba" };
							ImGui::Combo("Eje vertical", &editModelOpts.upAxis, upAxes, IM_ARRAYSIZE(upAxes));
							ImGui::Checkbox("Espejo X", &editModelOpts.mirrorX);
							ImGui::SameLine();
							ImGui::Checkbox("Espejo Z", &editModelOpts.mirrorZ);

							ImGui::Checkbox("Ignorar Camaras", &editModelOpts.ignoreCameras);
							ImGui::SameLine();
							ImGui::Checkbox("Ignorar Luces", &editModelOpts.ignoreLights);

							ImGui::TextDisabled("Afecta a todas las instancias del modelo.\nEscala y ejes se aplican al volver a colocarlo en la escena.");
							if (ImGui::Button("Reimportar Modelo"))
							{
								resources->SetModelImportSettings(assetPath, editModelOpts);
								if (scene) scene->MarkSpacePartitioningDirty();
							}
						}

						ImGui::Spacing();
						ImGui::SeparatorText("Visualización de Normales");

						Render* render = Engine::GetInstance().render.get();
						if (render)
						{
							RenderSettings rs = render->GetSettings();
							bool vertexNormals = (rs.normals == NormalsMode::Vertex || rs.normals == NormalsMode::Both);
							bool faceNormals = (rs.normals == NormalsMode::Face || rs.normals == NormalsMode::Both);

							bool changed = false;
							changed |= ImGui::Checkbox("Mostrar Normales por Vértice (Amarillo)", &vertexNormals);
							changed |= ImGui::Checkbox("Mostrar Normales por Cara (Cian)", &faceNormals);
							changed |= ImGui::Checkbox("Aplicar a todos los objetos de la escena", &rs.normalsAllObjects);

							ImGui::Spacing();
							ImGui::SeparatorText("Visualización de Volúmenes (Space Partitioning)");
							changed |= ImGui::Checkbox("Dibujar AABB (Verde)", &rs.drawAABB);
							changed |= ImGui::Checkbox("Dibujar Octree (Cian)", &rs.drawOctree);

							if (changed)
							{
								if (vertexNormals && faceNormals) rs.normals = NormalsMode::Both;
								else if (vertexNormals) rs.normals = NormalsMode::Vertex;
								else if (faceNormals) rs.normals = NormalsMode::Face;
								else rs.normals = NormalsMode::None;

								render->SetSettings(rs);
							}
						}
					}
					// =====================================================================
					// 3. MATERIAL / TEXTURE COMPONENT
					// =====================================================================
					else if (compType == COMPONENT_MATERIAL && !materialsToInspect.empty())
					{
						MaterialComponent* primaryMat = materialsToInspect[0];

						glm::vec4 color = primaryMat->GetColor();
						if (ImGui::ColorEdit4("Color Base (Alpha)", glm::value_ptr(color), ImGuiColorEditFlags_PickerHueWheel))
						{
							for (MaterialComponent* m : materialsToInspect)
							{
								m->SetColor(color);
							}
						}

						ImGui::Spacing();
						bool usingTexture = primaryMat->GetUseTexture();
						if (ImGui::Checkbox("Usar Textura Difusa", &usingTexture))
						{
							for (MaterialComponent* m : materialsToInspect)
							{
								m->SetUseTexture(usingTexture);
							}
						}

						bool usingChecker = primaryMat->GetUseCheckerboard();
						if (ImGui::Checkbox("Textura Checkerboard (Blanco/Negro)", &usingChecker))
						{
							for (MaterialComponent* m : materialsToInspect)
							{
								m->SetUseCheckerboard(usingChecker);
							}
						}

						ImGui::Spacing();
						ImGui::SeparatorText("Datos de Textura");
						const ResourceTexture* texRes = primaryMat->GetTexture();
						ResourceManager* resources = Engine::GetInstance().resources.get();
						if (texRes)
						{
							ImGui::BulletText("Asset: %s", texRes->GetAssetPath().c_str());
							ImGui::BulletText("Tamaño: %d x %d px", texRes->GetWidth(), texRes->GetHeight());
							ImGui::BulletText("OpenGL Texture ID: %u", texRes->GetGLID());

							if (resources && !texRes->GetAssetPath().empty())
							{
								const std::string& assetPath = texRes->GetAssetPath();
								ImGui::Spacing();
								ImGui::SeparatorText("Opciones de Importado (Textura)");

								static TextureImportSettings editTexOpts;
								static std::string lastTextureAsset;
								if (lastTextureAsset != assetPath)
								{
									editTexOpts = TextureImportSettings();
									resources->GetTextureImportSettings(assetPath, editTexOpts);
									lastTextureAsset = assetPath;
								}
								bool importChanged = false;

								static const char* minFilters[] = {
									"GL_NEAREST (Vecino más cercano)",
									"GL_LINEAR (Bilineal)",
									"GL_LINEAR_MIPMAP_LINEAR (Trilineal)"
								};
								importChanged |= ImGui::Combo("Filtro Minificación", &editTexOpts.minFilter, minFilters, IM_ARRAYSIZE(minFilters));

								static const char* magFilters[] = {
									"GL_NEAREST (Pixelado)",
									"GL_LINEAR (Suave)"
								};
								importChanged |= ImGui::Combo("Filtro Magnificación", &editTexOpts.magFilter, magFilters, IM_ARRAYSIZE(magFilters));

								static const char* wrapModes[] = {
									"GL_REPEAT (Repetición)",
									"GL_CLAMP_TO_EDGE (Abrazar bordes)",
									"GL_MIRRORED_REPEAT (Espejado)"
								};
								importChanged |= ImGui::Combo("Modo Wrap (S/T)", &editTexOpts.wrapMode, wrapModes, IM_ARRAYSIZE(wrapModes));

								importChanged |= ImGui::Checkbox("Flip Horizontal (X)", &editTexOpts.flipX);
								ImGui::SameLine();
								importChanged |= ImGui::Checkbox("Flip Vertical (Y)", &editTexOpts.flipY);

								static const int maxSizes[] = { 256, 512, 1024, 2048, 4096, 8192 };
								static const char* maxSizeNames[] = { "256", "512", "1024", "2048", "4096", "8192" };
								int sizeIndex = 3;
								for (int i = 0; i < IM_ARRAYSIZE(maxSizes); ++i) if (maxSizes[i] == editTexOpts.maxSize) sizeIndex = i;
								if (ImGui::Combo("Tamaño máximo", &sizeIndex, maxSizeNames, IM_ARRAYSIZE(maxSizeNames)))
								{
									editTexOpts.maxSize = maxSizes[sizeIndex];
									importChanged = true;
								}

								ImGui::TextDisabled("Afecta a todos los materiales que usan esta textura.");
								if (importChanged)
								{
									resources->SetTextureImportSettings(assetPath, editTexOpts);
								}
							}
						}
						else if (primaryMat->IsTextureMissing())
						{
							ImGui::TextDisabled("La textura asignada ya no existe: se muestra la textura missing.");
						}
						else
						{
							ImGui::TextDisabled("Sin textura difusa asignada (arrastra un PNG/JPG/DDS a la ventana).");
						}

						unsigned int activeId = primaryMat->GetActiveTextureID();
						if (activeId != 0)
						{
							ImGui::Spacing();
							ImGui::TextDisabled("Vista previa activa:");
							ImGui::Image((ImTextureID)(intptr_t)activeId, ImVec2(64, 64));
						}
					}
					// =====================================================================
					// 4. CAMERA COMPONENT
					// =====================================================================
					else if (compType == COMPONENT_CAMERA)
					{
						CameraComponent* cam = selectedGameObject->GetComponent<CameraComponent>();
						if (cam)
						{
							bool camModified = false;
							camModified |= ImGui::SliderFloat("FOV (Grados)", &cam->fov, 10.0f, 120.0f, "%.1f");
							camModified |= ImGui::DragFloat("Near Plane", &cam->nearPlane, 0.01f, 0.01f, 10.0f, "%.2f");
							camModified |= ImGui::DragFloat("Far Plane", &cam->farPlane, 1.0f, 10.0f, 1000.0f, "%.1f");

							float speed = cam->GetMovementSpeed();
							if (ImGui::SliderFloat("Velocidad", &speed, 0.5f, 30.0f, "%.1f"))
							{
								cam->SetMovementSpeed(speed);
								camModified = true;
							}
							if (camModified)
							{
								selectedGameObject->NotifySceneModified();
							}

							if (Render* render = Engine::GetInstance().render.get())
							{
								ImGui::Spacing();
								ImGui::SeparatorText("Frustum Culling");
								RenderSettings rs = render->GetSettings();
								bool camCullChanged = false;
								camCullChanged |= ImGui::Checkbox("Frustum Culling Activo", &rs.frustumCulling);
								if (ImGui::Checkbox("Ver Frustum Culling desde Cámara del Editor", &rs.cullFromGameCameraInScene))
								{
									if (rs.cullFromGameCameraInScene) rs.frustumCulling = true;
									camCullChanged = true;
								}
								camCullChanged |= ImGui::Checkbox("Dibujar Frustum", &rs.drawFrustum);
								if (camCullChanged)
								{
									render->SetSettings(rs);
								}
							}
						}
					}
				}

				ImGui::PopID();
			}

			ImGui::Separator();

			const char* components[] = {"COMPONENT_MESH","COMPONENT_MATERIAL","COMPONENT_CAMERA"};
			static int selected_component_idx = 0;
			const char* selectedComponent = components[selected_component_idx];
			if (ImGui::BeginCombo("Select ComponentType", selectedComponent))
			{
				static ImGuiTextFilter addComponentFilter;
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetKeyboardFocusHere();
					addComponentFilter.Clear();
				}
				addComponentFilter.Draw();

				for (int n = 0; n < IM_COUNTOF(components); n++)
				{
					const bool is_selected = (selected_component_idx == n);
					if (addComponentFilter.PassFilter(components[n]))
						if (ImGui::Selectable(components[n], is_selected))
							selected_component_idx = n;
				}
				ImGui::EndCombo();
			}
			
			if (ImGui::Button("Add Component", ImVec2(ImGui::GetItemRectSize().x, 0.0f)))
			{
				const ComponentType selectedType = ComponentType(selected_component_idx + 1);

				switch (selectedType)
				{
				case ComponentType::COMPONENT_MATERIAL:
					if (selectedGameObject->GetComponent<MaterialComponent>() != nullptr)
					{
						LOG_WARNING("This GameObject does already have this component");
					}
					else
					{
						auto mat = std::make_unique<MaterialComponent>(selectedGameObject);
						mat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
						mat->SetUseTexture(false);
						selectedGameObject->AddComponent(std::move(mat));
					}
					break;
				case ComponentType::COMPONENT_MESH:
					if (selectedGameObject->GetComponent<MeshComponent>() != nullptr)
					{
						LOG_WARNING("This GameObject does already have this component");
					}
					else
					{
						auto mesh = std::make_unique<MeshComponent>(selectedGameObject, ResourceManager::GetBuiltInMeshUID(PrimitiveType::Cube));
						selectedGameObject->AddComponent(std::move(mesh));
						if (scene) scene->MarkSpacePartitioningDirty();
					}
					break;
				case ComponentType::COMPONENT_CAMERA:
					if (selectedGameObject->GetComponent<CameraComponent>() != nullptr)
					{
						LOG_WARNING("This GameObject does already have this component");
					}
					else
					{
						auto gameCamComp = std::make_unique<CameraComponent>(selectedGameObject);
						selectedGameObject->AddComponent(std::move(gameCamComp));
					}
					break;
				default:
					break;
				}
			}

			if (copiedComponent != nullptr)
			{
				std::string pasteBtnLabel = std::string("Paste Component (") + GetComponentTypeName(copiedComponent->GetType()) + ")";
				if (ImGui::Button(pasteBtnLabel.c_str(), ImVec2(-1.0f, 0.0f)))
				{
					pasteComponentToSelected(copiedComponent.get());
				}
			}
		}
		else
		{
			ImGui::TextDisabled("Selecciona un GameObject en Hierarchy o en Scene para inspeccionar sus componentes.");
		}
	}
	ImGui::End();
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
	copiedComponent.reset();
	consoleViewCache.clear();
	consoleViewCache.shrink_to_fit();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImNodes::DestroyContext();
	ImGui::DestroyContext();

	return true;
}

void ModuleUI::RenderUI()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	ImGui::Render();

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!showConsole) return;

	if (!ImGui::Begin("Console", &showConsole))
	{
		ImGui::End();
		return;
	}

	LogSystem& logSys = LogSystem::GetInstance();

	// 1. Barra de herramientas superior
	if (ImGui::Button("Clear"))
	{
		logSys.Clear();
		selectedLogIndex = -1;
		consoleFilterDirty = true;
	}
	ImGui::SameLine();

	if (ImGui::Checkbox("Collapse", &collapseLogs))
	{
		consoleFilterDirty = true;
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &autoScroll);
	ImGui::SameLine();

	// Filtro de búsqueda por texto
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::InputTextWithHint("##ConsoleSearch", "Buscar...", consoleFilter, sizeof(consoleFilter)))
	{
		consoleFilterDirty = true;
	}
	if (consoleFilter[0] != '\0')
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("x"))
		{
			consoleFilter[0] = '\0';
			consoleFilterDirty = true;
		}
	}

	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();

	// Filtros por Categoría con Contadores
	ImVec4 infoColor = showInfo ? ImVec4(0.35f, 0.75f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, infoColor);
	std::string infoLabel = "[i] Info (" + std::to_string(logSys.GetInfoCount()) + ")";
	if (ImGui::SmallButton(infoLabel.c_str()))
	{
		showInfo = !showInfo;
		consoleFilterDirty = true;
	}
	ImGui::PopStyleColor();
	ImGui::SameLine();

	ImVec4 warnColor = showWarnings ? ImVec4(1.0f, 0.82f, 0.25f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, warnColor);
	std::string warnLabel = "[!] Warn (" + std::to_string(logSys.GetWarningCount()) + ")";
	if (ImGui::SmallButton(warnLabel.c_str()))
	{
		showWarnings = !showWarnings;
		consoleFilterDirty = true;
	}
	ImGui::PopStyleColor();
	ImGui::SameLine();

	ImVec4 errColor = showErrors ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Text, errColor);
	std::string errLabel = "[X] Error (" + std::to_string(logSys.GetErrorCount()) + ")";
	if (ImGui::SmallButton(errLabel.c_str()))
	{
		showErrors = !showErrors;
		consoleFilterDirty = true;
	}
	ImGui::PopStyleColor();

	ImGui::Separator();

	// 2. Reconstruir la caché de índices visibles solo si cambió la revisión del LogSystem o los filtros
	const uint64_t currentRev = logSys.GetRevision();
	if (currentRev != lastConsoleRevision || consoleFilterDirty)
	{
		if (currentRev != lastConsoleRevision && autoScroll)
		{
			scrollToBottom = true;
		}
		lastConsoleRevision = currentRev;
		consoleFilterDirty = false;

		consoleViewCache.clear();
		std::string searchLower = consoleFilter;
		std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });

		std::string prevText;
		LogType prevType = LogType::Info;

		logSys.ForEach([&](int index, const LogEntry& entry)
		{
			if (entry.type == LogType::Info && !showInfo) return;
			if (entry.type == LogType::Warning && !showWarnings) return;
			if (entry.type == LogType::Error && !showErrors) return;
			if (!ContainsCaseInsensitive(entry.text, searchLower)) return;

			if (collapseLogs && !consoleViewCache.empty() && entry.type == prevType && entry.text == prevText)
			{
				consoleViewCache.back().collapsedCount++;
				return;
			}

			consoleViewCache.push_back({ index, 1 });
			prevText = entry.text;
			prevType = entry.type;
		});
	}

	// 3. Región de lista y panel de detalles inferior
	LogEntry selectedEntry;
	const bool hasSelectedEntry = (selectedLogIndex >= 0) && logSys.GetEntry(selectedLogIndex, selectedEntry);
	if (!hasSelectedEntry)
	{
		selectedLogIndex = -1;
	}

	float detailsHeight = hasSelectedEntry ? 95.0f : 0.0f;
	float listHeight = ImGui::GetContentRegionAvail().y - detailsHeight;
	if (detailsHeight > 0.0f) listHeight -= ImGui::GetStyle().ItemSpacing.y;

	ImGui::BeginChild("LogScrollRegion", ImVec2(0, listHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

	LogEntry entry;
	for (size_t viewIdx = 0; viewIdx < consoleViewCache.size(); ++viewIdx)
	{
		const ConsoleDisplayItem& item = consoleViewCache[viewIdx];
		if (!logSys.GetEntry(item.logIndex, entry)) continue;

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

		ImGui::PushID(item.logIndex);

		bool isSelected = (selectedLogIndex == item.logIndex);

		ImGui::TextDisabled("[%s]", entry.timestamp.c_str());
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Text, textColor);
		ImGui::TextUnformatted(typeTag);
		ImGui::PopStyleColor();
		ImGui::SameLine();

		if (collapseLogs && item.collapsedCount > 1)
		{
			ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "[x%d]", item.collapsedCount);
			ImGui::SameLine();
		}

		if (ImGui::Selectable(entry.text.c_str(), isSelected))
		{
			selectedLogIndex = isSelected ? -1 : item.logIndex;
		}

		ImGui::PopID();
	}

	if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
	{
		ImGui::SetScrollHereY(1.0f);
		scrollToBottom = false;
	}

	ImGui::EndChild();

	// 4. Panel inferior de detalle del mensaje seleccionado
	if (hasSelectedEntry)
	{
		ImGui::Separator();
		ImGui::BeginChild("LogDetailsRegion", ImVec2(0, detailsHeight), true);

		ImGui::TextDisabled("Detalle:");
		ImGui::SameLine();
		if (ImGui::SmallButton("Copiar Mensaje"))
		{
			std::string fullText = "[" + selectedEntry.timestamp + "] " + selectedEntry.text;
			if (!selectedEntry.file.empty()) fullText += " (" + selectedEntry.file + ":" + std::to_string(selectedEntry.line) + ")";
			ImGui::SetClipboardText(fullText.c_str());
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Cerrar Detalle"))
		{
			selectedLogIndex = -1;
		}

		if (!selectedEntry.file.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("| Origen: %s:%d", selectedEntry.file.c_str(), selectedEntry.line);
		}

		ImGui::TextWrapped("%s", selectedEntry.text.c_str());

		ImGui::EndChild();
	}

	ImGui::End();
}

void ModuleUI::DrawConfigurationPanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!showConfiguration) return;

	if (ImGui::Begin("Configuration", &showConfiguration))
	{
		if (ImGui::BeginTabBar("ConfigurationTabBar", ImGuiTabBarFlags_None))
		{
			// =========================================================================
			// MONITORIZACIÓN DEL RENDIMIENTO (APPLICATION)
			// =========================================================================
			if (ImGui::BeginTabItem("Application"))
			{
				Engine& eng = Engine::GetInstance();
				const auto& perf = eng.GetPerformanceData();

				ImGui::Text("Motor: Motor Videojuego 3D");
				ImGui::BulletText("Tiempo Activo (Uptime): %.1f s", perf.uptimeSeconds);
				ImGui::BulletText("Frames Totales: %zu", perf.totalFrames);
				ImGui::BulletText("Consumo RAM del Proceso: %zu MB (Pico: %zu MB)", perf.processMemoryMB, perf.processPeakMemoryMB);

				ImGui::Spacing();
				ImGui::SeparatorText("Time Management (Reloj del Editor y del Juego)");

				const SimulationState simState = eng.GetSimulationState();
				const char* stateLabel = (simState == SimulationState::Playing) ? "PLAYING" : ((simState == SimulationState::Paused) ? "PAUSED" : "STOPPED (Editor)");
				ImGui::BulletText("Estado de Simulación: %s", stateLabel);
				ImGui::BulletText("Cronómetro 1 (Tiempo del Editor / Real Time): %.2f s (dt real: %.2f ms | Frames: %zu)",
					eng.GetEditorTimeSec(), eng.GetDt(), perf.totalFrames);
				ImGui::BulletText("Cronómetro 2 (Tiempo del Juego / Game Clock): %.2f s (Real en Play: %.2f s | dt juego: %.2f ms | Frames juego: %d)",
					eng.GetGameTimeSec(), eng.GetGameTimerRawSec(), eng.GetGameDt(), eng.GetGameFrameCount());

				float timeScale = eng.GetTimeScale();
				if (ImGui::SliderFloat("Escala de Tiempo (Time Scale)", &timeScale, 0.0f, 4.0f, "%.2fx"))
				{
					eng.SetTimeScale(timeScale);
				}

				std::error_code ec;
				const bool tempExists = std::filesystem::exists(eng.GetTempSceneFilePath(), ec);
				ImGui::BulletText("Archivo Temporal de Escena ('%s'): %s",
					eng.GetTempSceneFilePath().c_str(),
					tempExists ? "Creado en disco (listo para restaurar al dar Stop)" : "Inactivo");

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

				char fpsOverlay[128];
				sprintf_s(fpsOverlay, sizeof(fpsOverlay), "FPS: %.1f (Min: %.1f, Max: %.1f, Med: %.1f)",
					perf.currentFps, perf.minFps, perf.maxFps, perf.avgFps);
				ImGui::PlotLines("##FPSGraph", perf.fpsHistory, (int)PerformanceData::HISTORY_CAPACITY,
					(int)perf.historyIndex, fpsOverlay, 0.0f, 140.0f, ImVec2(0, 80));

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
			// CONFIGURACIÓN DEL MÓDULO WINDOW
			// =========================================================================
			if (ImGui::BeginTabItem("Window"))
			{
				auto win = Engine::GetInstance().window;
				const int curW = win->GetWidth();
				const int curH = win->GetHeight();

				ImGui::SeparatorText("Propiedades en Tiempo Real");

				bool applyTitle = false;
				if (ImGui::InputText("Título", windowTitleBuffer, sizeof(windowTitleBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					applyTitle = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Aplicar Título"))
				{
					applyTitle = true;
				}
				if (applyTitle && windowTitleBuffer[0] != '\0')
				{
					win->SetTitle(windowTitleBuffer);
				}

				// Modificación 100% reactiva del tamaño de ventana (sin botones intermedios)
				int w = curW;
				int h = curH;
				bool sizeChanged = false;
				sizeChanged |= ImGui::DragInt("Ancho (px)", &w, 2.0f, Window::MIN_WIDTH, 3840);
				sizeChanged |= ImGui::DragInt("Alto (px)", &h, 2.0f, Window::MIN_HEIGHT, 2160);
				if (sizeChanged)
				{
					win->SetSize(w, h);
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
				ImGui::SeparatorText("Parámetros de Contexto [Requiere Reinicio]");
				ImGui::TextDisabled("• API Gráfica: OpenGL 4.6 Core Profile");
				ImGui::TextDisabled("• Doble Búfer (Double Buffer): Habilitado");
				ImGui::TextDisabled("• Profundidad Z-Buffer: 24 bits | Stencil: 8 bits");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// CONFIGURACIÓN DEL MÓDULO RENDERER (Única fuente de verdad)
			// =========================================================================
			if (ImGui::BeginTabItem("Renderer"))
			{
				auto rnd = Engine::GetInstance().render;
				RenderSettings rs = rnd->GetSettings();
				bool settingsChanged = false;

				ImGui::SeparatorText("Pipeline OpenGL en Tiempo Real");
				settingsChanged |= ImGui::Checkbox("Depth Test (GL_DEPTH_TEST)", &rs.depthTest);
				settingsChanged |= ImGui::Checkbox("Face Culling (GL_CULL_FACE)", &rs.cullFace);
				settingsChanged |= ImGui::Checkbox("Iluminación Direccional (Shader Lighting)", &rs.lighting);
				settingsChanged |= ImGui::Checkbox("Modo Wireframe (GL_LINE)", &rs.wireframe);
				settingsChanged |= ImGui::Checkbox("Resaltar Selección", &rs.highlightSelection);

				// ColorEdit para seleccionar el color del outline en tiempo real
				if (rs.highlightSelection)
				{
					ImGui::Indent();
					settingsChanged |= ImGui::ColorEdit3("Color del Outline", glm::value_ptr(rs.outlineColor), ImGuiColorEditFlags_PickerHueWheel);
					ImGui::Unindent();
				}

				bool showDepth = rs.showDepthBuffer;
				if (ImGui::Checkbox("Show Z-Buffer Window", &showDepth))
				{
					rs.showDepthBuffer = showDepth;
					rnd->SetSettings(rs);
				}

				// Subapartado para Grid y Central Axis
				ImGui::Spacing();
				ImGui::SeparatorText("Grid & Origin Axis");
				settingsChanged |= ImGui::Checkbox("Mostrar Rejilla (Plano XZ)", &rs.showGrid);
				settingsChanged |= ImGui::Checkbox("Mostrar Ejes Centrales (X: Rojo, Y: Verde, Z: Azul)", &rs.showAxis);

				ImGui::Spacing();
				ImGui::SeparatorText("Space Partitioning & Frustum Culling");
				settingsChanged |= ImGui::Checkbox("Frustum Culling Activo", &rs.frustumCulling);
				settingsChanged |= ImGui::Checkbox("Evaluar Culling desde Main Camera en Scene", &rs.cullFromGameCameraInScene);

				static const char* partitionModes[] = {
					"Desactivado (Solo AABB directo)",
					"Octree (3D XYZ)"
				};
				int currentPartitionMode = (int)rs.partitionMode;
				if (ImGui::Combo("Estructura Activa", &currentPartitionMode, partitionModes, IM_ARRAYSIZE(partitionModes)))
				{
					rs.partitionMode = (SpacePartitionMode)currentPartitionMode;
					settingsChanged = true;
				}

				Scene* scenePtr = Engine::GetInstance().scene.get();
				if (scenePtr)
				{
					int maxObjs = scenePtr->GetTreeMaxObjects();
					int maxDepth = scenePtr->GetTreeMaxDepth();
					bool treeParamsChanged = false;
					treeParamsChanged |= ImGui::SliderInt("Máx. Objetos por Nodo", &maxObjs, 1, 10);
					treeParamsChanged |= ImGui::SliderInt("Profundidad Máxima", &maxDepth, 1, 8);
					if (treeParamsChanged)
					{
						scenePtr->SetTreeParameters(maxObjs, maxDepth);
					}
				}

				settingsChanged |= ImGui::Checkbox("Dibujar AABB (Verde)", &rs.drawAABB);
				settingsChanged |= ImGui::Checkbox("Dibujar Octree (Cian)", &rs.drawOctree);
				settingsChanged |= ImGui::Checkbox("Dibujar Frustum Main Camera (Ámbar)", &rs.drawFrustum);

				const CullingStats& cStats = rnd->GetCullingStats();
				ImGui::BulletText("Mallas Activas: %d (Visibles: %d | Descartadas por Culling: %d)",
					cStats.totalMeshObjects, cStats.visibleObjects, cStats.culledObjects);
				if (scenePtr)
				{
					ImGui::BulletText("Objetos Static en Árbol: %zu | Nodos Octree: %zu (Visitados: %d)",
						scenePtr->GetOctree().GetObjectCount(),
						scenePtr->GetOctree().GetNodeCount(),
						cStats.treeNodesVisited);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Depuración de Normales");
				static const char* normalModes[] = {
					"Desactivado",
					"Por Vértice (Amarillo)",
					"Por Cara / Triángulo (Cian)",
					"Ambos (Vértice + Cara)"
				};
				int currentNormalsMode = (int)rs.normals;
				if (ImGui::Combo("Modo Normales", &currentNormalsMode, normalModes, IM_ARRAYSIZE(normalModes)))
				{
					rs.normals = (NormalsMode)currentNormalsMode;
					settingsChanged = true;
				}
				settingsChanged |= ImGui::Checkbox("Mostrar normales en toda la escena", &rs.normalsAllObjects);

				if (settingsChanged)
				{
					rnd->SetSettings(rs);
				}

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
				ImGui::BulletText("Resolución Viewport: %d x %d px", rnd->GetViewportWidth(), rnd->GetViewportHeight());
				ImGui::BulletText("ID Framebuffer (FBO): %u", rnd->GetFramebufferID());
				ImGui::BulletText("ID Textura Color: %u", rnd->GetSceneTexture());
				ImGui::BulletText("ID Renderbuffer (RBO): %u", rnd->GetRboID());
				ImGui::BulletText("Estado OpenGL: %s", rnd->IsFramebufferComplete() ? "GL_FRAMEBUFFER_COMPLETE" : "INCOMPLETO");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// CONFIGURACIÓN DE CÁMARA DE EDITOR (CAMERA)
			// =========================================================================
			if (ImGui::BeginTabItem("Camera"))
			{
				Scene* scene = Engine::GetInstance().scene.get();
				GameObject* camObj = scene ? scene->GetMainCameraObject() : nullptr;
				CameraComponent* cam = camObj ? camObj->GetComponent<CameraComponent>() : nullptr;
				TransformComponent* camTrans = camObj ? camObj->GetComponent<TransformComponent>() : nullptr;

				if (cam && camTrans)
				{
					ImGui::SeparatorText("Parámetros de Proyección");
					ImGui::SliderFloat("Campo de Visión (FOV)", &cam->fov, 10.0f, 120.0f, "%.1f deg");
					ImGui::DragFloat("Plano Cercano (Near)", &cam->nearPlane, 0.01f, 0.01f, 10.0f, "%.2f");
					ImGui::DragFloat("Plano Lejano (Far)", &cam->farPlane, 1.0f, 10.0f, 2000.0f, "%.1f");

					ImGui::Spacing();
					ImGui::SeparatorText("Navegación y Control");
					glm::vec3 pos = camTrans->GetPosition();
					if (ImGui::DragFloat3("Posición Cámara", glm::value_ptr(pos), 0.1f))
					{
						camTrans->SetPosition(pos);
					}

					float speed = cam->GetMovementSpeed();
					if (ImGui::SliderFloat("Velocidad Movimiento", &speed, 0.5f, 30.0f, "%.1f u/s"))
					{
						cam->SetMovementSpeed(speed);
					}

					float sens = cam->GetMouseSensitivity();
					if (ImGui::SliderFloat("Sensibilidad Ratón", &sens, 0.01f, 1.0f, "%.2f"))
					{
						cam->SetMouseSensitivity(sens);
					}

					float orbitDist = cam->GetOrbitDistance();
					if (ImGui::SliderFloat("Distancia de Órbita", &orbitDist, 1.0f, 30.0f, "%.1f"))
					{
						cam->SetOrbitDistance(orbitDist);
					}

					if (ImGui::Button("Restablecer Cámara"))
					{
						cam->ResetCamera();
					}
					if (scene && scene->GetSelected())
					{
						ImGui::SameLine();
						if (ImGui::Button("Enfocar Seleccionado (F)"))
						{
							cam->FocusOnObject(scene->GetSelected());
						}
					}
				}
				else
				{
					ImGui::TextDisabled("No hay cámara activa en la escena.");
				}

				ImGui::EndTabItem();
			}

			// =========================================================================
			// CONFIGURACIÓN DEL MÓDULO INPUT
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

				ImGui::EndTabItem();
			}

			// =========================================================================
			// CONFIGURACIÓN DEL MÓDULO TEXTURES (DEVIL / OPENGL)
			// =========================================================================
			if (ImGui::BeginTabItem("Textures"))
			{
				const auto& hw = Engine::GetInstance().GetHardwareMetrics();
				TextureManager* texMgr = Engine::GetInstance().textures.get();

				ImGui::SeparatorText("Subsistema DevIL & Caché GPU");
				ImGui::BulletText("Librería de Carga: Developer's Image Library (DevIL v%s)", hw.devilVersion.c_str());
				ImGui::BulletText("Formatos Compatibles: PNG, JPG, JPEG, DDS, TGA, BMP");
				// Las opciones de muestreo son de cada textura (su .meta) y se editan en el Inspector
				if (ResourceManager* resources = Engine::GetInstance().resources.get())
				{
					ImGui::BulletText("Texturas en VRAM: %zu (+ checkerboard y missing)", resources->GetLoadedCount(ResourceType::Texture));
					ImGui::BulletText("Mallas en memoria: %zu", resources->GetLoadedCount(ResourceType::Mesh));
					ImGui::BulletText("Memoria de recursos: %.2f MB", resources->GetTotalMemoryUsage() / (1024.0 * 1024.0));
				}
				(void)texMgr;

				ImGui::EndTabItem();
			}

			// =========================================================================
			// CONFIGURACIÓN DEL MÓDULO AUDIO
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
			// 8. INFORMACIÓN DEL SISTEMA, HARDWARE Y VERSIONES (Cacheadas)
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

				// Memoria (RAM y VRAM)
				ImGui::Spacing();
				ImGui::SeparatorText("Consumo de Memoria (RAM y VRAM)");
				ImGui::BulletText("RAM del Proceso (Working Set): %zu MB (Pico: %zu MB)", perf.processMemoryMB, perf.processPeakMemoryMB);
				if (hw.vramTotalMB > 0)
				{
					int vramUsed = std::max(0, hw.vramTotalMB - hw.vramAvailableMB);
					ImGui::BulletText("VRAM Dedicada Total: %d MB", hw.vramTotalMB);
					ImGui::BulletText("VRAM Disponible: %d MB (En uso aprox.: %d MB)", hw.vramAvailableMB, vramUsed);
				}
				else
				{
					ImGui::BulletText("VRAM: No reportada por el driver actual (sin extensión NVX/ATI)");
				}

				// Software y Bibliotecas
				ImGui::Spacing();
				ImGui::SeparatorText("Versiones Activas de Software");
				ImGui::BulletText("Sistema Operativo: %s", hw.osPlatform.c_str());
				ImGui::BulletText("SDL3: v%s", hw.sdlVersion.c_str());
				ImGui::BulletText("OpenGL: %s", hw.openglVersion.c_str());
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

void ModuleUI::DrawAboutPanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!showAbout) return;

	ImGui::SetNextWindowSize(ImVec2(520, 440), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("About MotorVideojuego", &showAbout))
	{
		const auto& hw = Engine::GetInstance().GetHardwareMetrics();

		ImGui::TextColored(ImVec4(0.35f, 0.8f, 1.0f, 1.0f), "MotorVideojuego 3D Engine");
		ImGui::SameLine();
		ImGui::TextDisabled("v0.0.1");
		ImGui::TextWrapped("Motor gráfico 3D educativo desarrollado en c++ con arquitectura modular de Entidades/Componentes y editor integrado.");

		ImGui::Spacing();
		ImGui::SeparatorText("Equipo de Desarrollo");
		ImGui::BulletText("Jan Corredor");
		ImGui::BulletText("Rubén Mateo Guerrero");
		ImGui::BulletText("Marc Jiménez Marquina");

		ImGui::Spacing();
		ImGui::SeparatorText("Librerías Utilizadas");
		ImGui::BulletText("SDL3 v%s (Ventana, Contexto, Input y Audio)", hw.sdlVersion.c_str());
		ImGui::BulletText("OpenGL %s + GLAD (Renderizado 3D)", hw.openglVersion.c_str());
		ImGui::BulletText("Dear ImGui v%s (Interfaz del Editor y Docking)", hw.imguiVersion.c_str());
		ImGui::BulletText("Assimp v%s (Importación de Modelos 3D)", hw.assimpVersion.c_str());
		ImGui::BulletText("DevIL v%s (Carga de Texturas)", hw.devilVersion.c_str());
		ImGui::BulletText("GLM (Matemáticas 3D, Matrices y Transformaciones)");
		ImGui::BulletText("Tracy Profiler (Profiling y Rendimiento)");

		ImGui::Spacing();
		ImGui::SeparatorText("Licencia MIT");
		ImGui::BeginChild("MITLicenseChild", ImVec2(0, 130), true);
		ImGui::TextWrapped(
			"MIT License\n\n"
			"Copyright (c) 2026 MotorVideojuego Team\n\n"
			"Permission is hereby granted, free of charge, to any person obtaining a copy "
			"of this software and associated documentation files (the \"Software\"), to deal "
			"in the Software without restriction, including without limitation the rights "
			"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
			"copies of the Software, and to permit persons to whom the Software is "
			"furnished to do so, subject to the following conditions:\n\n"
			"The above copyright notice and this permission notice shall be included in all "
			"copies or substantial portions of the Software.\n\n"
			"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
			"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
			"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
			"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
			"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
			"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
			"SOFTWARE."
		);
		ImGui::EndChild();
	}
	ImGui::End();
}

void ModuleUI::DrawDepthBufferPanel()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	Render* render = Engine::GetInstance().render.get();
	if (!render) return;

	RenderSettings rs = render->GetSettings();
	if (!rs.showDepthBuffer) return; // Window stays hidden if setting is false

	bool open = rs.showDepthBuffer;
	if (ImGui::Begin("Z-Buffer Visualizer", &open))
	{
		unsigned int depthTexture = render->GetDepthTexture();

		if (depthTexture != 0)
		{
			ImVec2 availSize = ImGui::GetContentRegionAvail();
			if (availSize.x < 50.0f) availSize.x = 200.0f; // Fallback width
			if (availSize.y < 50.0f) availSize.y = 100.0f; // Fallback height
			
			// Draw the depth buffer texture (Invert UVs on Y axis for OpenGL)
			ImGui::Image((ImTextureID)(intptr_t)depthTexture, availSize, ImVec2(0, 1), ImVec2(1, 0));			
		}
		else
		{
			ImGui::TextDisabled("Depth texture ID is 0 (Not initialized).");
		}
	}
	ImGui::End();

	// Sync state if closed via the 'X' button
	if (!open)
	{
		rs.showDepthBuffer = false;
		render->SetSettings(rs);
	}
}

void ModuleUI::TriggerSaveScene()
{
	if (!Engine::GetInstance().IsStopped())
	{
		LOG_WARNING("[UI] Detén el modo Play/Pause antes de guardar la escena para no sobrescribir el estado inicial.");
		return;
	}

	Scene* scene = Engine::GetInstance().scene.get();
	if (!scene) return;

	if (scene->HasSavedScenePath())
	{
		scene->Save(scene->GetCurrentScenePath());
	}
	else
	{
		// La escena no ha sido guardada previamente: pedir obligatoriamente un nombre nuevo
		saveSceneFileNameBuffer[0] = '\0';
		openSaveSceneAsPopup = true;
	}
}

void ModuleUI::TriggerSaveSceneAs()
{
	if (!Engine::GetInstance().IsStopped())
	{
		LOG_WARNING("[UI] Detén el modo Play/Pause antes de guardar la escena para no sobrescribir el estado inicial.");
		return;
	}

	Scene* scene = Engine::GetInstance().scene.get();
	if (scene && scene->HasSavedScenePath())
	{
		std::filesystem::path p(scene->GetCurrentScenePath());
		std::string fname = p.filename().string();
		strncpy_s(saveSceneFileNameBuffer, sizeof(saveSceneFileNameBuffer), fname.c_str(), _TRUNCATE);
	}
	else
	{
		saveSceneFileNameBuffer[0] = '\0';
	}
	openSaveSceneAsPopup = true;
}

void ModuleUI::DrawSceneFileModals()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	Scene* scene = Engine::GetInstance().scene.get();

	if (openSaveSceneAsPopup)
	{
		ImGui::OpenPopup("Save Scene As");
		openSaveSceneAsPopup = false;
	}

	if (openLoadScenePopup)
	{
		ImGui::OpenPopup("Load Scene");
		openLoadScenePopup = false;
	}

	// =========================================================================
	// MODAL: SAVE SCENE AS
	// =========================================================================
	ImGui::SetNextWindowSize(ImVec2(480, 350), ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_None))
	{
		std::string scenesDir = FileSystem::ToDiskPath(FileSystem::SCENES_DIR);
		ImGui::TextDisabled("Carpeta de destino: %s", FileSystem::SCENES_DIR);
		ImGui::Spacing();

		ImGui::TextUnformatted("Escenas existentes (clic para seleccionar):");
		ImGui::BeginChild("ExistingScenesSaveList", ImVec2(0, 140), true);
		std::vector<std::string> existingScenes = FileSystem::ListSavedSceneFiles();
		if (existingScenes.empty())
		{
			ImGui::TextDisabled("No hay archivos .scene o .json guardados todavía.");
		}
		else
		{
			for (size_t i = 0; i < existingScenes.size(); ++i)
			{
				std::filesystem::path sp(existingScenes[i]);
				std::string fname = sp.filename().string();
				bool isSelected = (fname == saveSceneFileNameBuffer);
				if (ImGui::Selectable(fname.c_str(), isSelected))
				{
					strncpy_s(saveSceneFileNameBuffer, sizeof(saveSceneFileNameBuffer), fname.c_str(), _TRUNCATE);
				}
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::TextUnformatted("Nombre del archivo (.scene o .json):");
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(-1.0f);
		bool enterPressed = ImGui::InputTextWithHint("##SaveSceneFileName", "Introduce un nombre para la escena...", saveSceneFileNameBuffer, sizeof(saveSceneFileNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

		std::string trimmedName = saveSceneFileNameBuffer;
		while (!trimmedName.empty() && std::isspace(static_cast<unsigned char>(trimmedName.front())))
		{
			trimmedName.erase(trimmedName.begin());
		}
		while (!trimmedName.empty() && std::isspace(static_cast<unsigned char>(trimmedName.back())))
		{
			trimmedName.pop_back();
		}
		const bool hasValidName = !trimmedName.empty();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(!hasValidName);
		bool saveClicked = ImGui::Button("Guardar (Save)", ImVec2(140, 0));
		ImGui::EndDisabled();

		if ((saveClicked || enterPressed) && hasValidName)
		{
			std::string rawName = trimmedName;
			// Sanitizar caracteres inválidos en nombres de archivo de Windows
			const char* invalidChars = "<>:\"|?*";
			for (size_t i = 0; i < rawName.size(); ++i)
			{
				if (std::strchr(invalidChars, rawName[i]) != nullptr)
				{
					rawName[i] = '_';
				}
			}

			std::filesystem::path p(rawName);
			std::string ext = p.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(),
				[](unsigned char c) { return (char)std::tolower(c); });

			if (ext != ".scene" && ext != ".json")
			{
				rawName += ".scene";
			}

			std::string fullPath = scenesDir + "/" + std::filesystem::path(rawName).filename().string();

			std::error_code ec;
			if (std::filesystem::exists(fullPath, ec))
			{
				pendingOverwriteScenePath = fullPath;
				ImGui::OpenPopup("Confirm Overwrite");
			}
			else
			{
				if (scene)
				{
					scene->Save(fullPath);
				}
				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancelar", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		// Sub-modal de confirmación al intentar sobrescribir una escena existente
		bool closeSaveAsModal = false;
		if (ImGui::BeginPopupModal("Confirm Overwrite", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			std::filesystem::path overwriteFile(pendingOverwriteScenePath);
			ImGui::TextWrapped("Ya existe una escena con el nombre '%s'.\n¿Estás seguro de que quieres sobrescribirla?",
				overwriteFile.filename().string().c_str());
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.22f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
			if (ImGui::Button("Sobrescribir (Overwrite)", ImVec2(175, 0)))
			{
				if (scene && !pendingOverwriteScenePath.empty())
				{
					scene->Save(pendingOverwriteScenePath);
				}
				pendingOverwriteScenePath.clear();
				ImGui::CloseCurrentPopup();
				closeSaveAsModal = true;
			}
			ImGui::PopStyleColor(2);

			ImGui::SameLine();
			if (ImGui::Button("Cancelar", ImVec2(120, 0)))
			{
				pendingOverwriteScenePath.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (closeSaveAsModal)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// =========================================================================
	// MODAL: LOAD SCENE
	// =========================================================================
	ImGui::SetNextWindowSize(ImVec2(500, 360), ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal("Load Scene", nullptr, ImGuiWindowFlags_None))
	{
		std::string scenesDir = FileSystem::SCENES_DIR;
		ImGui::TextDisabled("Carpeta de escenas: %s", scenesDir.c_str());
		ImGui::Spacing();

		ImGui::TextUnformatted("Selecciona una escena para cargar (doble clic para cargar directamente):");
		ImGui::BeginChild("ExistingScenesLoadList", ImVec2(0, 160), true);
		std::vector<std::string> savedScenes = FileSystem::ListSavedSceneFiles();
		bool doubleClickLoad = false;

		if (savedScenes.empty())
		{
			ImGui::TextDisabled("No se encontraron archivos .scene o .json en Assets/Scenes.");
		}
		else
		{
			for (size_t i = 0; i < savedScenes.size(); ++i)
			{
				std::filesystem::path sp(savedScenes[i]);
				std::string fname = sp.filename().string();
				bool isSelected = (savedScenes[i] == loadSceneFilePathBuffer || fname == loadSceneFilePathBuffer);
				if (ImGui::Selectable(fname.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
				{
					strncpy_s(loadSceneFilePathBuffer, sizeof(loadSceneFilePathBuffer), savedScenes[i].c_str(), _TRUNCATE);
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						doubleClickLoad = true;
					}
				}
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::TextUnformatted("Ruta del archivo de escena:");
		ImGui::SetNextItemWidth(-1.0f);
		bool enterPressed = ImGui::InputText("##LoadSceneFilePath", loadSceneFilePathBuffer, sizeof(loadSceneFilePathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if ((ImGui::Button("Cargar (Load)", ImVec2(140, 0)) || enterPressed || doubleClickLoad) && loadSceneFilePathBuffer[0] != '\0')
		{
			if (scene)
			{
				if (!Engine::GetInstance().IsStopped())
				{
					Engine::GetInstance().Stop();
				}
				std::string projectScene = FileSystem::EnsureAssetInProject(loadSceneFilePathBuffer, "Scenes");
				std::string resolved = FileSystem::ResolveExistingPath(projectScene);
				scene->RequestLoadScene(resolved);
			}
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancelar", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}