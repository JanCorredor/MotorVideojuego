#include "Engine.h"
#include "Window.h"
#include "Render.h"
#include "Log.h"

#include <glad/glad.h>

Render::Render() : Module()
{
	name = "render";
	background.r = 0;
	background.g = 0;
	background.b = 0;
	background.a = 0;
}

// Destructor
Render::~Render()
{
}

// Called before render is available
bool Render::Awake()
{
	LOG("Initializing OpenGL renderer");

	LOG( "Renderer: %s\n", glGetString(GL_RENDERER));

	return true;
}

// Called before the first frame
bool Render::Start() {
	LOG("render start");

	return true;
}

// Called each loop iteration
bool Render::PreUpdate()
{
	glClearColor(
		background.r / 255.0f,
		background.g / 255.0f,
		background.b / 255.0f,
		background.a / 255.0f
	);

	glClear(GL_COLOR_BUFFER_BIT);

	return true;
}

bool Render::Update(float dt)
{
	return true;
}

bool Render::PostUpdate()
{
	SDL_Window* window = Engine::GetInstance().window->GetWindow();

	SDL_GL_SwapWindow(window);

	return true;
}

// Called before quitting
bool Render::CleanUp() 
{

	return true;
}

void Render::SetBackgroundColor(SDL_Color color)
{
	background = color;
}
