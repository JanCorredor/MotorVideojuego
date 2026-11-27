#include "GameClock.h"

void GameClock::Play()
{
	if (state == SimulationState::Stopped)
	{
		Reset();
		state = SimulationState::Playing;
	}
	else if (state == SimulationState::Paused)
	{
		state = SimulationState::Playing;
	}
}

void GameClock::Pause()
{
	if (state == SimulationState::Playing)
	{
		state = SimulationState::Paused;
		dtMs = 0.0f;
	}
}

void GameClock::Step()
{
	if (state == SimulationState::Paused)
	{
		stepRequested = true;
	}
}

void GameClock::Stop()
{
	state = SimulationState::Stopped;
	Reset();
}

void GameClock::Advance(float realDtMs)
{
	if (state == SimulationState::Playing)
	{
		dtMs = realDtMs * timeScale;
		realTimeSec += realDtMs / 1000.0f;
	}
	else if (state == SimulationState::Paused && stepRequested)
	{
		// Con timeScale 0 el Step avanza a x1: si no, no haría nada
		dtMs = STEP_MS * (timeScale > 0.0f ? timeScale : 1.0f);
		stepRequested = false;
	}
	else
	{
		dtMs = 0.0f;
		return;
	}

	timeSec += dtMs / 1000.0f;
	frameCount++;
}

void GameClock::Reset()
{
	dtMs = 0.0f;
	timeSec = 0.0f;
	realTimeSec = 0.0f;
	frameCount = 0;
	stepRequested = false;
}
