#include "Render.h"

//GLM Math Library
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../Scene/GameObjects/GameObject.h"

//////////////////////////
#include "../Engine.h"
#include "../Window/Window.h"
#include "../../Utils/Log.h"
#include "../../Utils/OpenGLDebug.h"

#include <glad/glad.h>

#include "../Scene/Scene.h"
#include "../Scene/GameObjects/Components/TransformComponent.h"
#include "../Scene/GameObjects/Components/MeshComponent.h"
#include "../Scene/GameObjects/Components/MaterialComponent.h"
#include "../Scene/GameObjects/Components/CameraComponent.h"

#include <algorithm>
#include <tracy/Tracy.hpp>

// -----------------------------------------------------------------------------
// Helper: compila y enlaza un programa de shaders registrando cualquier error
// -----------------------------------------------------------------------------
static unsigned int BuildProgram(const char* vertexSource, const char* fragmentSource, const char* label)
{
	char infoLog[512];
	int success = 0;

	unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vertexSource, NULL);
	glCompileShader(vs);
	glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vs, 512, NULL, infoLog);
		LOG_ERROR("[OpenGL] Error compilando Vertex Shader (%s): %s", label, infoLog);
	} else {
		LOG("[OpenGL] Vertex Shader (%s) compilado con éxito", label);
	}

	unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fragmentSource, NULL);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fs, 512, NULL, infoLog);
		LOG_ERROR("[OpenGL] Error compilando Fragment Shader (%s): %s", label, infoLog);
	} else {
		LOG("[OpenGL] Fragment Shader (%s) compilado con éxito", label);
	}

	unsigned int program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		LOG_ERROR("[OpenGL] Error enlazando Shader Program (%s): %s", label, infoLog);
	} else {
		LOG("[OpenGL] Shader Program (%s) enlazado exitosamente (Program ID: %u)", label, program);
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
	return program;
}

Render::Render() : Module()
{
	name = "render";
	background.r = 0;
	background.g = 0;
	background.b = 125;
	background.a = 255;
}

// Destructor
Render::~Render()
{
}

// Called before render is available
bool Render::Awake()
{
	LOG("Initializing OpenGL renderer");

	EnableOpenGLDebugOutput();

	// Enable Depth Test
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Face Culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW); // Counter-Clockwise

	// Alpha
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// V-Sync: Render es el único dueño de este estado
	SetVSync(settings.vsync);

	return true;
}

