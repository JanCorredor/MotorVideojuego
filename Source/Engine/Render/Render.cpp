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
		"uniform vec3 uColor;\n"
		"uniform bool uUseTexture;\n"
		"uniform bool uLighting;\n"
		"uniform sampler2D uTexture;\n"
		"void main()\n"
		"{\n"
		"   vec3 objectColor = uColor;\n"
		"   if (uUseTexture) {\n"
		"       objectColor *= texture(uTexture, TexCoords).rgb;\n"
		"   }\n"
		"   vec3 result = objectColor;\n"
		"   if (uLighting) {\n"
		"       vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));\n"
		"       vec3 norm = normalize(Normal);\n"
		"       float ambientStrength = 0.25;\n"
		"       vec3 ambient = ambientStrength * vec3(1.0);\n"
		"       float diff = max(dot(norm, lightDir), 0.0);\n"
		"       vec3 diffuse = diff * vec3(1.0);\n"
		"       result = (ambient + diffuse) * objectColor;\n"
		"   }\n"
		"   FragColor = vec4(result, 1.0);\n"
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
	const char* singleColorVS = "#version 460 core\n"
		"layout (location = 0) in vec3 aPos;\n"
		"layout (location = 1) in vec3 aNormal;\n"
		"uniform mat4 model;\n"
		"uniform mat4 view;\n"
		"uniform mat4 projection;\n"
		"void main()\n"
		"{\n"
		"   // Desplazamiento ligero por normal para evitar deformaciones en mallas complejas\n"
		"   vec3 scaledPos = aPos + aNormal * 0.03;\n"
		"   gl_Position = projection * view * model * vec4(scaledPos, 1.0);\n"
		"}\0";

	const char* singleColorFS = "#version 460 core\n"
		"out vec4 FragColor;\n"
		"uniform vec3 uColor;\n"
		"void main()\n"
		"{\n"
		"   FragColor = vec4(uColor, 1.0);\n"
		"}\0";

	singleColorShaderProgram = BuildProgram(singleColorVS, singleColorFS, "SingleColor");

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

	// Ejes centrados en (0,0,0)
	std::vector<float> axisVerts = {
		// Eje X (Rojo): de -50 a +50 a lo largo del centro
		-50.0f, 0.0005f, 0.0f,
		 50.0f, 0.0005f, 0.0f,

		// Eje Y (Verde): vertical de 0 a 25
		 0.0f,  0.0f,    0.0f,
		 0.0f, 25.0f,    0.0f,

		// Eje Z (Azul): de -50 a +50 a lo largo del centro
		 0.0f, 0.0005f, -50.0f,
		 0.0f, 0.0005f,  50.0f
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
		// Color gris tenue
		glUniform3f(lu.color, 0.32f, 0.32f, 0.35f);
		glBindVertexArray(gridVAO);
		glDrawArrays(GL_LINES, 0, gridVertexCount);
		glBindVertexArray(0);
	}

	// Dibujar Axis con mayor grosor para destacar sobre la cuadrícula
	if (settings.showAxis && axisVAO != 0)
	{
		glLineWidth(2.5f);
		glBindVertexArray(axisVAO);

		// Eje X: Rojo
		glUniform3f(lu.color, 0.95f, 0.22f, 0.22f);
		glDrawArrays(GL_LINES, 0, 2);

		// Eje Y: Verde
		glUniform3f(lu.color, 0.22f, 0.95f, 0.22f);
		glDrawArrays(GL_LINES, 2, 2);

		// Eje Z: Azul
		glUniform3f(lu.color, 0.25f, 0.55f, 1.0f);
		glDrawArrays(GL_LINES, 4, 2);

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

void Render::DestroyFramebuffer()
{
	if (framebuffer != 0) { glDeleteFramebuffers(1, &framebuffer); framebuffer = 0; }
	if (textureColorbuffer != 0) { glDeleteTextures(1, &textureColorbuffer); textureColorbuffer = 0; }
	if (rbo != 0) { glDeleteRenderbuffers(1, &rbo); rbo = 0; }
	fboComplete = false;
}

void Render::CreateFramebuffer(int width, int height)
{
	if (width <= 0 || height <= 0) return;

	fboWidth = width;
	fboHeight = height;

	DestroyFramebuffer();

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glGenTextures(1, &textureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	fboComplete = (fbStatus == GL_FRAMEBUFFER_COMPLETE);
	if (!fboComplete)
	{
		LOG_ERROR("[OpenGL] Framebuffer incompleto! (Código: 0x%X)", fbStatus);
	}
	else
	{
		static bool firstFboLogged = false;
		if (!firstFboLogged)
		{
			LOG("[OpenGL] Framebuffer inicializado: %dx%d (FBO ID: %u, Textura ID: %u, RBO ID: %u)",
				width, height, framebuffer, textureColorbuffer, rbo);
			firstFboLogged = true;
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Called each loop iteration
bool Render::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	// Con el viewport oculto no se mantiene el FBO
	if (viewportVisible && viewportWidth > 0 && viewportHeight > 0)
	{
		if (viewportWidth != fboWidth || viewportHeight != fboHeight)
		{
			CreateFramebuffer(viewportWidth, viewportHeight);
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
	glClear(GL_COLOR_BUFFER_BIT);
}

void Render::RenderScene()
{
	if (framebuffer == 0 || fboWidth <= 0 || fboHeight <= 0) return;

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glViewport(0, 0, fboWidth, fboHeight);

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);


	// Habilitar Stencil Test y limpiar buffers
	glEnable(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);


	glClearColor(
		background.r / 255.0f,
		background.g / 255.0f,
		background.b / 255.0f,
		background.a / 255.0f
	);

	// Limpiar Color, Depth y Stencil Buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	if (settings.depthTest) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	if (settings.cullFace) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);

	if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUseProgram(shaderProgram);

	Scene* scene = Engine::GetInstance().scene.get();
	GameObject* cameraObj = scene->GetMainCameraObject();
	CameraComponent* camera = cameraObj ? cameraObj->GetComponent<CameraComponent>() : nullptr;

	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);

	if (camera) {
		float aspectRatio = (fboHeight > 0) ? (float)fboWidth / (float)fboHeight : 1.0f;

		view = camera->GetViewMatrix();
		projection = camera->GetProjectionMatrix(aspectRatio);
	}

	// Send to GPU
	glUniformMatrix4fv(u.view, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniform1i(u.lighting, settings.lighting ? 1 : 0);
	glUniform1i(u.texture, 0);

	// Configuración por defecto del Stencil Buffer para objetos no seleccionados (no escriben valor 1)
	glStencilFunc(GL_ALWAYS, 1, 0xFF);
	glStencilMask(0x00);

	// -------------------------------------------------------------------------
	// Draw Scene GameObjects
	// -------------------------------------------------------------------------
	GameObject* root = scene->GetRoot();
	if (root)
	{
		DrawGameObject(root, glm::mat4(1.0f));
	}

	// Restaurar máscara de stencil completa tras la escena
	glStencilMask(0xFF);

	// -------------------------------------------------------------------------
	// Draw Grid and Axis inside FBO
	// -------------------------------------------------------------------------
	DrawGridAndAxis(view, projection);

	// Normales y contorno de selección (no se ven afectados por el modo wireframe global)
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

void Render::DrawGameObject(GameObject* go, const glm::mat4& parentMatrix)
{
	if (!go->IsActive()) return;

	glm::mat4 globalMatrix = parentMatrix;
	TransformComponent* transform = go->GetComponent<TransformComponent>();
	if (transform) {
		globalMatrix = parentMatrix * transform->GetLocalModelMatrix();
	}

	MeshComponent* mesh = go->GetComponent<MeshComponent>();
	MaterialComponent* material = go->GetComponent<MaterialComponent>();

	if (mesh && mesh->GetPrimitive()) {
		glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(globalMatrix));

		if (material) {
			unsigned int texId = material->GetActiveTextureID();
			glUniform3fv(u.color, 1, glm::value_ptr(material->GetColor()));
			if (texId != 0) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, texId);
				glUniform1i(u.useTexture, 1);
			} else {
				glUniform1i(u.useTexture, 0);
			}
		} else {
			// Default visual if no material
			glUniform3f(u.color, 0.8f, 0.8f, 0.8f);
			glUniform1i(u.useTexture, 0);
		}


		// Si el objeto actual es el seleccionado, habilitamos la escritura '1' en el Stencil Buffer
		Scene* scene = Engine::GetInstance().scene.get();
		bool isSelected = (scene && scene->GetSelected() == go && settings.highlightSelection);

		if (isSelected) 
		{
			glStencilFunc(GL_ALWAYS, 1, 0xFF);
			glStencilMask(0xFF); // Habilita escritura en Stencil Buffer
		}
		else 
		{
			glStencilMask(0x00); // Deshabilita escritura
		}

		mesh->GetPrimitive()->Draw();
	}

	for (GameObject* child : go->GetChildren()) {
		DrawGameObject(child, globalMatrix);
	}
}

// -----------------------------------------------------------------------------
// Dibujo de depuración
// -----------------------------------------------------------------------------
void Render::DrawDebug(const glm::mat4& view, const glm::mat4& projection)
{
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
			AppendNormals(selected, selected->GetWorldMatrix());
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
	MeshComponent* mesh = selected->GetComponent<MeshComponent>();
	if (!mesh || !mesh->GetPrimitive()) return;

	// Configuración de Stencil Buffer para la silueta:
	// Solo dibujar en píxeles donde el valor Stencil sea DIFERENTE de 1
	glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
	glStencilMask(0x00); // Desactivar escritura en Stencil para no sobrescribir la máscara

	glDisable(GL_DEPTH_TEST); // Evitar que el depth buffer oculte el contorno

	glUseProgram(singleColorShaderProgram);
	glUniformMatrix4fv(scu.view, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(scu.projection, 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(scu.model, 1, GL_FALSE, glm::value_ptr(selected->GetWorldMatrix()));
	
	glUniform3fv(scu.color, 1, glm::value_ptr(settings.outlineColor));
	
	// Renderizar la malla con el Vertex Shader de expansión por normal
	mesh->GetPrimitive()->Draw();

	// Restablecer estado normal de OpenGL
	glEnable(GL_DEPTH_TEST);
	glStencilMask(0xFF);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);
}
