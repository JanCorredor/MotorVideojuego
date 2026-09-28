#include "Render.h"

#include "../Engine.h"
#include "../Window/Window.h"
#include "../Input/Input.h"
#include "../../Utils/Log.h"

#include <glad/glad.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

	// Enable Depth Test
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Face Culling
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW); // Counter-Clockwise

	LOG( "Renderer: %s\n", glGetString(GL_RENDERER));

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
		"uniform sampler2D uTexture;\n"
		"void main()\n"
		"{\n"
		"   vec3 objectColor = uColor;\n"
		"   if (uUseTexture) {\n"
		"       objectColor *= texture(uTexture, TexCoords).rgb;\n"
		"   }\n"
		"   vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));\n"
		"   vec3 norm = normalize(Normal);\n"
		"   float ambientStrength = 0.25;\n"
		"   vec3 ambient = ambientStrength * vec3(1.0);\n"
		"   float diff = max(dot(norm, lightDir), 0.0);\n"
		"   vec3 diffuse = diff * vec3(1.0);\n"
		"   vec3 result = (ambient + diffuse) * objectColor;\n"
		"   FragColor = vec4(result, 1.0);\n"
		"}\0";

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	//Create primitives
	cube = GeometryGenerator::CreateCube(1.0f);
	sphere = GeometryGenerator::CreateSphere(0.5f, 20, 20);
	sphereTextureID = LoadTexture("../test.jpg");
	
	pyramid = GeometryGenerator::CreatePyramid(1.0f, 1.5f);

	return true;
}

// Called each loop iteration
bool Render::PreUpdate()
{
	glClearColor(
		background.r / 255.0f,
		background.g / 255.0f,
		background.b / 255.0f,
		background.a / 255.0f
	);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	return true;
}

bool Render::Update(float dt)
{
	rotationAngle += 0.002f * dt;

	glUseProgram(shaderProgram);

	//glm::mat4 model = glm::mat4(1.0f);
	//model = glm::rotate(model, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

	//glm::mat4 view = glm::mat4(1.0f);
	//view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

	//int width, height;
	//Engine::GetInstance().window->GetWindowSize(width, height);
	//float aspectRatio = (height > 0) ? (float)width / (float)height : 1.0f;
	//glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);

	//glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
	//glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
	//glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	//glBindVertexArray(VAO);
	//glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	// 3. Configurar la matriz de Vista (Cámara)
	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -6.0f));

	// 4. Configurar la matriz de Proyección (Perspectiva)
	int width, height;
	Engine::GetInstance().window->GetWindowSize(width, height);
	float aspectRatio = (height > 0) ? (float)width / (float)height : 1.0f;
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);

	// Enviar matrices de Vista y Proyección a la GPU
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// Obtener ubicaciones de las variables Uniform del Shader
	int modelLoc = glGetUniformLocation(shaderProgram, "model");
	int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
	int useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
	int texLoc = glGetUniformLocation(shaderProgram, "uTexture");

	// -------------------------------------------------------------------------
	// DIBUJAR CUBO (Izquierda - Con Color Rojo y Sin Textura)
	// -------------------------------------------------------------------------
	if (cube)
	{
		glm::mat4 modelCube = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
		modelCube = glm::rotate(modelCube, rotationAngle, glm::vec3(0.5f, 1.0f, 0.0f));

		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelCube));
		glUniform3f(colorLoc, 1.0f, 0.2f, 0.2f); // Rojo
		glUniform1i(useTexLoc, false);           // Desactivar textura

		cube->Draw();
	}

	// -------------------------------------------------------------------------
	// DIBUJAR ESFERA (Derecha - Con Textura o Color Azul)
	// -------------------------------------------------------------------------
	if (sphere)
	{
		glm::mat4 modelSphere = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
		modelSphere = glm::rotate(modelSphere, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelSphere));

		if (sphereTextureID != 0)
		{
			// Si hay textura cargada la vinculamos en la unidad GL_TEXTURE0
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sphereTextureID);
			glUniform1i(texLoc, 0);
			glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f); // Blanco (mantiene colores de la textura)
			glUniform1i(useTexLoc, true);            // Activar uso de textura
		}
		else
		{
			// Si no hay textura, pintamos de Azul
			glUniform3f(colorLoc, 0.2f, 0.5f, 1.0f);
			glUniform1i(useTexLoc, false);
		}

		sphere->Draw();
	}

	// -------------------------------------------------------------------------
	// DIBUJAR PIRÁMIDE (Centro - Color Verde)
	// -------------------------------------------------------------------------
	//if (pyramid)
	//{
	//	glm::mat4 modelPyramid = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	//	modelPyramid = glm::rotate(modelPyramid, rotationAngle * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f));

	//	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelPyramid));
	//	glUniform3f(colorLoc, 0.2f, 0.9f, 0.3f); // Verde
	//	glUniform1i(useTexLoc, false);

	//	pyramid->Draw();
	//}


	// Comprobar si se ha arrastrado un archivo a la ventana
    if (Engine::GetInstance().input->HasDroppedFile()) {
        std::string filePath = Engine::GetInstance().input->GetDroppedFilePath();

        // Verificar si es una extensión compatible (FBX, OBJ, etc.)
        if (filePath.find(".fbx") != std::string::npos || 
            filePath.find(".FBX") != std::string::npos ||
            filePath.find(".obj") != std::string::npos) 
        {
            // Descargar el modelo anterior (si existía) reseteando el pointer
            loadedModel.reset(); 

            // Cargar el nuevo modelo arrastrado
            loadedModel = ModelLoader::LoadFBX(filePath);
        }

        // Limpiar el evento de drag and drop
        Engine::GetInstance().input->ClearDroppedFile();
    }

    // Dibujar el modelo FBX cargado si existe
    if (loadedModel) {
        glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)); // Ajustar escala según el FBX
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        
        // Asignar color/textura por defecto y dibujar
        glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), 0.8f, 0.8f, 0.8f);
        glUniform1i(glGetUniformLocation(shaderProgram, "uUseTexture"), false);
        
        loadedModel->Draw();
    }

	return true;
}

bool Render::PostUpdate()
{
	SDL_Window* window = Engine::GetInstance().window->GetWindow();

	SDL_GL_SwapWindow(window);

	return true;
}

// Called before quitting
bool Render::CleanUp() 
{

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteProgram(shaderProgram);

	return true;
}

void Render::SetBackgroundColor(SDL_Color color)
{
	background = color;
}
