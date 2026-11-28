#pragma once

// Estado de la simulación (Play / Pause / Stop del editor)
enum class SimulationState
{
	Stopped = 0,
	Playing,
	Paused
};

// Reloj de juego: estado de la simulación, escala de tiempo y Step. No depende de SDL: Engine le pasa el dt real.
class GameClock
{
public:
	// Un Step avanza un frame fijo de 60 FPS: en pausa el dt real puede llevar un tirón del editor
	static constexpr float STEP_MS = 1000.0f / 60.0f;

	void Play();   // Stopped -> Playing desde cero | Paused -> Playing
	void Pause();  // Playing -> Paused
	void Step();   // en Paused, el siguiente Advance avanza un frame
	void Stop();   // vuelve a Stopped y pone el reloj a cero

	// Una vez por frame con el dt real en ms: calcula el dt de juego y avanza el reloj
	void Advance(float realDtMs);

	SimulationState GetState() const { return state; }
	float GetDtMs() const { return dtMs; }               // 0 si la simulación no avanza
	float GetTimeSec() const { return timeSec; }         // escalado por timeScale
	float GetRealTimeSec() const { return realTimeSec; } // tiempo real en Playing, sin pausas
	int GetFrameCount() const { return frameCount; }

	float GetTimeScale() const { return timeScale; }
	void SetTimeScale(float scale) { timeScale = (scale < 0.0f) ? 0.0f : scale; }

private:
	void Reset();

	SimulationState state = SimulationState::Stopped;
	float dtMs = 0.0f;
	float timeSec = 0.0f;
	float realTimeSec = 0.0f;
	float timeScale = 1.0f;
	int frameCount = 0;
	bool stepRequested = false;
};