// Called before the first frame
bool Render::Start() {
	LOG("render start");

	const char* vertexShaderSource = "#version 460 core\n"
		"layout (location = 0) in vec3 aPos;\n"
		"layout (location = 1) in vec3 aNormal;\n"
		"layout (location = 2) in vec2 aTexCoords;\n"
		"out vec3 FragPos;\n"
		"out vec3 Normal;\n"
		"out vec2 TexCoords;\n"
		"uniform mat4 model;\n"
		"uniform mat4 view;\n"
		"uniform mat4 projection;\n"
		"void main()\n"
		"{\n"
		"   FragPos = vec3(model * vec4(aPos, 1.0));\n"
		"   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
		"   TexCoords = aTexCoords;\n"
		"   gl_Position = projection * view * vec4(FragPos, 1.0);\n"
		"}\0";

	const char* fragmentShaderSource = "#version 460 core\n"
		"out vec4 FragColor;\n"
		"in vec3 FragPos;\n"
		"in vec3 Normal;\n"
		"in vec2 TexCoords;\n"
		"uniform vec4 uColor;\n"         
		"uniform bool uUseTexture;\n"
		"uniform bool uLighting;\n"
		"uniform sampler2D uTexture;\n"
		"void main()\n"
		"{\n"
		"   vec4 objectColor = uColor;\n"
		"   if (uUseTexture) {\n"
		"       objectColor *= texture(uTexture, TexCoords);\n" 
		"   }\n"
		"\n"
		"   // Alpha Test: Descarte de píxeles transparentes para corte de profundidad correcto\n"
		"   if (objectColor.a < 0.05)\n"
		"       discard;\n"
		"\n"
		"   vec3 result = objectColor.rgb;\n"
		"   if (uLighting) {\n"
		"       vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));\n"
		"       vec3 norm = normalize(Normal);\n"
		"       float ambientStrength = 0.25;\n"
		"       vec3 ambient = ambientStrength * vec3(1.0);\n"
		"       float diff = max(dot(norm, lightDir), 0.0);\n"
		"       vec3 diffuse = diff * vec3(1.0);\n"
		"       result = (ambient + diffuse) * objectColor.rgb;\n"
		"   }\n"
		"   FragColor = vec4(result, objectColor.a);\n"
		"}\0";

	shaderProgram = BuildProgram(vertexShaderSource, fragmentShaderSource, "Scene");

	/////////////////////  Line Shader (grid, ejes, normales y contorno de selección) /////////////////////////////
	const char* lineVS = "#version 460 core\n"
		"layout (location = 0) in vec3 aPos;\n"
		"uniform mat4 model;\n"
		"uniform mat4 view;\n"
		"uniform mat4 projection;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
		"}\0";

	const char* lineFS = "#version 460 core\n"
		"out vec4 FragColor;\n"
		"uniform vec3 uLineColor;\n"
		"void main()\n"
		"{\n"
		"   FragColor = vec4(uLineColor, 1.0);\n"
		"}\0";

	lineShaderProgram = BuildProgram(lineVS, lineFS, "Line");

	/////////////////////  Single Color Shader (Stencil Outline) /////////////////////////////
	const char* singleColorVS = 
		"#version 460 core\n"
		"layout (location = 0) in vec3 aPos;\n"
		"\n"
		"uniform mat4 model;\n"
		"uniform mat4 view;\n"
		"uniform mat4 projection;\n"
		"uniform float scaleFactor;\n" // Scale
		"\n"
		"void main()\n"
		"{\n"
		"   vec3 scaledPos = aPos * scaleFactor;\n"
		"   gl_Position = projection * view * model * vec4(scaledPos, 1.0);\n"
		"}\0";

	const char* singleColorFS = 
		"#version 460 core\n"
		"out vec4 FragColor;\n"
		"uniform vec3 uColor;\n"
		"void main()\n"
		"{\n"
		"   FragColor = vec4(uColor, 1.0);\n"
		"}\0";

	singleColorShaderProgram = BuildProgram(singleColorVS, singleColorFS, "SingleColor");

	///////////////////////////////////// Z Buffer //////////////////////////////////////////////////////////////
	const char* depthVS = R"(#version 460 core
	layout (location = 0) in vec2 aPos;
	layout (location = 1) in vec2 aTexCoords;
	out vec2 TexCoords;
	void main() {
		TexCoords = aTexCoords;
		gl_Position = vec4(aPos, 0.0, 1.0);
	})";

	const char* depthFS = R"(#version 460 core
	out vec4 FragColor;
	in vec2 TexCoords;

	uniform sampler2D depthMap;
	uniform float nearPlane;
	uniform float farPlane;

	float LinearizeDepth(float depth) {
		float z = depth * 2.0 - 1.0; // De [0, 1] a NDC [-1, 1]
		return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
	}

	void main() {
		float depthVal = texture(depthMap, TexCoords).r;
    
		// Convertir a profundidad lineal en unidades del mundo
		float linearDepth = LinearizeDepth(depthVal);
    
		// Normalizar respecto a una distancia visiva razonable (ej. hasta 50 unidades)
		// o escalar exponencialmente para resaltar el degradado de grises
		float c = linearDepth / (farPlane * 0.1); // Multiplicar por un factor para ajustar la sensibilidad
		c = clamp(c, 0.0, 1.0);

		FragColor = vec4(vec3(c), 1.0);
	})";

	depthDebugProgram = BuildProgram(depthVS, depthFS, "DepthLinearizer");

	// Cuadrilátero para renderizar en pantalla completa el shader
	float quadVertices[] = {
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f
	};

	glGenVertexArrays(1, &depthDebugVAO);
	glGenBuffers(1, &depthDebugVBO);
	glBindVertexArray(depthDebugVAO);
	glBindBuffer(GL_ARRAY_BUFFER, depthDebugVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glBindVertexArray(0);

	////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Ubicaciones de uniforms: se consultan una sola vez
	u.model = glGetUniformLocation(shaderProgram, "model");
	u.view = glGetUniformLocation(shaderProgram, "view");
	u.projection = glGetUniformLocation(shaderProgram, "projection");
	u.color = glGetUniformLocation(shaderProgram, "uColor");
	u.useTexture = glGetUniformLocation(shaderProgram, "uUseTexture");
	u.texture = glGetUniformLocation(shaderProgram, "uTexture");
	u.lighting = glGetUniformLocation(shaderProgram, "uLighting");

	lu.model = glGetUniformLocation(lineShaderProgram, "model");
	lu.view = glGetUniformLocation(lineShaderProgram, "view");
	lu.projection = glGetUniformLocation(lineShaderProgram, "projection");
	lu.color = glGetUniformLocation(lineShaderProgram, "uLineColor");

	scu.model = glGetUniformLocation(singleColorShaderProgram, "model");
	scu.view = glGetUniformLocation(singleColorShaderProgram, "view");
	scu.projection = glGetUniformLocation(singleColorShaderProgram, "projection");
	scu.color = glGetUniformLocation(singleColorShaderProgram, "uColor");
	scu.scaleFactor = glGetUniformLocation(singleColorShaderProgram, "scaleFactor");

	// Buffers persistentes para el dibujo de depuración
	glGenVertexArrays(1, &debugVAO);
	glGenBuffers(1, &debugVBO);
	glBindVertexArray(debugVAO);
	glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Inicializar geometría persistente de Grid y Axis
	CreateGridAndAxisBuffers();

	int width = Engine::GetInstance().window->GetWidth();
	int height = Engine::GetInstance().window->GetHeight();
	viewportWidth = width > 0 ? width : 800;
	viewportHeight = height > 0 ? height : 600;
	CreateFramebuffer(viewportWidth, viewportHeight);

	gameViewportWidth = viewportWidth;
	gameViewportHeight = viewportHeight;
	CreateGameFramebuffer(gameViewportWidth, gameViewportHeight);

	return true;
}

