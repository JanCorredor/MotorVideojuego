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

private:
	SDL_Color background;

	unsigned int VBO, VAO, EBO;
	unsigned int shaderProgram;

	float rotationAngle = 0.0f;

	//Scene
	std::unique_ptr<Primitive> cube;
	
	std::unique_ptr<Primitive> sphere;
	unsigned int sphereTextureID;

	std::unique_ptr<Primitive> pyramid;

	std::unique_ptr<Primitive> loadedModel = nullptr;

};
