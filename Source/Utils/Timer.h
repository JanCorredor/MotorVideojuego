#pragma once

class Timer
{
public:

	// Constructor
	Timer();

	void Start();
	void Stop();
	void Pause();
	void Resume();

	int ReadSec() const;
	float ReadSecFloat() const;
	float ReadMSec() const;

	bool IsRunning() const { return running; }
	bool IsPaused() const { return paused; }

private:
	unsigned int startTime = 0;
	unsigned int pausedTime = 0;
	bool running = false;
	bool paused = false;
};