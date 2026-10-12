#include "Log.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void LogSystem::AddLog(LogType type, const char* file, int line, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	AddLogVA(type, file, line, format, args);
	va_end(args);
}

void LogSystem::AddLogVA(LogType type, const char* file, int line, const char* format, va_list args)
{
	static char buffer[4096];
	vsnprintf(buffer, sizeof(buffer), format, args);

	// Obtener hora actual en formato HH:MM:SS
	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	std::tm tm_now;
#if defined(_WIN32)
	localtime_s(&tm_now, &now_time);
#else
	localtime_r(&now_time, &tm_now);
#endif
	std::stringstream ss;
	ss << std::put_time(&tm_now, "%H:%M:%S");
	std::string timestamp = ss.str();

	std::string msgText(buffer);
	std::string filename = file ? file : "";

	// Acortar la ruta del archivo para mostrar sólo el nombre de archivo
	size_t lastSlash = filename.find_last_of("\\/");
	if (lastSlash != std::string::npos)
	{
		filename = filename.substr(lastSlash + 1);
	}

	std::lock_guard<std::mutex> lock(logMutex);

	// Agrupación si el último mensaje es exactamente el mismo (Collapse)
	if (!logs.empty() && logs.back().type == type && logs.back().text == msgText)
	{
		logs.back().count++;
	}
	else
	{
		LogEntry entry;
		entry.type = type;
		entry.timestamp = timestamp;
		entry.text = msgText;
		entry.file = filename;
		entry.line = line;
		entry.count = 1;

		if (logs.size() >= maxLogs)
		{
			// Control de límite de memoria
			if (logs.front().type == LogType::Info) infoCount--;
			else if (logs.front().type == LogType::Warning) warningCount--;
			else if (logs.front().type == LogType::Error) errorCount--;
			logs.erase(logs.begin());
		}

		logs.push_back(entry);
	}

	// Actualizar contadores
	if (type == LogType::Info) infoCount++;
	else if (type == LogType::Warning) warningCount++;
	else if (type == LogType::Error) errorCount++;

	// Salida estándar y debug output
	std::string prefix;
	switch (type)
	{
	case LogType::Info:    prefix = "[INFO]"; break;
	case LogType::Warning: prefix = "[WARNING]"; break;
	case LogType::Error:   prefix = "[ERROR]"; break;
	}

	std::string consoleOutput = "[" + timestamp + "]" + prefix + " (" + filename + ":" + std::to_string(line) + ") " + msgText + "\n";
	std::cout << consoleOutput;

#ifdef _WIN32
	OutputDebugStringA(consoleOutput.c_str());
#endif
}

// Retrocompatibilidad con el motor existente
void Log(const char file[], int line, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogSystem::GetInstance().AddLogVA(LogType::Info, file, line, format, args);
	va_end(args);
}

void Log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogSystem::GetInstance().AddLogVA(LogType::Info, "", 0, format, args);
	va_end(args);
}

void Warning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogSystem::GetInstance().AddLogVA(LogType::Warning, "", 0, format, args);
	va_end(args);
}

void Error(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogSystem::GetInstance().AddLogVA(LogType::Error, "", 0, format, args);
	va_end(args);
}
