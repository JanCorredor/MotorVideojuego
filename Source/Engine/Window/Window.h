#pragma once

#include "../Module.h"
#include <SDL3/SDL.h>
#include <string>

class Window : public Module
{
public:

	// Límites mínimos de la ventana (se aplican tanto al redimensionar con el ratón como desde el Editor)
	static constexpr int MIN_WIDTH = 640;
	static constexpr int MIN_HEIGHT = 480;

	Window();

	// Destructor
	virtual ~Window();

	// Called before render is available
	bool Awake();

	// Called before quitting
	bool CleanUp();

	// Título de la ventana nativa (fuente de verdad: el std::string interno)
	void SetTitle(const std::string& newTitle);
	const std::string& GetTitle() const { return title; }

	// Sufijo dinámico de la escena activa en la barra de título (ej. " - SinGuardar*" o " - MiEscena")
	void SetSceneTitleInfo(const std::string& suffix);
	const std::string& GetSceneTitleInfo() const { return sceneTitleSuffix; }

	//Get window
	SDL_Window* GetWindow() { return window;}

	//Get Open GL Context
	SDL_GLContext GetGLContext() const { return glContext;}

	// Tamaño cacheado de la ventana (siempre sincronizado con SDL a través de RefreshSize)
	int GetWidth() const { return windowWidth; }
	int GetHeight() const { return windowHeight; }

	// Aplica un nuevo tamaño en ventana (ignorado en fullscreen / maximizada)
	void SetSize(int width, int height);

	// Consulta el tamaño real a SDL y actualiza el estado interno.
	// La invoca Input al recibir SDL_EVENT_WINDOW_RESIZED / SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED.
	void RefreshSize();

	// Retrieve window scale
	int GetScale() const;

	void SetFullscreen(bool enabled);
	bool IsFullscreen() const;

	void SetResizable(bool resizable);
	bool IsResizable() const;

	void SetBorderless(bool borderless);
	bool IsBorderless() const;

private:
	bool HasFlag(SDL_WindowFlags flag) const;

	// The window we'll be rendering to
	SDL_Window* window;

	// OpenGL context
	SDL_GLContext glContext = nullptr;

	std::string title = "Motor Videojuego";
	std::string sceneTitleSuffix = " - Untitled";
	std::string lastAppliedWindowTitle;
	int windowWidth = 1280;
	int windowHeight = 780;
	int windowScale = 1;
};
