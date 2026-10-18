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
    Scene* scene = Engine::GetInstance().scene.get();
    GameObject* selectedObject = scene ? scene->GetSelected() : nullptr;

    SDL_Window* sdlWindow = Engine::GetInstance().window->GetWindow();
    bool rmbDown = (input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_DOWN);
    bool rmbHeld = (rmbDown || input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_REPEAT);
    bool altHeld = (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT);
    bool lmbDown = (input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_DOWN);
    bool lmbHeld = (lmbDown || input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_REPEAT);

    // Solo iniciamos el control de cámara si el clic comienza dentro del viewport de Scene
    if ((rmbDown || (altHeld && lmbDown)) && viewportHovered)
    {
        isControllingCamera = true;
    }
    if (!rmbHeld && !(altHeld && lmbHeld))
    {
        isControllingCamera = false;
    }

    bool isFreeLooking = isControllingCamera && rmbHeld;
    bool isOrbiting = isControllingCamera && altHeld && lmbHeld && selectedObject != nullptr;

    SDL_SetWindowRelativeMouseMode(sdlWindow, isFreeLooking || isOrbiting);

    float currentSpeed = movementSpeed;
    if (input->GetKey(SDL_SCANCODE_LSHIFT))
    {
        currentSpeed *= 2.0f;
    }

    // Rueda del ratón: Zoom (solo cuando el cursor está sobre el viewport de Scene)
    if (viewportHovered)
    {
        float scroll = input->GetMouseScrollDelta();
        if (scroll != 0.0f)
        {
            fov -= scroll * 2.0f;
            fov = std::max(1.0f, std::min(fov, 120.0f));
        }
    }

    // Botón derecho: Free look y WASD
    if (isFreeLooking)
    {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;
        pitch = std::max(-89.0f, std::min(pitch, 89.0f));

        UpdateCameraVectors();

        glm::vec3 pos = transform->GetPosition();
        float step = currentSpeed * (dt / 1000.0f);

        if (input->GetKey(SDL_SCANCODE_W)) pos += front * step;
        if (input->GetKey(SDL_SCANCODE_S)) pos -= front * step;
        if (input->GetKey(SDL_SCANCODE_A)) pos -= right * step;
        if (input->GetKey(SDL_SCANCODE_D)) pos += right * step;
        if (input->GetKey(SDL_SCANCODE_E)) pos += worldUp * step;
        if (input->GetKey(SDL_SCANCODE_Q)) pos -= worldUp * step;

        transform->SetPosition(pos);
    }

    // Alt + Click Izquierdo: Orbitar objeto seleccionado
    if (isOrbiting && selectedObject)
    {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;
        pitch = std::max(-89.0f, std::min(pitch, 89.0f));

        UpdateCameraVectors();

        glm::vec3 targetPos = glm::vec3(selectedObject->GetWorldMatrix()[3]);
        glm::vec3 newPos = targetPos - (front * orbitDistance);
        transform->SetPosition(newPos);
    }

    // Tecla F: Focus en el objeto seleccionado (cuando el viewport está activo)
    if (viewportHovered && input->GetKeyDown(SDL_SCANCODE_F) && selectedObject)
    {
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
    if (!target || !transform) return;
    if (Engine::GetInstance().scene)
    {
        Engine::GetInstance().scene->SetSelected(target);
    }
    glm::vec3 targetPos = glm::vec3(target->GetWorldMatrix()[3]);
    transform->SetPosition(targetPos - (front * orbitDistance));
}

void CameraComponent::ResetCamera()
{
    yaw = -90.0f;
    pitch = 0.0f;
    fov = 45.0f;
    UpdateCameraVectors();
    if (transform)
    {
        transform->SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
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

    if (trans)
    {
        globalMatrix = parentMatrix * trans->GetLocalModelMatrix();

        MeshComponent* mesh = go->GetComponent<MeshComponent>();
        if (mesh && mesh->GetPrimitive() && go != owner)
        {
            Primitive* prim = mesh->GetPrimitive();
            glm::vec3 localCenter = 0.5f * (prim->GetBoundsMin() + prim->GetBoundsMax());
            float localRadius = 0.5f * glm::length(prim->GetBoundsMax() - prim->GetBoundsMin());
            if (localRadius <= 0.001f) localRadius = 0.75f;

            glm::vec3 objPos = glm::vec3(globalMatrix * glm::vec4(localCenter, 1.0f));

            float scaleX = glm::length(glm::vec3(globalMatrix[0]));
            float scaleY = glm::length(glm::vec3(globalMatrix[1]));
            float scaleZ = glm::length(glm::vec3(globalMatrix[2]));
            float radius = localRadius * std::max({ scaleX, scaleY, scaleZ });

            float hitDistance = 0.0f;
            if (RaySphereIntersection(rayOrigin, rayDir, objPos, radius, hitDistance))
            {
                if (hitDistance < minDistance)
                {
                    minDistance = hitDistance;
                    closestObject = go;
                }
            }
        }
    }

    for (GameObject* child : go->GetChildren())
    {
        CheckRaycastRecursive(child, rayOrigin, rayDir, closestObject, minDistance, globalMatrix);
    }
}

void CameraComponent::PerformRaycast(float mX, float mY, float width, float height)
{
    if (width <= 0.0f || height <= 0.0f || !transform) return;

    float x = (2.0f * mX) / width - 1.0f;
    float y = 1.0f - (2.0f * mY) / height;

    glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);
    float aspectRatio = width / height;
    glm::mat4 projection = GetProjectionMatrix(aspectRatio);

    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    glm::mat4 view = GetViewMatrix();
    glm::vec3 ray_wor = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    glm::vec3 rayOrigin = transform->GetPosition();

    Scene* scene = Engine::GetInstance().scene.get();
    if (!scene) return;

    GameObject* root = scene->GetRoot();
    GameObject* closestObj = nullptr;
    float minDistance = FLT_MAX;

    if (root)
    {
        CheckRaycastRecursive(root, rayOrigin, ray_wor, closestObj, minDistance, glm::mat4(1.0f));
    }

    scene->SetSelected(closestObj);
    if (closestObj)
    {
        LOG("[Scene] GameObject seleccionado por Raycast: '%s'", closestObj->GetName().c_str());
    }
}