void Render::CreateGridAndAxisBuffers()
{
	// Cuadrícula en el plano XZ de -50 a +50 con paso 1.0
	std::vector<float> gridVerts;
	gridVerts.reserve((101 + 101) * 2 * 3);

	constexpr int GRID_RANGE = 50;
	for (int i = -GRID_RANGE; i <= GRID_RANGE; ++i)
	{
		const float coord = static_cast<float>(i);
		// Líneas paralelas a Z (avanzan en X)
		gridVerts.push_back(coord); gridVerts.push_back(0.0f); gridVerts.push_back(-static_cast<float>(GRID_RANGE));
		gridVerts.push_back(coord); gridVerts.push_back(0.0f); gridVerts.push_back(static_cast<float>(GRID_RANGE));

		// Líneas paralelas a X (avanzan en Z)
		gridVerts.push_back(-static_cast<float>(GRID_RANGE)); gridVerts.push_back(0.0f); gridVerts.push_back(coord);
		gridVerts.push_back(static_cast<float>(GRID_RANGE));  gridVerts.push_back(0.0f); gridVerts.push_back(coord);
	}

	gridVertexCount = static_cast<int>(gridVerts.size() / 3);

	glGenVertexArrays(1, &gridVAO);
	glGenBuffers(1, &gridVBO);
	glBindVertexArray(gridVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
	glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * sizeof(float), gridVerts.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Ejes divididos en Negativo y Positivo (Escalados a 5.0)
	std::vector<float> axisVerts = {
		// --- EJE X ---
		-5.0f, 0.0005f, 0.0f,   // 0: X Negativo (Inicio)
		 0.0f, 0.0005f, 0.0f,   // 1: X Centro
		 0.0f, 0.0005f, 0.0f,   // 2: X Centro
		 5.0f, 0.0005f, 0.0f,   // 3: X Positivo (Fin)

		 // --- EJE Y ---
		  0.0f,  0.0f,   0.0f,   // 4: Y Centro (Origen)
		  0.0f,  5.0f,   0.0f,   // 5: Y Positivo (Fin)

		  // --- EJE Z ---
		   0.0f, 0.0005f, -5.0f,  // 6: Z Negativo (Inicio)
		   0.0f, 0.0005f,  0.0f,  // 7: Z Centro
		   0.0f, 0.0005f,  0.0f,  // 8: Z Centro
		   0.0f, 0.0005f,  5.0f,  // 9: Z Positivo (Fin)

		   // --- MARCAS / FLECHAS EN LOS EXTREMOS POSITIVOS (Proporcionales) ---
		   // Flecha X+ (punta en 5.0, base en 4.5)
			5.0f, 0.0005f,  0.0f,    4.5f, 0.0005f,  0.25f,
			5.0f, 0.0005f,  0.0f,    4.5f, 0.0005f, -0.25f,

			// Flecha Y+ (punta en 5.0, base en 4.5)
			 0.0f,  5.0f,    0.0f,    0.25f,  4.5f,   0.0f,
			 0.0f,  5.0f,    0.0f,   -0.25f,  4.5f,   0.0f,

			 // Flecha Z+ (punta en 5.0, base en 4.5)
			  0.0f, 0.0005f,  5.0f,    0.25f, 0.0005f,  4.5f,
			  0.0f, 0.0005f,  5.0f,   -0.25f, 0.0005f,  4.5f
	};

	axisVertexCount = static_cast<int>(axisVerts.size() / 3);

	glGenVertexArrays(1, &axisVAO);
	glGenBuffers(1, &axisVBO);
	glBindVertexArray(axisVAO);
	glBindBuffer(GL_ARRAY_BUFFER, axisVBO);
	glBufferData(GL_ARRAY_BUFFER, axisVerts.size() * sizeof(float), axisVerts.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	LOG("[Render] Buffers estáticos creados para Grid (%d vértices) y Axis (%d vértices)",
		gridVertexCount, axisVertexCount);
}

void Render::DestroyGridAndAxisBuffers()
{
	if (gridVBO != 0) { glDeleteBuffers(1, &gridVBO); gridVBO = 0; }
	if (gridVAO != 0) { glDeleteVertexArrays(1, &gridVAO); gridVAO = 0; }
	gridVertexCount = 0;

	if (axisVBO != 0) { glDeleteBuffers(1, &axisVBO); axisVBO = 0; }
	if (axisVAO != 0) { glDeleteVertexArrays(1, &axisVAO); axisVAO = 0; }
	axisVertexCount = 0;
}

void Render::DrawGridAndAxis(const glm::mat4& view, const glm::mat4& projection)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!settings.showGrid && !settings.showAxis) return;

	glUseProgram(lineShaderProgram);
	glUniformMatrix4fv(lu.view, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(lu.projection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(lu.model, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

	glEnable(GL_DEPTH_TEST);

	// Dibujar Grid
	if (settings.showGrid && gridVAO != 0)
	{
		glLineWidth(1.0f);
		glUniform3f(lu.color, 0.32f, 0.32f, 0.35f);
		glBindVertexArray(gridVAO);
		glDrawArrays(GL_LINES, 0, gridVertexCount);
		glBindVertexArray(0);
	}

	if (settings.showAxis && axisVAO != 0)
	{
		glBindVertexArray(axisVAO);

		// --- EJE X ---
		glLineWidth(2.5f);
		// X Positivo (Rojo brillante)
		glUniform3f(lu.color, 0.95f, 0.22f, 0.22f);
		glDrawArrays(GL_LINES, 2, 2);

		// --- EJE Y (Ya es solo positivo) ---
		// Y Positivo (Verde brillante)
		glUniform3f(lu.color, 0.22f, 0.95f, 0.22f);
		glDrawArrays(GL_LINES, 4, 2);

		// --- EJE Z ---
		glLineWidth(2.5f);
		// Z Positivo (Azul brillante)
		glUniform3f(lu.color, 0.25f, 0.55f, 1.0f);
		glDrawArrays(GL_LINES, 8, 2);

		// --- FLECHAS DE LOS EXTREMOS POSITIVOS ---
		glLineWidth(2.0f);
		glUniform3f(lu.color, 1.0f, 1.0f, 1.0f);
		glDrawArrays(GL_LINES, 10, 12);

		glBindVertexArray(0);
		glLineWidth(1.0f);
	}
}

void Render::SetViewportSize(int width, int height)
{
	if (width > 0 && height > 0)
	{
		viewportWidth = width;
		viewportHeight = height;
	}
}

void Render::SetGameViewportSize(int width, int height)
{
	if (width > 0 && height > 0)
	{
		gameViewportWidth = width;
		gameViewportHeight = height;
	}
}

void Render::DestroyGameFramebuffer()
{
	if (gameFramebuffer != 0) { glDeleteFramebuffers(1, &gameFramebuffer); gameFramebuffer = 0; }
	if (gameTextureColorbuffer != 0) { glDeleteTextures(1, &gameTextureColorbuffer); gameTextureColorbuffer = 0; }
	if (gameDepthTextureBuffer != 0) { glDeleteTextures(1, &gameDepthTextureBuffer); gameDepthTextureBuffer = 0; }
	gameFboComplete = false;
}

void Render::CreateGameFramebuffer(int width, int height)
{
	if (width <= 0 || height <= 0) return;

	gameFboWidth = width;
	gameFboHeight = height;

	DestroyGameFramebuffer();

	glGenFramebuffers(1, &gameFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gameFramebuffer);

	glGenTextures(1, &gameTextureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, gameTextureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameTextureColorbuffer, 0);

	glGenTextures(1, &gameDepthTextureBuffer);
	glBindTexture(GL_TEXTURE_2D, gameDepthTextureBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, gameDepthTextureBuffer, 0);

	gameFboComplete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Render::DestroyFramebuffer()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (framebuffer != 0) { glDeleteFramebuffers(1, &framebuffer); framebuffer = 0; }
	if (textureColorbuffer != 0) { glDeleteTextures(1, &textureColorbuffer); textureColorbuffer = 0; }
	if (depthTextureBuffer != 0) { glDeleteTextures(1, &depthTextureBuffer); depthTextureBuffer = 0; }
	if (linearDepthTexture != 0) { glDeleteTextures(1, &linearDepthTexture); linearDepthTexture = 0; } // <- Añadido
	fboComplete = false;
}

void Render::CreateFramebuffer(int width, int height)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (width <= 0 || height <= 0) return;

	fboWidth = width;
	fboHeight = height;

	DestroyFramebuffer();

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	// 1. Textura de Color principal
	glGenTextures(1, &textureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

	// 2. Textura del Depth Buffer nativo de OpenGL
	glGenTextures(1, &depthTextureBuffer);
	glBindTexture(GL_TEXTURE_2D, depthTextureBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTextureBuffer, 0);

	// 3. Textura de Color procesada (Z-Buffer Lineal para ImGui)
	glGenTextures(1, &linearDepthTexture);
	glBindTexture(GL_TEXTURE_2D, linearDepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	fboComplete = (fbStatus == GL_FRAMEBUFFER_COMPLETE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Called each loop iteration
bool Render::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	if (viewportVisible && viewportWidth > 0 && viewportHeight > 0)
	{
		if (viewportWidth != fboWidth || viewportHeight != fboHeight)
		{
			CreateFramebuffer(viewportWidth, viewportHeight);
		}
	}
	if (gameViewportVisible && gameViewportWidth > 0 && gameViewportHeight > 0)
	{
		if (gameViewportWidth != gameFboWidth || gameViewportHeight != gameFboHeight)
		{
			CreateGameFramebuffer(gameViewportWidth, gameViewportHeight);
		}
	}
	return true;
}

bool Render::Update(float dt)
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	if (viewportVisible)
	{
		RenderScene();
	}
	if (gameViewportVisible)
	{
		RenderGame();
	}

	BindDefaultFramebuffer();
	return true;
}

void Render::BindDefaultFramebuffer()
{
	// Unbind FBO and prepare default framebuffer for UI rendering
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	int winW = Engine::GetInstance().window->GetWidth();
	int winH = Engine::GetInstance().window->GetHeight();
	glViewport(0, 0, winW > 0 ? winW : 800, winH > 0 ? winH : 600);

	// Reset states that might affect ImGui
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_LINE);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void Render::RenderScene()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (framebuffer == 0 || fboWidth <= 0 || fboHeight <= 0) return;

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glViewport(0, 0, fboWidth, fboHeight);

	// Configuración de Stencil y Clear
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	glClearColor(
		background.r / 255.0f,
		background.g / 255.0f,
		background.b / 255.0f,
		background.a / 255.0f
	);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	if (settings.depthTest) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	if (settings.cullFace) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);

	if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUseProgram(shaderProgram);

	Scene* scene = Engine::GetInstance().scene.get();
	GameObject* cameraObj = scene ? scene->GetMainCameraObject() : nullptr;
	CameraComponent* camera = cameraObj ? cameraObj->GetComponent<CameraComponent>() : nullptr;

	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);
	glm::vec3 cameraPos = glm::vec3(0.0f);

	if (camera) {
		float aspectRatio = (fboHeight > 0) ? (float)fboWidth / (float)fboHeight : 1.0f;
		view = camera->GetViewMatrix();
		projection = camera->GetProjectionMatrix(aspectRatio);
		if (auto camTrans = cameraObj->GetComponent<TransformComponent>()) {
			cameraPos = camTrans->GetPosition();
		}
	}

	glUniformMatrix4fv(u.view, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1i(u.lighting, settings.lighting ? 1 : 0);
	glUniform1i(u.texture, 0);

	// -------------------------------------------------------------------------
	// Recolección y Clasificación (Opacos vs Transparentes)
	// -------------------------------------------------------------------------
	struct RenderItem {
		GameObject* go;
		glm::mat4 modelMatrix;
		float distanceToCamera;
	};

	std::vector<RenderItem> opaqueItems;
	std::vector<RenderItem> transparentItems;

	std::function<void(GameObject*, const glm::mat4&)> collectItems = [&](GameObject* go, const glm::mat4& parentMatrix) 
	{
		if (!go || !go->IsActive()) return;

		glm::mat4 globalMatrix = parentMatrix;
		if (auto t = go->GetComponent<TransformComponent>()) 
		{
			globalMatrix = parentMatrix * t->GetLocalModelMatrix();
		}

		if (go->GetComponent<MeshComponent>()) 
		{
			MaterialComponent* mat = go->GetComponent<MaterialComponent>();

			// Usamos el nuevo método que revisa tanto el color como la textura cargada
			bool isTransparent = mat ? mat->HasTransparentAlpha() : false;

			glm::vec3 objPos = glm::vec3(globalMatrix[3]);
			float dist = glm::length(cameraPos - objPos);

			if (isTransparent) {
				transparentItems.push_back({ go, globalMatrix, dist });
			}
			else {
				opaqueItems.push_back({ go, globalMatrix, dist });
			}
		}

		for (GameObject* child : go->GetChildren()) 
		{
			collectItems(child, globalMatrix);
		}
	};

	if (scene->GetRoot()) {
		collectItems(scene->GetRoot(), glm::mat4(1.0f));
	}

	// Ordenar transparentes de más lejano a más cercano (Back-to-Front)
	std::sort(transparentItems.begin(), transparentItems.end(), [](const RenderItem& a, const RenderItem& b) {
		return a.distanceToCamera > b.distanceToCamera;
		});

	// -------------------------------------------------------------------------
	// Objetos Opacos (Escriben en Depth Buffer)
	// -------------------------------------------------------------------------
	glDepthMask(GL_TRUE);
	for (const auto& item : opaqueItems) 
	{
		DrawSingleGameObject(item.go, item.modelMatrix);
	}

	// -------------------------------------------------------------------------
	// Objetos Transparentes
	// -------------------------------------------------------------------------
	// Mantenemos glDepthMask(GL_TRUE) si queremos que escriban su profundidad
	// para que queden registrados en el Z-Buffer Visualizer y el Stencil del Outline
	
	// Blending para transparencias
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask(GL_FALSE);
	for (const auto& item : transparentItems) {
		DrawSingleGameObject(item.go, item.modelMatrix);
	}
	glDepthMask(GL_TRUE); // Restaurar la máscara por defecto

	// Blending para transparencias
	glDisable(GL_BLEND);

	// -------------------------------------------------------------------------
	// PASE DE LINEARIZACIÓN DEL Z-BUFFER (Para ImGui)
	// -------------------------------------------------------------------------
	if (settings.showDepthBuffer && linearDepthTexture != 0 && depthTextureBuffer != 0)
	{
		// 1. Guardar estados actuales para no contaminar el pipeline
		GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);

		// 2. Apuntar temporalmente el FBO a la textura del Z-Buffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, linearDepthTexture, 0);

		glDisable(GL_DEPTH_TEST);
		glUseProgram(depthDebugProgram);

		float nearP = camera ? camera->nearPlane : 0.1f;
		float farP = camera ? camera->farPlane : 100.0f;

		glUniform1f(glGetUniformLocation(depthDebugProgram, "nearPlane"), nearP);
		glUniform1f(glGetUniformLocation(depthDebugProgram, "farPlane"), farP);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthTextureBuffer);
		glUniform1i(glGetUniformLocation(depthDebugProgram, "depthMap"), 0);

		glBindVertexArray(depthDebugVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		// 3. RESTAURACIÓN STRICTA: Volver a conectar la textura de color principal a la escena
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

		// 4. Restaurar el estado previo de Depth Test
		if (depthTestWasEnabled) {
			glEnable(GL_DEPTH_TEST);
		}
	}
	else if (linearDepthTexture != 0)
	{
		// Aseguramos que el FBO siempre vuelva a la textura de color original aunque showDepthBuffer se apague
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
	}

	// -------------------------------------------------------------------------
	// Grid, Axis y Debug
	// -------------------------------------------------------------------------

	DrawGridAndAxis(view, projection);
	DrawDebug(view, projection);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


bool Render::PostUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	SDL_Window* window = Engine::GetInstance().window->GetWindow();
	SDL_GL_SwapWindow(window);

	return true;
}

// Called before quitting
bool Render::CleanUp() 
{
	DestroyFramebuffer();
	DestroyGameFramebuffer();
	DestroyGridAndAxisBuffers();

	if (debugVBO != 0) { glDeleteBuffers(1, &debugVBO); debugVBO = 0; }
	if (debugVAO != 0) { glDeleteVertexArrays(1, &debugVAO); debugVAO = 0; }
	debugVBOCapacityBytes = 0;
	debugVertexLines.clear();
	debugVertexLines.shrink_to_fit();
	debugFaceLines.clear();
	debugFaceLines.shrink_to_fit();

	if (shaderProgram != 0) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
	if (lineShaderProgram != 0) { glDeleteProgram(lineShaderProgram); lineShaderProgram = 0; }
	if (singleColorShaderProgram != 0) { glDeleteProgram(singleColorShaderProgram); singleColorShaderProgram = 0; }

	LOG("[Render] Recursos OpenGL liberados (FBO, RBO, textura de color, shaders, buffers de depuración, Grid y Axis)");
	return true;
}

void Render::SetBackgroundColor(SDL_Color color)
{
	background = color;
}

void Render::SetSettings(const RenderSettings& newSettings)
{
	const bool vsyncChanged = (newSettings.vsync != settings.vsync);
	settings = newSettings;
	if (vsyncChanged)
	{
		SetVSync(settings.vsync);
	}
}

void Render::SetVSync(bool enabled)
{
	settings.vsync = enabled;
	if (!SDL_GL_SetSwapInterval(enabled ? 1 : 0))
	{
		LOG_WARNING("[OpenGL] No se pudo cambiar el V-Sync: %s", SDL_GetError());
		return;
	}
	LOG("[OpenGL] V-Sync %s", enabled ? "Habilitado (1)" : "Deshabilitado (0)");
}

void Render::DrawSingleGameObject(GameObject* go, const glm::mat4& worldMatrix)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!go || !go->IsActive()) return;

	MeshComponent* mesh = go->GetComponent<MeshComponent>();
	MaterialComponent* material = go->GetComponent<MaterialComponent>();

	if (mesh && mesh->GetPrimitive()) 
	{
		glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(worldMatrix));

		glActiveTexture(GL_TEXTURE0);
		if (material) 
		{
			unsigned int texId = material->GetActiveTextureID();
			glUniform4fv(u.color, 1, glm::value_ptr(material->GetColor()));
			if (texId != 0) 
			{
				glBindTexture(GL_TEXTURE_2D, texId);
				glUniform1i(u.useTexture, 1);
			}
			else 
			{
				glUniform1i(u.useTexture, 0);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}
		else 
		{
			glUniform4f(u.color, 0.8f, 0.8f, 0.8f, 1.0f);
			glUniform1i(u.useTexture, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		Scene* scene = Engine::GetInstance().scene.get();
		GameObject* selectedObj = scene ? scene->GetSelected() : nullptr;
		bool isSelected = (selectedObj != nullptr && settings.highlightSelection &&
			(selectedObj == go || go->IsDescendantOf(selectedObj)));

		// Configurar Stencil para la selección (Aplica tanto a opacos como a transparentes)
		if (isSelected)
		{
			glEnable(GL_STENCIL_TEST);
			glStencilFunc(GL_ALWAYS, 1, 0xFF); // Siempre pasa y escribe 1
			glStencilMask(0xFF);                // Habilita escritura en el stencil buffer
			glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
		}

		mesh->GetPrimitive()->Draw();

		if (isSelected)
		{
			glStencilMask(0x00);
			glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		}
	}
}
// -----------------------------------------------------------------------------
// Dibujo de depuración
// -----------------------------------------------------------------------------
void Render::DrawDebug(const glm::mat4& view, const glm::mat4& projection)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	Scene* scene = Engine::GetInstance().scene.get();
	GameObject* selected = scene ? scene->GetSelected() : nullptr;

	const bool wantNormals = settings.normals != NormalsMode::None;
	const bool wantOutline = settings.highlightSelection && selected != nullptr;
	if (!wantNormals && !wantOutline) return;

	if (wantOutline && selected)
	{
		DrawSelectionOutline(selected, view, projection);
	}

	if (wantNormals)
	{
		glUseProgram(lineShaderProgram);
		glUniformMatrix4fv(lu.view, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(lu.projection, 1, GL_FALSE, glm::value_ptr(projection));

		// Limpiamos los elementos pero reservamos la memoria previamente para evitar reallocations
		debugVertexLines.clear();
		debugFaceLines.clear();

		// Reserva estimada para evitar petar la RAM con push_back() en cada frame
		debugVertexLines.reserve(10000);
		debugFaceLines.reserve(10000);

		if (settings.normalsAllObjects)
		{
			CollectNormals(scene->GetRoot(), glm::mat4(1.0f));
		}
		else if (selected)
		{
			glm::mat4 parentWorld = selected->GetParent() ? selected->GetParent()->GetWorldMatrix() : glm::mat4(1.0f);
			CollectNormals(selected, parentWorld);
		}

		const size_t vertexBytes = debugVertexLines.size() * sizeof(float);
		const size_t faceBytes = debugFaceLines.size() * sizeof(float);
		const size_t totalBytes = vertexBytes + faceBytes;

		if (totalBytes > 0)
		{
			glBindVertexArray(debugVAO);
			glBindBuffer(GL_ARRAY_BUFFER, debugVBO);

			// Reasignar buffer dinámico de OpenGL solo si la capacidad actual no es suficiente
			if (totalBytes > debugVBOCapacityBytes)
			{
				debugVBOCapacityBytes = totalBytes * 2;
				glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)debugVBOCapacityBytes, nullptr, GL_DYNAMIC_DRAW);
			}

			if (vertexBytes > 0)
				glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vertexBytes, debugVertexLines.data());
			if (faceBytes > 0)
				glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)vertexBytes, (GLsizeiptr)faceBytes, debugFaceLines.data());

			glUniformMatrix4fv(lu.model, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
			glLineWidth(1.5f);

			const GLsizei vertexCount = (GLsizei)(debugVertexLines.size() / 3);
			const GLsizei faceCount = (GLsizei)(debugFaceLines.size() / 3);

			if (vertexCount > 0)
			{
				glUniform3f(lu.color, 1.0f, 1.0f, 0.0f); // Amarillo: vértices
				glDrawArrays(GL_LINES, 0, vertexCount);
			}
			if (faceCount > 0)
			{
				glUniform3f(lu.color, 0.0f, 1.0f, 1.0f); // Cian: caras
				glDrawArrays(GL_LINES, vertexCount, faceCount);
			}

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}
}

