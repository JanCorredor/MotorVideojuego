#pragma once

#include "../Module.h"
#include <SDL3/SDL.h>

class Window : public Module
{
public:

	Window();

	// Destructor
	virtual ~Window();

	// Called before render is available
	bool Awake();

	// Called before quitting
	bool CleanUp();

	// Change title
	void SetTitle(const char* title);

	//Get window
	SDL_Window* GetWindow() { return window;}

	//Get Open GL Context
	SDL_GLContext GetGLContext() const { return glContext;}

	// Retrive window size
	void GetWindowSize(int& width, int& height) const;
	void SetWindowSize(int width, int height);

	// Retrieve window scale
	int GetScale() const;

	void SetFullscreen(bool enabled, SDL_Renderer* renderer = nullptr);
	bool IsFullscreen() const { return isFullscreen; }

	void SetResizable(bool resizable);
	bool IsResizable() const;

	void SetBorderless(bool borderless);
	bool IsBorderless() const;

	const char* GetTitle() const;

private:
	// The window we'll be rendering to
	SDL_Window* window;

	// OpenGL context
	SDL_GLContext glContext = nullptr;

	std::string title;
	int windowWidth = 1280;
	int windowHeight = 780;
	int windowScale = 1;

	bool isFullscreen = false;
};
