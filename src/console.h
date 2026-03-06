#pragma once

#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>

class Console {
public:
    void init(GLFWwindow* window);

    // Called from GLFW key callback
    void onKey(int key, int action, int mods);
    // Called from GLFW char callback
    void onChar(unsigned int codepoint);

    bool isOpen() const { return open; }

    // Current input text (without the leading '/')
    const std::string& getInput() const { return inputBuffer; }

    // Message log (for display)
    const std::vector<std::string>& getMessages() const { return messages; }

    // Add a message to the console log
    void addMessage(const std::string& msg);

    // Set the command handler (called when Enter is pressed with a command)
    void setCommandHandler(std::function<void(const std::string&)> handler) {
        commandHandler = handler;
    }

private:
    bool open = false;
    std::string inputBuffer;
    std::vector<std::string> messages;
    std::function<void(const std::string&)> commandHandler;
    GLFWwindow* window = nullptr;
};
