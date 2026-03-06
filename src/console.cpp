#include "console.h"
#include <algorithm>

void Console::init(GLFWwindow* win) {
    window = win;
}

void Console::onKey(int key, int action, int /*mods*/) {
    if (!open) {
        if (key == GLFW_KEY_SLASH && action == GLFW_PRESS) {
            open = true;
            inputBuffer.clear();
            return;
        }
        return;
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_ESCAPE) {
            open = false;
            inputBuffer.clear();
        } else if (key == GLFW_KEY_ENTER) {
            if (!inputBuffer.empty() && commandHandler) {
                std::string cmd = "/" + inputBuffer;
                addMessage(cmd);
                commandHandler(inputBuffer);
            }
            open = false;
            inputBuffer.clear();
        } else if (key == GLFW_KEY_BACKSPACE) {
            if (!inputBuffer.empty()) {
                inputBuffer.pop_back();
            }
        }
    }
}

void Console::onChar(unsigned int codepoint) {
    if (!open) return;
    if (codepoint >= 32 && codepoint < 127) {
        if (inputBuffer.empty() && codepoint == '/') return;
        inputBuffer += static_cast<char>(codepoint);
    }
}

void Console::addMessage(const std::string& msg) {
    messages.push_back(msg);
    if (messages.size() > 10) {
        messages.erase(messages.begin());
    }
}
