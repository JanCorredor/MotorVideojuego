#include "CameraComponent.h"
#include "../../../Engine.h"
#include "../../../Input/Input.h"
#include "../GameObject.h"
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
    float currentSpeed = movementSpeed;

    // Shift: Duplica la velocidad
    if (input->GetKey(SDL_SCANCODE_LSHIFT)) {
        currentSpeed *= 2.0f;
    }

    // Rueda del ratón: Zoom (modificando FOV)
    float scroll = input->GetMouseScrollDelta();
    if (scroll != 0.0f) {
        fov -= scroll * 2.0f;
        // Corrección C3861: Sustitución de std::clamp por compatibilidad
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
        if (input->GetKey(SDL_SCANCODE_W)) pos += front * currentSpeed * dt;
        if (input->GetKey(SDL_SCANCODE_S)) pos -= front * currentSpeed * dt;
        if (input->GetKey(SDL_SCANCODE_A)) pos -= right * currentSpeed * dt;
        if (input->GetKey(SDL_SCANCODE_D)) pos += right * currentSpeed * dt;
        transform->SetPosition(pos);
    }

    // Alt + Click Izquierdo: Orbitar objeto
    if ((input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT) &&
        (input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_DOWN || input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_REPEAT) &&
        selectedObject) {

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