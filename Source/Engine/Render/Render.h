#pragma once

#include "../Module.h"
#include "SDL3/SDL.h"

//GLM Math Library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//Basic Geometry
#include "Primitives/GeometryGenerator.h"
#include "Texture/TextureLoader.h"

//3DModels
#include "3DModels/ModelLoader.h"

#include "../Scene/GameObjects/GameObject.h"

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

	// Render settings
	bool depthTestEnabled = true;
	bool cullFaceEnabled = true;
	bool lightingEnabled = true;
	bool wireframeMode = false;

	unsigned int GetSceneTexture() const { return textureColorbuffer; }

	void ImportModel(std::string filePath);

private:
	SDL_Color background;

	unsigned int framebuffer = 0;
	unsigned int textureColorbuffer = 0;
	unsigned int rbo = 0;
	void CreateFramebuffer(int width, int height);

	unsigned int VBO, VAO, EBO;
	unsigned int shaderProgram;

	//Scene
	void DrawGameObject(GameObject* go, int modelLoc, int colorLoc, int useTexLoc, int texLoc, const glm::mat4& parentMatrix);

};
