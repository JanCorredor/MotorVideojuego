#include "CameraComponent.h"
#include "../../../Engine.h"
#include "../../../Input/Input.h"
#include "../../../Window/Window.h"
#include "../GameObject.h"
#include "../../Scene.h"
#include "MeshComponent.h"
#include "../../../../Utils/Log.h"
#include <algorithm>

CameraComponent::CameraComponent(GameObject* owner) : Component(COMPONENT_CAMERA, owner)
{
    transform = owner->GetComponent<TransformComponent>();
    UpdateCameraVectors();
}

void CameraComponent::Update(float dt)
{
    if (!transform) return;

    Input* input = Engine::GetInstance().input.get();

    // CONTROL DE RATÓN RELATIVO (Para movimiento infinito de cámara)
    SDL_Window* sdlWindow = Engine::GetInstance().window->GetWindow();
    bool isFreeLooking = (input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_DOWN || input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_REPEAT);
    bool isOrbiting = (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT) && selectedObject;

    if (isFreeLooking || isOrbiting) {
        SDL_SetWindowRelativeMouseMode(sdlWindow, true);
    }
    else {
        SDL_SetWindowRelativeMouseMode(sdlWindow, false);
    }

    float currentSpeed = movementSpeed;

    // Shift: Duplica la velocidad
    if (input->GetKey(SDL_SCANCODE_LSHIFT)) {
        currentSpeed *= 2.0f;
    }

    // Rueda del ratón: Zoom (modificando FOV)
    float scroll = input->GetMouseScrollDelta();
    if (scroll != 0.0f) {
        fov -= scroll * 2.0f;
        fov = std::max(1.0f, std::min(fov, 120.0f));
    }

    // Botón derecho: Free look y WASD
    if (input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_DOWN || input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_REPEAT) {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;
        pitch = std::max(-89.0f, std::min(pitch, 89.0f));

        UpdateCameraVectors();

        glm::vec3 pos = transform->GetPosition();

        // Movimiento WASD: dirección * velocidad * (delta time / 1000)
        if (input->GetKey(SDL_SCANCODE_W)) pos += front * currentSpeed * (dt / 1000);
        if (input->GetKey(SDL_SCANCODE_S)) pos -= front * currentSpeed * (dt / 1000);
        if (input->GetKey(SDL_SCANCODE_A)) pos -= right * currentSpeed * (dt / 1000);
        if (input->GetKey(SDL_SCANCODE_D)) pos += right * currentSpeed * (dt / 1000);

        transform->SetPosition(pos);
    }

    // Alt + Click Izquierdo: Orbitar objeto
    if ((input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT) && selectedObject) {

        TransformComponent* targetTrans = selectedObject->GetComponent<TransformComponent>();
        if (targetTrans) {
            glm::vec2 mouseDelta = input->GetMouseDelta();
            yaw += mouseDelta.x * mouseSensitivity;
            pitch -= mouseDelta.y * mouseSensitivity;
            pitch = std::max(-89.0f, std::min(pitch, 89.0f));

            UpdateCameraVectors();

            glm::vec3 targetPos = targetTrans->GetPosition();
            glm::vec3 newPos = targetPos - (front * orbitDistance);
            transform->SetPosition(newPos);
        }
    }

    // Tecla F: Focus
    if (input->GetKeyDown(SDL_SCANCODE_F) && selectedObject) {
        FocusOnObject(selectedObject);
    }
}

void CameraComponent::UpdateCameraVectors()
{
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    front = glm::normalize(newFront);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 CameraComponent::GetViewMatrix()
{
    glm::vec3 pos = transform ? transform->GetPosition() : glm::vec3(0.0f);
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 CameraComponent::GetProjectionMatrix(float aspectRatio)
{
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void CameraComponent::FocusOnObject(GameObject* target)
{
    TransformComponent* targetTrans = target->GetComponent<TransformComponent>();
    if (targetTrans && transform) {
        selectedObject = target;
        glm::vec3 targetPos = targetTrans->GetPosition();

        transform->SetPosition(targetPos - (front * orbitDistance));
    }
}

bool CameraComponent::RaySphereIntersection(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::vec3 sphereCenter, float sphereRadius, float& intersectionDistance)
{
    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) return false;

    intersectionDistance = (-b - sqrt(discriminant)) / (2.0f * a);
    return intersectionDistance > 0.0f;
}

void CameraComponent::CheckRaycastRecursive(GameObject* go, glm::vec3 rayOrigin, glm::vec3 rayDir, GameObject*& closestObject, float& minDistance, const glm::mat4& parentMatrix)
{
    if (!go->IsActive()) return;

    glm::mat4 globalMatrix = parentMatrix;
    TransformComponent* trans = go->GetComponent<TransformComponent>();

    if (trans) {
        globalMatrix = parentMatrix * trans->GetLocalModelMatrix();

        // Solo intentamos seleccionar objetos que tengan geometría visual y que no sean la propia cámara
        if (go->GetComponent<MeshComponent>() && go != owner) {
            glm::vec3 objPos = glm::vec3(globalMatrix[3]); // La posición global se almacena en la última columna de la matriz

            // Calculamos un radio aproximado basado en la escala global mayor del objeto
            float scaleX = glm::length(glm::vec3(globalMatrix[0]));
            float scaleY = glm::length(glm::vec3(globalMatrix[1]));
            float scaleZ = glm::length(glm::vec3(globalMatrix[2]));
            float radius = std::max({ scaleX, scaleY, scaleZ });

            float hitDistance = 0.0f;
            if (RaySphereIntersection(rayOrigin, rayDir, objPos, radius, hitDistance)) {
                if (hitDistance < minDistance) {
                    minDistance = hitDistance; // Guardamos el más cercano a la cámara
                    closestObject = go;
                }
            }
        }
    }

    // Comprobar los hijos en la jerarquía
    for (GameObject* child : go->GetChildren()) {
        CheckRaycastRecursive(child, rayOrigin, rayDir, closestObject, minDistance, globalMatrix);
    }
}

void CameraComponent::PerformRaycast(float mX, float mY, float width, float height)
{
    // Convertir a Normalized Device Coordinates (NDC) [-1 a 1] usando el tamaño del panel
    float x = (2.0f * mX) / width - 1.0f;
    float y = 1.0f - (2.0f * mY) / height; // Invertido porque la Y en pantalla baja y en 3D sube

    // Coordenadas de Clip y Eye
    glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);

    // Usamos el Aspect Ratio real del panel Scene de ImGui (width / height)
    // para que la proyección del rayo coincida exactamente con la proyección de renderizado
    float aspectRatio = (height > 0.0f) ? (width / height) : 1.0f;

    glm::mat4 projection = GetProjectionMatrix(aspectRatio);

    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    // Coordenadas de World (Rayo final en el mundo)
    glm::mat4 view = GetViewMatrix();
    glm::vec3 ray_wor = glm::vec3(glm::inverse(view) * ray_eye);
    ray_wor = glm::normalize(ray_wor);

    glm::vec3 rayOrigin = transform->GetPosition();

    // Comprobar colisión recursivamente con todos los objetos
    GameObject* root = Engine::GetInstance().scene->GetRoot();
    GameObject* closestObj = nullptr;
    float minDistance = FLT_MAX;

    if (root) {
        CheckRaycastRecursive(root, rayOrigin, ray_wor, closestObj, minDistance, glm::mat4(1.0f));
    }

    // Asignar el objeto colisionado
    if (closestObj) {
        selectedObject = closestObj;
        LOG("Objeto clickeado y seleccionado correctamente.");
    }
}