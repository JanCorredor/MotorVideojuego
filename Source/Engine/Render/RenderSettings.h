#pragma once

// Modo de visualización de normales (solo depuración, lo dibuja Render)
enum class NormalsMode
{
	None = 0,
	Vertex,  // una línea por vértice
	Face,    // una línea por triángulo (desde su centroide)
	Both
};

// Única fuente de verdad de las opciones de render. El Editor las edita a través de
// Render::SetSettings(); nadie más guarda copias persistentes de estos valores.
struct RenderSettings
{
	bool depthTest = true;
	bool cullFace = true;
	bool lighting = true;
	bool wireframe = false;
	bool vsync = true;

	NormalsMode normals = NormalsMode::None;
	bool normalsAllObjects = false;   // false: solo el GameObject seleccionado
	bool highlightSelection = true;   // contorno del GameObject seleccionado
};
