#pragma once

#include <string>
#include <vector>
#include <mutex>
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
	int count = 1;
};

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

	std::vector<LogEntry> GetLogs() const
	{
		std::lock_guard<std::mutex> lock(logMutex);
		return logs;
	}

	void Clear()
	{
		std::lock_guard<std::mutex> lock(logMutex);
		logs.clear();
		infoCount = 0;
		warningCount = 0;
		errorCount = 0;
	}

	int GetInfoCount() const { return infoCount; }
	int GetWarningCount() const { return warningCount; }
	int GetErrorCount() const { return errorCount; }

private:
	LogSystem() = default;
	mutable std::mutex logMutex;
	std::vector<LogEntry> logs;
	int infoCount = 0;
	int warningCount = 0;
	int errorCount = 0;
	const size_t maxLogs = 2000;
};

// Retrocompatibilidad con el motor existente
void Log(const char file[], int line, const char* format, ...);

// Funciones globales estándar
void Log(const char* format, ...);
void Warning(const char* format, ...);
void Error(const char* format, ...);

// Macros de logging con captura automática de fichero y línea
#define LOG(format, ...) LogSystem::GetInstance().AddLog(LogType::Info, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) LogSystem::GetInstance().AddLog(LogType::Warning, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LogSystem::GetInstance().AddLog(LogType::Error, __FILE__, __LINE__, format, ##__VA_ARGS__)