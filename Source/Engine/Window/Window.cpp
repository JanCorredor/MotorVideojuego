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

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

	// Create window
	Uint32 flags = SDL_WINDOW_OPENGL;

#ifdef WINDOW_FULLSCREEN
	flags |= SDL_WINDOW_FULLSCREEN;
#endif
#ifdef WINDOW_RESIZEABLE
	flags |= SDL_WINDOW_RESIZABLE;
#endif

	window = SDL_CreateWindow(title.c_str(), windowWidth, windowHeight, flags);

	if (window == NULL)
	{
		LOG_ERROR("[SDL3] Error al crear ventana (%dx%d): %s", windowWidth, windowHeight, SDL_GetError());
		return false;
	}

	LOG("[SDL3] Ventana creada con éxito (%dx%d, flags: 0x%X)", windowWidth, windowHeight, flags);

	SDL_SetWindowMinimumSize(window, MIN_WIDTH, MIN_HEIGHT);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_ShowWindow(window);
	SDL_RaiseWindow(window);
	RefreshSize();

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
void Window::SetTitle(const std::string& newTitle)
{
	title = newTitle;
	if (window != nullptr)
	{
		SDL_SetWindowTitle(window, title.c_str());
	}
}

void Window::SetSize(int width, int height)
{
	if (window == nullptr) return;

	// SDL_SetWindowSize solo afecta al tamaño en modo ventana
	if (HasFlag(SDL_WINDOW_FULLSCREEN) || HasFlag(SDL_WINDOW_MAXIMIZED)) return;

	width = SDL_max(width, MIN_WIDTH);
	height = SDL_max(height, MIN_HEIGHT);
	if (width == windowWidth && height == windowHeight) return;

	if (SDL_SetWindowSize(window, width, height))
	{
		// Se refleja de inmediato en el Editor; si el SO ajusta el tamaño final,
		// el evento SDL_EVENT_WINDOW_RESIZED lo corregirá a través de RefreshSize()
		windowWidth = width;
		windowHeight = height;
	}
	else
	{
		LOG_WARNING("[SDL3] No se pudo cambiar el tamaño de la ventana a %dx%d: %s", width, height, SDL_GetError());
	}
}

void Window::RefreshSize()
{
	if (window == nullptr) return;

	int w = 0, h = 0;
	if (SDL_GetWindowSize(window, &w, &h) && w > 0 && h > 0)
	{
		windowWidth = w;
		windowHeight = h;
	}
}

int Window::GetScale() const
{
	return windowScale;
}

bool Window::HasFlag(SDL_WindowFlags flag) const
{
	return window != nullptr && (SDL_GetWindowFlags(window) & flag) != 0;
}

void Window::SetResizable(bool resizable)
{
	if (window == nullptr) return;

	SDL_SetWindowResizable(window, resizable);
	SDL_SyncWindow(window);
	RefreshSize();
}

bool Window::IsResizable() const
{
	return HasFlag(SDL_WINDOW_RESIZABLE);
}

void Window::SetBorderless(bool borderless)
{
	if (window == nullptr) return;

	SDL_SetWindowBordered(window, !borderless);
	SDL_SyncWindow(window);
	RefreshSize();
}

bool Window::IsBorderless() const
{
	return HasFlag(SDL_WINDOW_BORDERLESS);
}

bool Window::IsFullscreen() const
{
	return HasFlag(SDL_WINDOW_FULLSCREEN);
}

void Window::SetFullscreen(bool enabled)
{
	if (window == nullptr) return;

	// Save mouse Position
	float mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	const float relX = (windowWidth > 0) ? mouseX / (float)windowWidth : 0.5f;
	const float relY = (windowHeight > 0) ? mouseY / (float)windowHeight : 0.5f;

	SDL_SetWindowFullscreen(window, enabled);
	SDL_SetWindowMouseGrab(window, enabled);
	SDL_SyncWindow(window);

	// Tamaño real tras el cambio de modo
	RefreshSize();

	// Reposition the cursor in the same relative position
	SDL_WarpMouseInWindow(window, relX * windowWidth, relY * windowHeight);
}
