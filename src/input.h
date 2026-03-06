#pragma once

#include <GLFW/glfw3.h>
#include "camera.h"
#include "console.h"

// Forward declare
class Player;

class InputHandler {
public:
    void init(GLFWwindow* window, Camera* camera);
    void update(float deltaTime);

    bool isKeyPressed(int key) const;
    bool shouldClose() const;

    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    bool firstMouse = true;
    float lastX = 640.0f;
    float lastY = 360.0f;
    bool cursorLocked = true;
    bool* framebufferResized = nullptr;

    Console* console = nullptr;
    Player* player = nullptr;

private:
    GLFWwindow* window = nullptr;
    Camera* camera = nullptr;
};
