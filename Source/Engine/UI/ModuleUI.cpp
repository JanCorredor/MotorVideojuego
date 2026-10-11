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

#include <glm/gtc/type_ptr.hpp>

// ImGui headers (adjust paths as needed depending on your project structure)
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <SDL3/SDL.h>

#include <tracy/Tracy.hpp>

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

	// Enable a DockSpace over the entire main viewport so windows can be docked properly
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	// Scene panel
	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene");
	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
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

	// Hierarchy panel
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Hierarchy");
	if (Engine::GetInstance().scene) {
		GameObject* root = Engine::GetInstance().scene->GetRoot();
		if (root) {
			DrawHierarchyNode(root);
		}
	}
	ImGui::End();

	// Inspector panel
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Inspector");
	if (selectedGameObject) {
		ImGui::Text("Name: %s", selectedGameObject->GetName().c_str());
		
		ImGui::Separator();
		

		TransformComponent* transform = selectedGameObject->GetComponent<TransformComponent>();
		if (transform) 
		{
			ImGui::Text("Transform");
			glm::vec3 pos = transform->GetPosition();
			if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f)) transform->SetPosition(pos);
			
			glm::vec3 rot = transform->GetRotation();
			if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), 1.0f)) transform->SetRotation(rot);
			
			glm::vec3 scl = transform->GetScale();
			if (ImGui::DragFloat3("Scale", glm::value_ptr(scl), 0.1f)) transform->SetScale(scl);
		} 
		else 
		{
			ImGui::Text("No Transform Component attached.");
		}

		MeshComponent* mesh = selectedGameObject->GetComponent<MeshComponent>();
		if (mesh) 
		{
			ImGui::Text("Mesh");
			ImGui::Text("Primitive: %s", mesh->GetPrimitive());
		}
		else
		{
			ImGui::Text("No Mesh Component attached.");
		}

		MaterialComponent* mat = selectedGameObject->GetComponent<MaterialComponent>();
		if (mat)
		{
			ImGui::Text("Material");

			ImGui::Text("TextureID: %d", mat->GetTextureID());

			bool usingTexture = mat->GetUseTexture();
			if (ImGui::Checkbox("UseTexture", &usingTexture)) mat->SetUseTexture(usingTexture);


			glm::vec3 color = mat->GetColor(); //TODO :: ImGuiColorEditFlags_PickerHueWheel ImGuiColorEditFlags_NoBorder
			if (ImGui::ColorPicker3("Color", glm::value_ptr(color))) mat->SetColor(color);
		}
		else
		{
			ImGui::Text("No Material Component attached.");
		}



	} 
	else 
	{
		ImGui::Text("Select a GameObject to inspect");
	}
	ImGui::End();

	// Render Options panel
	ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
	ImGui::Begin("Render Settings");
	ImGui::Checkbox("Depth Test", &Engine::GetInstance().render->depthTestEnabled);
	ImGui::Checkbox("Cull Face", &Engine::GetInstance().render->cullFaceEnabled);
	ImGui::Checkbox("Lighting", &Engine::GetInstance().render->lightingEnabled);
	ImGui::Checkbox("Wireframe Mode", &Engine::GetInstance().render->wireframeMode);
	ImGui::End();

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
			filePath.find(".jpg") != std::string::npos)
		{
			MaterialComponent* mat = selectedGameObject->GetComponent<MaterialComponent>();
			mat->SetTexture(filePath.c_str());
			mat->SetUseTexture(true);
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
