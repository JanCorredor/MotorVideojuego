#pragma once

#include "../Module.h"

#include "SDL3/SDL.h"
#include <glm/glm.hpp>

class GameObject;

class Render : public Module
{
public:

	Render();

	// Destructor
	virtual ~Render();

	// Called before render is available
	bool Awake();

	// Called before the first frame
	bool Start();

	// Called each loop iteration
	bool PreUpdate();
	bool Update(float dt);
	bool PostUpdate();

	// Called before quitting
	bool CleanUp();

	// Set background color
	void SetBackgroundColor(SDL_Color color);
	SDL_Color GetBackgroundColor() const { return background; }

	// Render settings
	bool depthTestEnabled = true;
	bool cullFaceEnabled = true;
	bool lightingEnabled = true;
	bool wireframeMode = false;
	bool showNormals = false;
	
	bool vsync = true;
	void SetVSync(bool enabled);
	bool GetVSync() const { return vsync; }

	unsigned int GetFramebufferID() const { return framebuffer; }
	unsigned int GetSceneTexture() const { return textureColorbuffer; }
	unsigned int GetRboID() const { return rbo; }

	void SetViewportSize(int width, int height);
	int GetViewportWidth() const { return viewportWidth; }
	int GetViewportHeight() const { return viewportHeight; }

	void ImportModel(std::string filePath);

private:
	SDL_Color background;

	unsigned int framebuffer = 0;
	unsigned int textureColorbuffer = 0;
	unsigned int rbo = 0;
	int fboWidth = 0;
	int fboHeight = 0;
	int viewportWidth = 800;
	int viewportHeight = 600;
	void CreateFramebuffer(int width, int height);

	unsigned int VBO, VAO, EBO;
	unsigned int shaderProgram;
	unsigned int lineShaderProgram = 0;

	//Scene
	void DrawGameObject(GameObject* go, int modelLoc, int colorLoc, int useTexLoc, int texLoc, const glm::mat4& parentMatrix);
	void DrawNormals(GameObject* go, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& parentMatrix);
};