void Render::CollectNormals(GameObject* go, const glm::mat4& parentMatrix)
{
	if (!go || !go->IsActive()) return;

	glm::mat4 globalMatrix = parentMatrix;
	TransformComponent* transform = go->GetComponent<TransformComponent>();
	if (transform) {
		globalMatrix = parentMatrix * transform->GetLocalModelMatrix();
	}

	AppendNormals(go, globalMatrix);

	for (GameObject* child : go->GetChildren()) {
		CollectNormals(child, globalMatrix);
	}
}

void Render::AppendNormals(GameObject* go, const glm::mat4& worldMatrix)
{
	MeshComponent* mesh = go->GetComponent<MeshComponent>();
	if (!mesh || !mesh->GetPrimitive()) return;

	Primitive* prim = mesh->GetPrimitive();
	const bool wantVertex = (settings.normals == NormalsMode::Vertex || settings.normals == NormalsMode::Both);
	const bool wantFace = (settings.normals == NormalsMode::Face || settings.normals == NormalsMode::Both);

	// Longitud proporcional al tamaño del objeto en mundo
	const float maxScale = std::max({ glm::length(glm::vec3(worldMatrix[0])), glm::length(glm::vec3(worldMatrix[1])), glm::length(glm::vec3(worldMatrix[2])) });
	const float diagonal = glm::length(prim->GetBoundsMax() - prim->GetBoundsMin());
	const float normalLength = std::max(0.05f, 0.1f * diagonal * maxScale);

	// Matriz para transformar la normal correctamente (sin escalas deformadas)
	const glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(worldMatrix)));

	auto pushLine = [](std::vector<float>& buffer, const glm::vec3& a, const glm::vec3& b) {
		buffer.push_back(a.x); buffer.push_back(a.y); buffer.push_back(a.z);
		buffer.push_back(b.x); buffer.push_back(b.y); buffer.push_back(b.z);
	};

	if (wantVertex)
	{
		for (const Vertex& v : prim->GetVertices())
		{
			glm::vec3 worldPos = glm::vec3(worldMatrix * glm::vec4(v.position, 1.0f));
			glm::vec3 worldNormal = glm::normalize(normalMatrix * v.normal);
			pushLine(debugVertexLines, worldPos, worldPos + worldNormal * normalLength);
		}
	}

	if (wantFace)
	{
		for (const FaceData& f : prim->GetFaces())
		{
			glm::vec3 worldPos = glm::vec3(worldMatrix * glm::vec4(f.centroid, 1.0f));
			glm::vec3 worldNormal = glm::normalize(normalMatrix * f.normal);
			pushLine(debugFaceLines, worldPos, worldPos + worldNormal * normalLength);
		}
	}
}

