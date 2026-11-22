#include <classes/Game/InputHandler.hpp>

InputHandler::InputHandler(GLFWwindow *window) : window(window) {
    MapKeys();
    glfwGetCursorPos(window, &posX, &posY);
	glfwSetWindowUserPointer(window, this);
	glfwSetScrollCallback(window, ScrollCallback);
}

void InputHandler::AddCallback(I_Input *newCallback) {
    callbackVector.push_back(newCallback);
}

void InputHandler::HandleInput()
{
	double newPosX, newPosY, mouseMoveX, mouseMoveY;
    glfwGetCursorPos(window, &newPosX, &newPosY);
	mouseMoveX = newPosX - posX;
	mouseMoveY = newPosY - posY;
	posX = newPosX;
	posY = newPosY;

	for (u_int key = 0; key <= KEY_LAST; key++)
		keyState[key] = 0;

	for (int i = 0; i <= GLFW_KEY_LAST; i++) {
		const u_int key = keyMap[i];
		if (!key)
			continue;

		const bool isDown = glfwGetKey(window, i) == GLFW_PRESS;
		if (isDown) {
			keyState[key] |= KEY_HOLD;
			if (!keyWasDown[i])
				keyState[key] |= KEY_PRESS;
		}
		keyWasDown[i] = isDown;
	}
	const InputFrame inputFrame{keyState, mouseMoveX, mouseMoveY, scrollX, scrollY};
	scrollX = 0.0;
	scrollY = 0.0;
	for (I_Input *callback : callbackVector)
		callback->SendInput(inputFrame);
}

void InputHandler::ScrollCallback(GLFWwindow *window, double xOffset, double yOffset)
{
	InputHandler *handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
	if (handler)
	{
		handler->scrollX += xOffset;
		handler->scrollY += yOffset;
	}
}

void InputHandler::MapKeys() {
    keyMap[GLFW_KEY_W] = KEY_MOVE_FORWARD;
    keyMap[GLFW_KEY_S] = KEY_MOVE_BACKWARD;
    keyMap[GLFW_KEY_D] = KEY_MOVE_RIGHTWARD;
    keyMap[GLFW_KEY_A] = KEY_MOVE_LEFTWARD;
    keyMap[GLFW_KEY_LEFT_SHIFT] = KEY_SPRINT;
    keyMap[GLFW_KEY_LEFT_CONTROL] = KEY_MOVE_DOWNWARD;

	

    keyMap[GLFW_KEY_ENTER] = KEY_ENTER;
    keyMap[GLFW_KEY_ESCAPE] = KEY_ESCAPE;
	keyMap[GLFW_KEY_SPACE] = KEY_SPACE;

    keyMap[GLFW_KEY_KP_8] = KEY_ROTATE_X_NEGATIVE;
    keyMap[GLFW_KEY_KP_5] = KEY_ROTATE_X_POSITIVE;
    keyMap[GLFW_KEY_KP_4] = KEY_ROTATE_Y_NEGATIVE;
    keyMap[GLFW_KEY_KP_6] = KEY_ROTATE_Y_POSITIVE;
    keyMap[GLFW_KEY_KP_7] = KEY_ROTATE_Z_NEGATIVE;
    keyMap[GLFW_KEY_KP_9] = KEY_ROTATE_Z_POSITIVE;
	keyMap[GLFW_KEY_Q] = KEY_DELETE_ONE_BLOCK;
	keyMap[GLFW_KEY_E] = KEY_DELETE_MORE_BLOCK;
	keyMap[GLFW_KEY_P] = KEY_DISPLAY_INFO;
	keyMap[GLFW_KEY_LEFT_ALT] = KEY_SWITCH_CURSOR_MODE;


}

InputHandler::~InputHandler() {}