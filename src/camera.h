#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 80.0f, 0.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 10.0f;
    float sensitivity = 0.1f;
    float fov = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

    void updateVectors();
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    void processMouseMovement(float xoffset, float yoffset);
    void processKeyboard(int direction, float deltaTime); // 0=forward, 1=back, 2=left, 3=right, 4=up, 5=down
};
