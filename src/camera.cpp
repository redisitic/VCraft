#include "camera.h"

#include <cmath>

void Camera::updateVectors() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(dir);

    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const {
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1.0f;
    return proj;
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    updateVectors();
}

void Camera::processKeyboard(int direction, float deltaTime) {
    float velocity = speed * deltaTime;

    switch (direction) {
        case 0: // forward
            position += front * velocity;
            break;
        case 1: // back
            position -= front * velocity;
            break;
        case 2: // left
            position -= right * velocity;
            break;
        case 3: // right
            position += right * velocity;
            break;
        case 4: // up
            position += worldUp * velocity;
            break;
        case 5: // down
            position -= worldUp * velocity;
            break;
        default:
            break;
    }
}
