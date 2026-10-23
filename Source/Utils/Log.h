#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstdio>

enum class LogType
{
	Info = 0,
	Warning,
	Error
};

struct LogEntry
{
	LogType type = LogType::Info;
	std::string timestamp; // "HH:MM:SS"
	std::string text;
	std::string file;
	int line = 0;
	int count = 1; // Siempre 1: la agrupación (Collapse) es una decisión de la vista
};

// Sistema de log único y thread-safe. El Editor lo consume sin copiar el buffer
// (ForEach) y detecta cambios mediante GetRevision().
class LogSystem
{
public:
	static LogSystem& GetInstance()
	{
		static LogSystem instance;
		return instance;
	}

	void AddLog(LogType type, const char* file, int line, const char* format, ...);
	void AddLogVA(LogType type, const char* file, int line, const char* format, va_list args);

	// Recorre las entradas bajo lock, sin copiarlas.
	template <typename F>
	void ForEach(F&& fn) const
	{
		std::lock_guard<std::mutex> lock(logMutex);
		int index = 0;
		for (const LogEntry& entry : logs)
		{
			fn(index++, entry);
		}
	}

	// Copia de una única entrada (para el panel de detalle). Devuelve false si el índice ya no existe.
	bool GetEntry(int index, LogEntry& out) const
	{
		std::lock_guard<std::mutex> lock(logMutex);
		if (index < 0 || index >= (int)logs.size()) return false;
		out = logs[index];
		return true;
	}

	std::vector<LogEntry> GetLogs() const
	{
		std::lock_guard<std::mutex> lock(logMutex);
		return std::vector<LogEntry>(logs.begin(), logs.end());
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(logMutex);

		std::deque<LogEntry>().swap(logs);
		
		infoCount = 0;
		warningCount = 0;
		errorCount = 0;
		revision++;
	}

	// Se incrementa con cada cambio (nueva entrada o Clear)
	uint64_t GetRevision() const { return revision.load(); }

	int GetInfoCount() const { return infoCount.load(); }
	int GetWarningCount() const { return warningCount.load(); }
	int GetErrorCount() const { return errorCount.load(); }

private:
	LogSystem() = default;
	mutable std::mutex logMutex;
	std::deque<LogEntry> logs;
	std::atomic<int> infoCount{ 0 };
	std::atomic<int> warningCount{ 0 };
	std::atomic<int> errorCount{ 0 };
	std::atomic<uint64_t> revision{ 0 };
	const size_t maxLogs = 500;
};

// Macros de logging con captura automática de fichero y línea
#define LOG(format, ...) LogSystem::GetInstance().AddLog(LogType::Info, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) LogSystem::GetInstance().AddLog(LogType::Warning, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LogSystem::GetInstance().AddLog(LogType::Error, __FILE__, __LINE__, format, ##__VA_ARGS__)