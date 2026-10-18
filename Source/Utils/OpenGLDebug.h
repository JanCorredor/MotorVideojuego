#pragma once

// Activa el callback de depuración de OpenGL (solo en builds _DEBUG).
// Los mensajes de severidad alta se emiten como LOG_ERROR y los de severidad media como LOG_WARNING.
// Requiere un contexto OpenGL 4.3+ activo y GLAD ya inicializado.
void EnableOpenGLDebugOutput();
