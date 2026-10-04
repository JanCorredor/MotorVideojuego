#include "Render.h"

#include "../Engine.h"
#include "../Window/Window.h"
#include "../Input/Input.h"
#include "../../Utils/Log.h"

#include <glad/glad.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../Scene/Scene.h"
#include "../Scene/GameObjects/Components/TransformComponent.h"
#include "../Scene/GameObjects/Components/MeshComponent.h"
#include "../Scene/GameObjects/Components/MaterialComponent.h"

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
	
	int width, height;
	Engine::GetInstance().window->GetWindowSize(width, height);
	CreateFramebuffer(width > 0 ? width : 800, height > 0 ? height : 600);

	return true;
}

void Render::CreateFramebuffer(int width, int height)
{
	if (framebuffer != 0) {
		glDeleteFramebuffers(1, &framebuffer);
		glDeleteTextures(1, &textureColorbuffer);
		glDeleteRenderbuffers(1, &rbo);
	}
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

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG("ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Called each loop iteration
bool Render::PreUpdate()
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
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
	if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	if (cullFaceEnabled) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);

	if (wireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	rotationAngle += 0.002f * dt;

	glUseProgram(shaderProgram);


	// View Matrix
	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -6.0f));

	// Projection Matrix
	int width, height;
	Engine::GetInstance().window->GetWindowSize(width, height);
	float aspectRatio = (height > 0) ? (float)width / (float)height : 1.0f;
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);

	// Send to GPU
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

	// Get variables location for Uniform of Shader
	int modelLoc = glGetUniformLocation(shaderProgram, "model");
	int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
	int useTexLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
	int texLoc = glGetUniformLocation(shaderProgram, "uTexture");

	// -------------------------------------------------------------------------
	// Draw Scene GameObjects
	// -------------------------------------------------------------------------
	GameObject* root = Engine::GetInstance().scene->GetRoot();
	if (root)
	{
		DrawGameObject(root, modelLoc, colorLoc, useTexLoc, texLoc, glm::mat4(1.0f));
	}

	// Check if file has been dropped in the window
    if (Engine::GetInstance().input->HasDroppedFile()) {
        std::string filePath = Engine::GetInstance().input->GetDroppedFilePath();

        // Check if file extension is compatible (FBX, OBJ, etc.)
        if (filePath.find(".fbx") != std::string::npos || 
            filePath.find(".FBX") != std::string::npos ||
            filePath.find(".obj") != std::string::npos) 
        {
            // CleanUp last model if it exists
            loadedModel.reset(); 

            // Load new model
            loadedModel = ModelLoader::LoadFBX(filePath);
        }

        // Clean up drag & drop event
        Engine::GetInstance().input->ClearDroppedFile();
    }

    // Draw model
    if (loadedModel) {
        glm::mat4 modelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)); // Adjust scale
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        
        // Assign default color/texture
        glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), 0.8f, 0.8f, 0.8f);
        glUniform1i(glGetUniformLocation(shaderProgram, "uUseTexture"), false);
        
        loadedModel->Draw();
    }

	// Unbind FBO and prepare default framebuffer for UI rendering
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	// Reset states that might affect ImGui
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

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

void Render::DrawGameObject(GameObject* go, int modelLoc, int colorLoc, int useTexLoc, int texLoc, const glm::mat4& parentMatrix)
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
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(globalMatrix));

		if (material) {
			if (material->useTexture && material->textureID != 0) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, material->textureID);
				glUniform1i(texLoc, 0);
				glUniform3fv(colorLoc, 1, glm::value_ptr(material->color));
				glUniform1i(useTexLoc, true);
			} else {
				glUniform3fv(colorLoc, 1, glm::value_ptr(material->color));
				glUniform1i(useTexLoc, false);
			}
		} else {
			// Default visual if no material
			glUniform3f(colorLoc, 0.8f, 0.8f, 0.8f);
			glUniform1i(useTexLoc, false);
		}

		mesh->GetPrimitive()->Draw();
	}

	for (GameObject* child : go->GetChildren()) {
		DrawGameObject(child, modelLoc, colorLoc, useTexLoc, texLoc, globalMatrix);
	}
}
