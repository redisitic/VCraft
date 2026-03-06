#include "input.h"
#include "player.h"

void InputHandler::init(GLFWwindow* win, Camera* cam) {
    window = win;
    camera = cam;

    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetScrollCallback(window, scrollCallback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void InputHandler::update(float deltaTime) {
    if (!window || !camera)
        return;

    if (console && console->isOpen())
        return;

    if (player) {
        if (isKeyPressed(GLFW_KEY_W))
            player->moveForward(deltaTime);
        if (isKeyPressed(GLFW_KEY_S))
            player->moveBack(deltaTime);
        if (isKeyPressed(GLFW_KEY_A))
            player->moveLeft(deltaTime);
        if (isKeyPressed(GLFW_KEY_D))
            player->moveRight(deltaTime);
        if (isKeyPressed(GLFW_KEY_SPACE))
            player->jump();
    } else {
        if (isKeyPressed(GLFW_KEY_W))
            camera->processKeyboard(0, deltaTime);
        if (isKeyPressed(GLFW_KEY_S))
            camera->processKeyboard(1, deltaTime);
        if (isKeyPressed(GLFW_KEY_A))
            camera->processKeyboard(2, deltaTime);
        if (isKeyPressed(GLFW_KEY_D))
            camera->processKeyboard(3, deltaTime);
        if (isKeyPressed(GLFW_KEY_SPACE))
            camera->processKeyboard(4, deltaTime);
        if (isKeyPressed(GLFW_KEY_LEFT_SHIFT))
            camera->processKeyboard(5, deltaTime);
    }
}

bool InputHandler::isKeyPressed(int key) const {
    return window && glfwGetKey(window, key) == GLFW_PRESS;
}

bool InputHandler::shouldClose() const {
    return window && glfwWindowShouldClose(window);
}

void InputHandler::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
    if (!handler || !handler->cursorLocked)
        return;

    if (handler->console && handler->console->isOpen())
        return;

    float xf = static_cast<float>(xpos);
    float yf = static_cast<float>(ypos);

    if (handler->firstMouse) {
        handler->lastX = xf;
        handler->lastY = yf;
        handler->firstMouse = false;
    }

    float xoffset = xf - handler->lastX;
    float yoffset = handler->lastY - yf;

    handler->lastX = xf;
    handler->lastY = yf;

    handler->camera->processMouseMovement(xoffset, yoffset);
}

void InputHandler::mouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
    if (!handler || !handler->cursorLocked)
        return;

    if (handler->console && handler->console->isOpen())
        return;

    if (!handler->player)
        return;

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            handler->player->breakBlock();
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            handler->player->placeBlock();
        }
    }
}

void InputHandler::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
    if (handler && handler->framebufferResized)
        *(handler->framebufferResized) = true;
}

void InputHandler::keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
    if (!handler)
        return;

    if (handler->console) {
        bool wasOpen = handler->console->isOpen();
        handler->console->onKey(key, action, mods);
        if (wasOpen || handler->console->isOpen())
            return;
    }

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        handler->cursorLocked = !handler->cursorLocked;
        if (handler->cursorLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            handler->firstMouse = true;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void InputHandler::charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
    if (!handler || !handler->console)
        return;

    handler->console->onChar(codepoint);
}

void InputHandler::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
    if (!handler || !handler->player)
        return;
    
    if (handler->console && handler->console->isOpen())
        return;

    int& slot = handler->player->inventory.selectedSlot;
    slot -= static_cast<int>(yoffset);
    slot = ((slot % HOTBAR_SLOTS) + HOTBAR_SLOTS) % HOTBAR_SLOTS;
}
