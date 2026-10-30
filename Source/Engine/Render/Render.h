#pragma once

#include "../Module.h"
#include "RenderSettings.h"

#include "SDL3/SDL.h"
#include <glm/glm.hpp>
#include <vector>

class GameObject;

class Render : public Module
{
public:
	Render();
	virtual ~Render();

	// Called before render is available
	bool Awake() override;

	// Called before the first frame
	bool Start() override;

	// Called each loop iteration
	bool PreUpdate() override;
	bool Update(float dt) override;
	bool PostUpdate() override;

	// Called before quitting
	bool CleanUp() override;

	// Set background color
	void SetBackgroundColor(SDL_Color color);
	SDL_Color GetBackgroundColor() const { return background; }

	// Render settings (única fuente de verdad)
	const RenderSettings& GetSettings() const { return settings; }
	void SetSettings(const RenderSettings& newSettings);

	void SetVSync(bool enabled);
	bool GetVSync() const { return settings.vsync; }

	// Control en tiempo real de Grid y Axis
	bool IsGridEnabled() const { return settings.showGrid; }
	void SetGridEnabled(bool enabled) { settings.showGrid = enabled; }

	bool IsAxisEnabled() const { return settings.showAxis; }
	void SetAxisEnabled(bool enabled) { settings.showAxis = enabled; }

	// Framebuffer de la escena
	unsigned int GetFramebufferID() const { return framebuffer; }
	unsigned int GetSceneTexture() const { return textureColorbuffer; }
	unsigned int GetRboID() const { return rbo; }
	bool IsFramebufferComplete() const { return fboComplete; }

	void SetViewportSize(int width, int height);
	int GetViewportWidth() const { return viewportWidth; }
	int GetViewportHeight() const { return viewportHeight; }

	// El Editor indica si el viewport de la escena es visible. Si no lo es, no se renderiza el pase 3D.
	void SetViewportVisible(bool visible) { viewportVisible = visible; }
	bool IsViewportVisible() const { return viewportVisible; }

	// Framebuffer de la vista Game
	unsigned int GetGameTexture() const { return gameTextureColorbuffer; }
	void SetGameViewportSize(int width, int height);
	int GetGameViewportWidth() const { return gameViewportWidth; }
	int GetGameViewportHeight() const { return gameViewportHeight; }
	void SetGameViewportVisible(bool visible) { gameViewportVisible = visible; }
	bool IsGameViewportVisible() const { return gameViewportVisible; }

	//Z Buffer
	unsigned int GetDepthTexture() const { return linearDepthTexture; }

private:
	RenderSettings settings;
	SDL_Color background;
	bool viewportVisible = true;

	// Evita reallocations constantes reservando espacio previo
	static constexpr size_t INITIAL_DEBUG_BUFFER_CAPACITY = 10000;

	// Guardar último tamaño procesado para evitar recreate continuo
	int lastRequestedWidth = 0;
	int lastRequestedHeight = 0;

	// Framebuffer de la escena
	unsigned int framebuffer = 0;
	unsigned int textureColorbuffer = 0;
	unsigned int rbo = 0;
	bool fboComplete = false;
	int fboWidth = 0;
	int fboHeight = 0;
	int viewportWidth = 800;
	int viewportHeight = 600;
	void CreateFramebuffer(int width, int height);
	void DestroyFramebuffer();

	// Framebuffer de la vista Game
	bool gameViewportVisible = true;
	unsigned int gameFramebuffer = 0;
	unsigned int gameTextureColorbuffer = 0;
	unsigned int gameDepthTextureBuffer = 0;
	bool gameFboComplete = false;
	int gameFboWidth = 0;
	int gameFboHeight = 0;
	int gameViewportWidth = 800;
	int gameViewportHeight = 600;
	void CreateGameFramebuffer(int width, int height);
	void DestroyGameFramebuffer();


	// Shaders
	unsigned int shaderProgram = 0;
	unsigned int lineShaderProgram = 0;
	unsigned int singleColorShaderProgram = 0; // Shader para el outline

	//ZBuffer
	unsigned int depthTextureBuffer = 0; // Texture handle for the Z-Buffer

	unsigned int depthDebugProgram = 0;
	unsigned int depthDebugVAO = 0, depthDebugVBO = 0;
	unsigned int linearDepthTexture = 0;

	struct SceneUniforms
	{
		int model = -1, view = -1, projection = -1;
		int color = -1, useTexture = -1, texture = -1, lighting = -1;
	} u;

	struct LineUniforms
	{
		int model = -1, view = -1, projection = -1, color = -1;
	} lu;

	struct SingleColorUniforms
	{
		int model = -1, view = -1, projection = -1, color = -1;
		int scaleFactor = -1;
	} scu;

	// Rejilla de referencia (Grid) y Ejes centrales (Axis) en buffers estáticos persistentes
	unsigned int gridVAO = 0;
	unsigned int gridVBO = 0;
	int gridVertexCount = 0;

	unsigned int axisVAO = 0;
	unsigned int axisVBO = 0;
	int axisVertexCount = 0;

	void CreateGridAndAxisBuffers();
	void DestroyGridAndAxisBuffers();
	void DrawGridAndAxis(const glm::mat4& view, const glm::mat4& projection);

	// Dibujo de depuración (normales + contorno de selección): un único VAO/VBO persistente
	unsigned int debugVAO = 0;
	unsigned int debugVBO = 0;
	size_t debugVBOCapacityBytes = 0;
	std::vector<float> debugVertexLines; // reutilizado entre frames (sin realloc)
	std::vector<float> debugFaceLines;

	void RenderScene();
	void RenderGame();
	void BindDefaultFramebuffer();

	void DrawSingleGameObject(GameObject* go, const glm::mat4& parentMatrix);
	void DrawDebug(const glm::mat4& view, const glm::mat4& projection);
	void CollectNormals(GameObject* go, const glm::mat4& parentMatrix);
	void AppendNormals(GameObject* go, const glm::mat4& worldMatrix);
	void DrawSelectionOutline(GameObject* selected, const glm::mat4& view, const glm::mat4& projection);};
