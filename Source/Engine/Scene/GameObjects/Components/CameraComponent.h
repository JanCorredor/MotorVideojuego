#pragma once
#include "Component.h"
#include "TransformComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class GameObject;

class CameraComponent : public Component
{
public:
    CameraComponent(GameObject* owner);
    ~CameraComponent() override = default;

    void Update(float dt) override;

    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(float aspectRatio);

    void PerformRaycast(float mouseX, float mouseY, float width, float height);
    void FocusOnObject(GameObject* target);
    void ResetCamera();

    // El Editor informa si el ratón está sobre el viewport de Scene para filtrar el input de cámara
    void SetViewportHovered(bool hovered) { viewportHovered = hovered; }
    bool IsViewportHovered() const { return viewportHovered; }

    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    float GetMovementSpeed() const { return movementSpeed; }
    void SetMovementSpeed(float speed) { movementSpeed = speed; }

    float GetMouseSensitivity() const { return mouseSensitivity; }
    void SetMouseSensitivity(float sens) { mouseSensitivity = sens; }

    float GetOrbitDistance() const { return orbitDistance; }
    void SetOrbitDistance(float dist) { orbitDistance = dist; }

private:
    void UpdateCameraVectors();

    TransformComponent* transform = nullptr;
    bool viewportHovered = false;
    bool isControllingCamera = false;

    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    bool RaySphereIntersection(glm::vec3 rayOrigin, glm::vec3 rayDir, glm::vec3 sphereCenter, float sphereRadius, float& intersectionDistance);
    void CheckRaycastRecursive(GameObject* go, glm::vec3 rayOrigin, glm::vec3 rayDir, GameObject*& closestObject, float& minDistance, const glm::mat4& parentMatrix);

    float yaw = -90.0f;
    float pitch = 0.0f;

    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.1f;
    float orbitDistance = 5.0f;
};