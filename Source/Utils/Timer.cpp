// ----------------------------------------------------
// Timer with milisecons precision
// ----------------------------------------------------

#include "Timer.h"
#include <SDL3\SDL_timer.h>

Timer::Timer()
{
	Start();
}

void Timer::Start()
{
	running = true;
	paused = false;
	startTime = (unsigned int)SDL_GetTicks();
	pausedTime = 0;
}

void Timer::Stop()
{
	running = false;
	paused = false;
	startTime = 0;
	pausedTime = 0;
}

void Timer::Pause()
{
	if (running && !paused)
	{
		paused = true;
		pausedTime = (unsigned int)SDL_GetTicks() - startTime;
	}
}

void Timer::Resume()
{
	if (running && paused)
	{
		paused = false;
		startTime = (unsigned int)SDL_GetTicks() - pausedTime;
		pausedTime = 0;
	}
}

int Timer::ReadSec() const
{
	return (int)(ReadMSec() / 1000.0f);
}

float Timer::ReadSecFloat() const
{
	return ReadMSec() / 1000.0f;
}

float Timer::ReadMSec() const
{
	if (!running) return 0.0f;
	if (paused) return (float)pausedTime;
	return (float)((unsigned int)SDL_GetTicks() - startTime);
}