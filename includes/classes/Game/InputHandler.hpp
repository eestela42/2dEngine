#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <interfaces/Interface_Input.hpp>

class InputHandler
{
    public:
        InputHandler(GLFWwindow *window);
        ~InputHandler();

        void AddCallback(I_Input*);
		void HandleInput();
    private:
        std::vector<I_Input*> callbackVector;
        GLFWwindow *window;
        int keyMap[GLFW_KEY_LAST + 1] = {0};
		u_char keyState[KEY_LAST + 1] = {0};
		bool keyWasDown[GLFW_KEY_LAST + 1] = {false};
		double posX, posY;
		double scrollX = 0.0;
		double scrollY = 0.0;

        void MapKeys();
		static void ScrollCallback(GLFWwindow *window, double xOffset, double yOffset);
};

#endif