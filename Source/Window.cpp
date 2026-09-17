#include "Window.h"
#include "Log.h"
#include <glad/glad.h>

Window::Window() : Module()
{
	window = NULL;
	glContext = nullptr;
	name = "window";
}

// Destructor
Window::~Window()
{
}

// Called before render is available
bool Window::Awake()
{
	LOG("Init SDL window & surface");
	
	if (SDL_Init(SDL_INIT_VIDEO) != true)
	{
		LOG("SDL_VIDEO could not initialize! SDL_Error: %s\n", SDL_GetError());
		return false;
	}

	//Open GL Config
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);


	// Create window
	Uint32 flags = SDL_WINDOW_OPENGL;

	window = SDL_CreateWindow("Motor Videojuego", windowWidth, windowHeight, flags);

	if (window == NULL)
	{
		LOG("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		return false;
	}

	//if (fullscreen_window == true) TODO
	//{
	//	SDL_SetWindowFullscreenMode(window, nullptr); // use desktop resolution
	//	SDL_SetWindowFullscreen(window, true);
	//}
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(window);

	//Open GL context
	glContext = SDL_GL_CreateContext(window);

	if (glContext == nullptr)
	{
		LOG("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());

		SDL_DestroyWindow(window);
		window = nullptr;

		SDL_Quit();

		return false;
	}

	//VSync? SDL_GL_SetSwapInterval(vsync ? 1 : 0); TODO
	SDL_GL_SetSwapInterval(1);

	// Start GLAD (Load OpenGL Functions with GLAD)
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
	{
		LOG("Failed to initialize GLAD\n");

		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;

		SDL_DestroyWindow(window);
		window = nullptr;

		SDL_Quit();

		return false;
	}

	LOG("OpenGL initialized: %s\n", glGetString(GL_VERSION));

	return true;
}

// Called before quitting
bool Window::CleanUp()
{
	LOG("Destroying SDL window and quitting all SDL systems");
	
	// Destroy Open GL COntext
	if (glContext != nullptr)
	{
		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
	}


	// Destroy window
	if (window != nullptr)
	{
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	// Quit SDL subsystems
	SDL_Quit();
	return true;
}

// Set new window title
void Window::SetTitle(const char* new_title)
{
	SDL_SetWindowTitle(window, new_title);
}

void Window::GetWindowSize(int& width, int& height) const
{
	SDL_GetWindowSize(window, &width, &height);
}

int Window::GetScale() const
{
	return windowScale;
}

void Window::SetFullscreen(bool enabled, SDL_Renderer* renderer) 
{
	// Save mouse Position
	float mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	int oldW, oldH;
	SDL_GetWindowSize(window, &oldW, &oldH);

	float relX = (oldW > 0) ? mouseX / (float)oldW : 0.5f;
	float relY = (oldH > 0) ? mouseY / (float)oldH : 0.5f;

	// Fullscreen mode aplay
	isFullscreen = enabled;
	if (enabled) {
		SDL_SetWindowFullscreen(window, true);
		SDL_SetWindowMouseGrab(window, true);
	}
	else {
		SDL_SetWindowFullscreen(window, false);
		SDL_SetWindowMouseGrab(window, false);
	}

	SDL_SyncWindow(window);

	// Resize and reposition the cursor in the same relative position
	int newW, newH;
	SDL_GetWindowSize(window, &newW, &newH);
	//SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);

	SDL_WarpMouseInWindow(window, relX * newW, relY * newH);


	SDL_SetRenderLogicalPresentation(renderer, windowWidth, windowHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);

}
