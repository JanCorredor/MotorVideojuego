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
#include "../Scene/GameObjects/Components/CameraComponent.h"
#include "../Audio/Audio.h"

#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <SDL3/SDL.h>

#include "../../Utils/Log.h"
#include <algorithm>
#include <cctype>
#include <string>

#include <tracy/Tracy.hpp>

namespace
{
	constexpr const char* REPO_URL_DOCS     = "https://github.com/JanCorredor/MotorVideojuego";
	constexpr const char* REPO_URL_ISSUES   = "https://github.com/JanCorredor/MotorVideojuego/issues";
	constexpr const char* REPO_URL_RELEASES = "https://github.com/JanCorredor/MotorVideojuego/releases";

	// Helper para alinear etiquetas y campos vec3 uniformemente en tablas
	bool DrawVec3Control(const char* label, glm::vec3& values, float speed = 0.1f)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);

		ImGui::TableSetColumnIndex(1);
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
	SDL_Window* sdlWindow = Engine::GetInstance().window->GetWindow();
	bool relativeMouse = (sdlWindow != nullptr) && SDL_GetWindowRelativeMouseMode(sdlWindow);

	ImGuiIO& io = ImGui::GetIO();
	if (relativeMouse)
	{
		ImGui_ImplSDL3_SetMouseCaptureMode(ImGui_ImplSDL3_MouseCaptureMode_Disabled);
		io.ConfigFlags |= (ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
		io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
		io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
		io.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
		for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i)
		{
			io.MouseDown[i] = false;
		}
	}
	else
	{
		ImGui_ImplSDL3_SetMouseCaptureMode(ImGui_ImplSDL3_MouseCaptureMode_Enabled);
		io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
	}

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

			ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
			ImGui::DockBuilderDockWindow("Scene", dock_main_id);
			ImGui::DockBuilderDockWindow("Console", dock_down);
			ImGui::DockBuilderDockWindow("Inspector", dock_right);
			ImGui::DockBuilderDockWindow("Configuration", dock_main_id);

			ImGui::DockBuilderFinish(dockspace_id);
		}
		layoutInitialized = true;
		resetLayout = false;
	}

	DrawScenePanel();
	DrawHierarchyPanel();
	DrawInspectorPanel();
	DrawConfigurationPanel();
	DrawConsolePanel();
	DrawAboutPanel();

	return ret;
}

bool ModuleUI::DrawMainMenuBar()
{
	bool continueRunning = true;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
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
			ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &showInspector);
			ImGui::MenuItem("Console", nullptr, &showConsole);
			ImGui::MenuItem("Configuration", nullptr, &showConfiguration);
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

		ImGui::EndMainMenuBar();
	}

	return continueRunning;
}

void ModuleUI::DrawScenePanel()
{
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
	if (!visible)
	{
		if (render) render->SetViewportVisible(false);
		if (camComp) camComp->SetViewportHovered(false);
		ImGui::End();
		return;
	}

	if (render) render->SetViewportVisible(true);

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
		bool altHeld = input && (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT);

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

void ModuleUI::DrawHierarchyPanel()
{
	if (!showHierarchy) return;

	if (ImGui::Begin("Hierarchy", &showHierarchy))
	{
		Scene* scene = Engine::GetInstance().scene.get();
		GameObject* selected = scene ? scene->GetSelected() : nullptr;

		if (ImGui::SmallButton("+ Empty"))
		{
			if (scene)
			{
				GameObject* created = scene->CreateEmptyGameObject();
				scene->SetSelected(created);
				selected = created;
			}
		}

		if (selected)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Deseleccionar"))
			{
				scene->SetSelected(nullptr);
				selected = nullptr;
			}
		}
		ImGui::Separator();

		if (scene)
		{
			GameObject* root = scene->GetRoot();
			if (root)
			{
				for (GameObject* child : root->GetChildren())
				{
					DrawHierarchyNode(child, selected);
				}
			}
		}
	}
	ImGui::End();
}

void ModuleUI::DrawHierarchyNode(GameObject* node, GameObject* selected)
{
	if (!node) return;

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (selected == node)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (node->GetChildren().empty())
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
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

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		if (Engine::GetInstance().scene)
		{
			Engine::GetInstance().scene->SetSelected(node);
		}
	}

	if (nodeOpen && !node->GetChildren().empty())
	{
		for (GameObject* child : node->GetChildren())
		{
			DrawHierarchyNode(child, selected);
		}
		ImGui::TreePop();
	}
}

