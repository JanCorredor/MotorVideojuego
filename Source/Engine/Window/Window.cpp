#include "Window.h"
#include "../../Utils/Log.h"
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
	LOG("[SDL3] Inicializando subsistema de video...");
	
	if (SDL_Init(SDL_INIT_VIDEO) != true)
	{
		LOG_ERROR("[SDL3] No se pudo inicializar SDL_VIDEO: %s", SDL_GetError());
		return false;
	}

	int sdlVersion = SDL_GetVersion();
	LOG("[SDL3] Inicializado correctamente (Versión SDL: %d.%d.%d)",
		SDL_VERSIONNUM_MAJOR(sdlVersion), SDL_VERSIONNUM_MINOR(sdlVersion), SDL_VERSIONNUM_MICRO(sdlVersion));

	// Open GL Config
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	// Create window
	Uint32 flags = SDL_WINDOW_OPENGL;

#ifdef WINDOW_FULLSCREEN
	flags |= SDL_WINDOW_FULLSCREEN;
#endif
#ifdef WINDOW_RESIZEABLE
	flags |= SDL_WINDOW_RESIZABLE;
#endif

	window = SDL_CreateWindow("Motor Videojuego", windowWidth, windowHeight, flags);

	if (window == NULL)
	{
		LOG_ERROR("[SDL3] Error al crear ventana (%dx%d): %s", windowWidth, windowHeight, SDL_GetError());
		return false;
	}

	LOG("[SDL3] Ventana creada con éxito (%dx%d, flags: 0x%X)", windowWidth, windowHeight, flags);

	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(window);

	// Open GL context
	glContext = SDL_GL_CreateContext(window);

	if (glContext == nullptr)
	{
		LOG_ERROR("[OpenGL] Error al crear contexto OpenGL: %s", SDL_GetError());

		SDL_DestroyWindow(window);
		window = nullptr;
		SDL_Quit();

		return false;
	}

	LOG("[OpenGL] Contexto OpenGL 4.6 Core Profile creado exitosamente.");
	SDL_GL_SetSwapInterval(1);

	// Start GLAD (Load OpenGL Functions with GLAD)
	LOG("[GLAD] Cargando punteros de funciones OpenGL...");
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)))
	{
		LOG_ERROR("[GLAD] Fallo crítico al inicializar punteros GLAD con SDL_GL_GetProcAddress");

		SDL_GL_DestroyContext(glContext);
		glContext = nullptr;
		SDL_DestroyWindow(window);
		window = nullptr;
		SDL_Quit();

		return false;
	}

	LOG("[OpenGL] Driver detectado: %s", glGetString(GL_RENDERER));
	LOG("[OpenGL] Fabricante: %s", glGetString(GL_VENDOR));
	LOG("[OpenGL] Versión de OpenGL: %s", glGetString(GL_VERSION));
	LOG("[OpenGL] Versión de Shading Language (GLSL): %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

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

void Window::SetWindowSize(int width, int height)
{
	if (width >= 640 && height >= 480 && window != nullptr)
	{
		windowWidth = width;
		windowHeight = height;
		SDL_SetWindowSize(window, width, height);
	}
}

int Window::GetScale() const
{
	return windowScale;
}

void Window::SetResizable(bool resizable)
{
	if (window != nullptr)
	{
		SDL_SetWindowResizable(window, resizable);
	}
}

bool Window::IsResizable() const
{
	if (window != nullptr)
	{
		return (SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) != 0;
	}
	return false;
}

void Window::SetBorderless(bool borderless)
{
	if (window != nullptr)
	{
		SDL_SetWindowBordered(window, !borderless);
	}
}

bool Window::IsBorderless() const
{
	if (window != nullptr)
	{
		return (SDL_GetWindowFlags(window) & SDL_WINDOW_BORDERLESS) != 0;
	}
	return false;
}

const char* Window::GetTitle() const
{
	if (window != nullptr)
	{
		const char* t = SDL_GetWindowTitle(window);
		if (t != nullptr) return t;
	}
	return title.c_str();
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
