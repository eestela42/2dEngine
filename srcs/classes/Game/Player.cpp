#include <classes/Game/Player.hpp>

#include <cmath>

Player::Player(Window *window, Camera &camera)
	: window(window), camera(camera)
{
}

void Player::SendKeys(unsigned char *keyState, double mouseMoveX, double mouseMoveY)
{
	(void)mouseMoveX;
	(void)mouseMoveY;

	const bool freeCamera = window->getCursorMode() == GLFW_CURSOR_DISABLED;
	const float speedMultiplier = (keyState[KEY_SPRINT] & KEY_HOLD) ? 20.0f : 1.0f;
	std::lock_guard<std::mutex> lock(positionMutex);

	if (!freeCamera)
	{
		const glm::vec3 &cameraDirection = camera.GetDirection();
		glm::vec3 horizontalForward(cameraDirection.x, 0.0f, cameraDirection.z);
		if (glm::length(horizontalForward) > 0.0001f)
			horizontalForward = glm::normalize(horizontalForward);
		else
			horizontalForward = glm::normalize(glm::vec3(
				std::cos(glm::radians(-90.0f)), 0.0f, std::sin(glm::radians(-90.0f))));

		const glm::vec3 horizontalRight = glm::normalize(glm::cross(horizontalForward, worldUp));
		glm::vec3 horizontalMovement(0.0f);

		if (keyState[KEY_MOVE_FORWARD] & KEY_HOLD)
			horizontalMovement += horizontalForward;
		if (keyState[KEY_MOVE_BACKWARD] & KEY_HOLD)
			horizontalMovement -= horizontalForward;
		if (keyState[KEY_MOVE_RIGHTWARD] & KEY_HOLD)
			horizontalMovement += horizontalRight;
		if (keyState[KEY_MOVE_LEFTWARD] & KEY_HOLD)
			horizontalMovement -= horizontalRight;

		if (glm::length(horizontalMovement) > 1.0f)
			horizontalMovement = glm::normalize(horizontalMovement);
		position += speed * speedMultiplier * horizontalMovement;

		if (keyState[KEY_SPACE] & KEY_HOLD)
			position += worldUp * speed * speedMultiplier;
		if (keyState[KEY_MOVE_DOWNWARD] & KEY_HOLD)
			position -= worldUp * speed * speedMultiplier;

		if (keyState[KEY_DELETE_ONE_BLOCK] & KEY_PRESS)
			autoMove = !autoMove;
		if (autoMove)
			position += 2.0f * speed * speedMultiplier * horizontalForward;
	}

	camera.SetFollowPosition(position);

	if (keyState[KEY_SWITCH_CURSOR_MODE] & KEY_PRESS)
		window->switchCursorMode();
}

glm::vec3 &Player::GetPosition()
{
	return position;
}

std::mutex &Player::GetPositionMutex()
{
	return positionMutex;
}