void ModuleUI::DrawInspectorPanel()
{
	if (!showInspector) return;

	if (ImGui::Begin("Inspector", &showInspector))
	{
		Scene* scene = Engine::GetInstance().scene.get();
		GameObject* selected = scene ? scene->GetSelected() : nullptr;

		if (selected)
		{
			bool isActive = selected->IsActive();
			if (ImGui::Checkbox("##ActiveGO", &isActive))
			{
				if (isActive) selected->Enable();
				else selected->Disable();
			}
			ImGui::SameLine();

			char nameBuf[128] = "";
			strncpy_s(nameBuf, sizeof(nameBuf), selected->GetName().c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##GOName", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (nameBuf[0] != '\0')
				{
					selected->SetName(nameBuf);
				}
			}

			ImGui::Separator();

			// =====================================================================
			// 1. TRANSFORM COMPONENT
			// =====================================================================
			TransformComponent* transform = selected->GetComponent<TransformComponent>();
			if (transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::BeginTable("TransformTable", 2, ImGuiTableFlags_SizingFixedFit))
				{
					ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 75.0f);
					ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

					glm::vec3 pos = transform->GetPosition();
					if (DrawVec3Control("Position", pos, 0.1f))
					{
						transform->SetPosition(pos);
					}

					glm::vec3 rot = transform->GetRotation();
					if (DrawVec3Control("Rotation", rot, 1.0f))
					{
						transform->SetRotation(rot);
					}

					glm::vec3 scl = transform->GetScale();
					if (DrawVec3Control("Scale", scl, 0.05f))
					{
						transform->SetScale(scl);
					}

					ImGui::EndTable();
				}

				if (ImGui::SmallButton("Reset Transform"))
				{
					transform->SetPosition(glm::vec3(0.0f));
					transform->SetRotation(glm::vec3(0.0f));
					transform->SetScale(glm::vec3(1.0f));
				}
			}

			// =====================================================================
			// 2. MESH COMPONENT
			// =====================================================================
			MeshComponent* mesh = selected->GetComponent<MeshComponent>();
			if (mesh && ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const Primitive* prim = mesh->GetPrimitive();
				if (prim)
				{
					ImGui::BulletText("Vértices: %zu", prim->GetVertexCount());
					ImGui::BulletText("Triángulos (Caras): %zu", prim->GetTriangleCount());
					ImGui::BulletText("Índices: %zu", prim->GetIndices().size());

					const glm::vec3 bMin = prim->GetBoundsMin();
					const glm::vec3 bMax = prim->GetBoundsMax();
					ImGui::BulletText("AABB Min: (%.2f, %.2f, %.2f)", bMin.x, bMin.y, bMin.z);
					ImGui::BulletText("AABB Max: (%.2f, %.2f, %.2f)", bMax.x, bMax.y, bMax.z);

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
				else
				{
					ImGui::TextDisabled("Sin malla cargada en GPU");
				}
			}

			// =====================================================================
			// 3. MATERIAL / TEXTURE COMPONENT
			// =====================================================================
			MaterialComponent* mat = selected->GetComponent<MaterialComponent>();
			if (mat && ImGui::CollapsingHeader("Texture & Material", ImGuiTreeNodeFlags_DefaultOpen))
			{
				glm::vec3 color = mat->GetColor();
				if (ImGui::ColorEdit3("Color Base", glm::value_ptr(color), ImGuiColorEditFlags_PickerHueWheel))
				{
					mat->SetColor(color);
				}

				ImGui::Spacing();
				bool usingTexture = mat->GetUseTexture();
				if (ImGui::Checkbox("Usar Textura Difusa", &usingTexture))
				{
					mat->SetUseTexture(usingTexture);
				}

				bool usingChecker = mat->GetUseCheckerboard();
				if (ImGui::Checkbox("Textura Checkerboard (Blanco/Negro)", &usingChecker))
				{
					mat->SetUseCheckerboard(usingChecker);
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Datos de Textura");
				const TextureInfo* texInfo = mat->GetTexture();
				if (texInfo)
				{
					ImGui::BulletText("Path: %s", texInfo->path.c_str());
					ImGui::BulletText("Tamaño: %d x %d px", texInfo->width, texInfo->height);
					ImGui::BulletText("OpenGL Texture ID: %u", texInfo->id);
				}
				else
				{
					ImGui::TextDisabled("Sin textura difusa asignada (arrastra un PNG/JPG/DDS a la ventana).");
				}

				unsigned int activeId = mat->GetActiveTextureID();
				if (activeId != 0)
				{
					ImGui::Spacing();
					ImGui::TextDisabled("Vista previa activa:");
					ImGui::Image((ImTextureID)(intptr_t)activeId, ImVec2(64, 64));
				}
			}

			// =====================================================================
			// 4. CAMERA COMPONENT (si el GameObject seleccionado es una cámara)
			// =====================================================================
			CameraComponent* cam = selected->GetComponent<CameraComponent>();
			if (cam && ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::SliderFloat("FOV (Grados)", &cam->fov, 10.0f, 120.0f, "%.1f");
				ImGui::DragFloat("Near Plane", &cam->nearPlane, 0.01f, 0.01f, 10.0f, "%.2f");
				ImGui::DragFloat("Far Plane", &cam->farPlane, 1.0f, 10.0f, 1000.0f, "%.1f");

				float speed = cam->GetMovementSpeed();
				if (ImGui::SliderFloat("Velocidad", &speed, 0.5f, 30.0f, "%.1f"))
				{
					cam->SetMovementSpeed(speed);
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
	consoleViewCache.clear();
	consoleViewCache.shrink_to_fit();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	return true;
}

void ModuleUI::RenderUI()
{
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
				if (ImGui::Button("Sincronizar Actual"))
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
				ImGui::SeparatorText("Parámetros de Contexto [Requiere Reinicio]");
				ImGui::TextDisabled("• API Gráfica: OpenGL 4.6 Core Profile");
				ImGui::TextDisabled("• Doble Búfer (Double Buffer): Habilitado");
				ImGui::TextDisabled("• Profundidad Z-Buffer: 24 bits | Stencil: 8 bits");

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 3. CONFIGURACIÓN DEL MÓDULO RENDERER (Única fuente de verdad)
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
				settingsChanged |= ImGui::Checkbox("Resaltar Selección (Outline Naranja)", &rs.highlightSelection);
				settingsChanged |= ImGui::Checkbox("V-Sync", &rs.vsync);

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
			// 4. CONFIGURACIÓN DE CÁMARA DE EDITOR (CAMERA)
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
			// 5. CONFIGURACIÓN DEL MÓDULO INPUT
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
			// 6. CONFIGURACIÓN DEL MÓDULO TEXTURES (DEVIL / OPENGL)
			// =========================================================================
			if (ImGui::BeginTabItem("Textures"))
			{
				const auto& hw = Engine::GetInstance().GetHardwareMetrics();
				TextureManager* texMgr = Engine::GetInstance().textures.get();

				ImGui::SeparatorText("Subsistema DevIL & Caché GPU");
				ImGui::BulletText("Librería de Carga: Developer's Image Library (DevIL v%s)", hw.devilVersion.c_str());
				ImGui::BulletText("Formatos Compatibles: PNG, JPG, JPEG, DDS, TGA, BMP");
				if (texMgr)
				{
					ImGui::BulletText("Texturas en Caché VRAM: %zu (+1 Checkerboard procedural)", texMgr->GetLoadedCount());
				}

				if (texMgr)
				{
					ImGui::Spacing();
					ImGui::SeparatorText("Parámetros de Muestreo OpenGL (Tiempo Real)");

					TextureSettings ts = texMgr->GetSettings();
					bool texChanged = false;

					static const char* minFilters[] = {
						"GL_LINEAR_MIPMAP_LINEAR (Trilineal)",
						"GL_LINEAR (Bilineal)",
						"GL_NEAREST (Vecino más cercano)"
					};
					texChanged |= ImGui::Combo("Filtro Minificación", &ts.minFilter, minFilters, IM_ARRAYSIZE(minFilters));

					static const char* magFilters[] = {
						"GL_LINEAR (Suave)",
						"GL_NEAREST (Pixelado)"
					};
					texChanged |= ImGui::Combo("Filtro Magnificación", &ts.magFilter, magFilters, IM_ARRAYSIZE(magFilters));

					static const char* wrapModes[] = {
						"GL_REPEAT (Repetición)",
						"GL_CLAMP_TO_EDGE (Abrazar bordes)",
						"GL_MIRRORED_REPEAT (Espejado)"
					};
					texChanged |= ImGui::Combo("Modo Wrap (S/T)", &ts.wrapMode, wrapModes, IM_ARRAYSIZE(wrapModes));

					if (texChanged)
					{
						texMgr->SetSettings(ts);
					}
				}

				ImGui::EndTabItem();
			}

			// =========================================================================
			// 7. CONFIGURACIÓN DEL MÓDULO AUDIO
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
				ImGui::BulletText("RAM del Proceso (Working Set): %zu MB (Pico: %zu MB)", perf.memoryUsageMB, perf.peakMemoryUsageMB);
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
