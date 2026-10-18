#include "Log.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
	// Buffer local: AddLogVA puede llamarse desde cualquier hilo
	char buffer[4096];
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
	char timeBuf[16];
	std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_now);

	std::string filename = file ? file : "";

	// Acortar la ruta del archivo para mostrar sólo el nombre de archivo
	size_t lastSlash = filename.find_last_of("\\/");
	if (lastSlash != std::string::npos)
	{
		filename = filename.substr(lastSlash + 1);
	}

	LogEntry entry;
	entry.type = type;
	entry.timestamp = timeBuf;
	entry.text = buffer;
	entry.file = filename;
	entry.line = line;
	entry.count = 1;

	std::string prefix;
	switch (type)
	{
	case LogType::Info:    prefix = "[INFO]"; break;
	case LogType::Warning: prefix = "[WARNING]"; break;
	case LogType::Error:   prefix = "[ERROR]"; break;
	}
	std::string consoleOutput = "[" + entry.timestamp + "]" + prefix + " (" + filename + ":" + std::to_string(line) + ") " + entry.text + "\n";

	{
		std::lock_guard<std::mutex> lock(logMutex);

		if (logs.size() >= maxLogs)
		{
			// Control de límite de memoria
			switch (logs.front().type)
			{
			case LogType::Info:    infoCount--; break;
			case LogType::Warning: warningCount--; break;
			case LogType::Error:   errorCount--; break;
			}
			logs.pop_front();
		}

		logs.push_back(std::move(entry));

		switch (type)
		{
		case LogType::Info:    infoCount++; break;
		case LogType::Warning: warningCount++; break;
		case LogType::Error:   errorCount++; break;
		}
		revision++;

		// Salida estándar bajo el mismo lock para que no se mezclen líneas entre hilos
		std::cout << consoleOutput;
	}

#ifdef _WIN32
	OutputDebugStringA(consoleOutput.c_str());
#endif
}