void Render::DrawSelectionOutline(GameObject* selected, const glm::mat4& view, const glm::mat4& projection)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (!selected || !selected->IsActive()) return;

	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilMask(0x00);

	glDisable(GL_DEPTH_TEST);

	glUseProgram(singleColorShaderProgram);
	glUniformMatrix4fv(scu.view, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(scu.projection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1f(scu.scaleFactor, 1.03f);
	glUniform3fv(scu.color, 1, glm::value_ptr(settings.outlineColor));

	std::function<void(GameObject*)> drawOutlineRecursive = [&](GameObject* node)
		{
			if (!node || !node->IsActive()) return;

			MeshComponent* mesh = node->GetComponent<MeshComponent>();
			if (mesh && mesh->GetPrimitive())
			{
				glUniformMatrix4fv(scu.model, 1, GL_FALSE, glm::value_ptr(node->GetWorldMatrix()));
				mesh->GetPrimitive()->Draw();
			}

			for (GameObject* child : node->GetChildren())
			{
				drawOutlineRecursive(child);
			}
		};

	drawOutlineRecursive(selected);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glStencilMask(0xFF);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
	glDisable(GL_STENCIL_TEST);
}

void Render::RenderGame()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	if (gameFramebuffer == 0 || gameFboWidth <= 0 || gameFboHeight <= 0) return;

	glBindFramebuffer(GL_FRAMEBUFFER, gameFramebuffer);
	glViewport(0, 0, gameFboWidth, gameFboHeight);

	glClearColor(
		background.r / 255.0f,
		background.g / 255.0f,
		background.b / 255.0f,
		background.a / 255.0f
	);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Scene* scene = Engine::GetInstance().scene.get();
	GameObject* gameCameraObj = scene ? scene->GetGameCameraObject() : nullptr;
	CameraComponent* camera = (gameCameraObj && gameCameraObj->IsActive()) ? gameCameraObj->GetComponent<CameraComponent>() : nullptr;

	// Si no hay Main Camera activa en la escena, dejamos el fondo limpio y salimos
	if (!camera)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}

	if (settings.depthTest) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	if (settings.cullFace) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // La vista Game siempre se ve sólida

	glUseProgram(shaderProgram);

	float aspectRatio = (gameFboHeight > 0) ? (float)gameFboWidth / (float)gameFboHeight : 1.0f;
	glm::mat4 view = camera->GetViewMatrix();
	glm::mat4 projection = camera->GetProjectionMatrix(aspectRatio);
	glm::vec3 cameraPos = glm::vec3(gameCameraObj->GetWorldMatrix()[3]);

	glUniformMatrix4fv(u.view, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1i(u.lighting, settings.lighting ? 1 : 0);
	glUniform1i(u.texture, 0);

	struct RenderItem {
		GameObject* go;
		glm::mat4 modelMatrix;
		float distanceToCamera;
	};

	std::vector<RenderItem> opaqueItems;
	std::vector<RenderItem> transparentItems;

	std::function<void(GameObject*, const glm::mat4&)> collectItems = [&](GameObject* go, const glm::mat4& parentMatrix)
		{
			if (!go || !go->IsActive()) return;

			glm::mat4 globalMatrix = parentMatrix;
			if (auto t = go->GetComponent<TransformComponent>())
			{
				globalMatrix = parentMatrix * t->GetLocalModelMatrix();
			}

			if (go->GetComponent<MeshComponent>())
			{
				MaterialComponent* mat = go->GetComponent<MaterialComponent>();
				bool isTransparent = mat ? mat->HasTransparentAlpha() : false;
				glm::vec3 objPos = glm::vec3(globalMatrix[3]);
				float dist = glm::length(cameraPos - objPos);

				if (isTransparent) transparentItems.push_back({ go, globalMatrix, dist });
				else opaqueItems.push_back({ go, globalMatrix, dist });
			}

			for (GameObject* child : go->GetChildren())
			{
				collectItems(child, globalMatrix);
			}
		};

	if (scene->GetRoot())
	{
		collectItems(scene->GetRoot(), glm::mat4(1.0f));
	}

	std::sort(transparentItems.begin(), transparentItems.end(), [](const RenderItem& a, const RenderItem& b) {
		return a.distanceToCamera > b.distanceToCamera;
		});

	// Desactivamos temporalmente el highlight de selección para no ensuciar el stencil en la vista Game
	bool prevHighlight = settings.highlightSelection;
	settings.highlightSelection = false;

	glDepthMask(GL_TRUE);
	for (const auto& item : opaqueItems)
	{
		DrawSingleGameObject(item.go, item.modelMatrix);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	for (const auto& item : transparentItems)
	{
		DrawSingleGameObject(item.go, item.modelMatrix);
	}
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	settings.highlightSelection = prevHighlight;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}