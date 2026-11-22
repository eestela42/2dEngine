#include <classes/Game/Camera.hpp>

#include <algorithm>
#include <cmath>

Camera::Camera(Window *window)
	: window(window)
{
	UpdateDirection();
}

void Camera::SendKeys(unsigned char *keyState, double mouseMoveX, double mouseMoveY)
{
	const bool cursorDisabled = window->getCursorMode() == GLFW_CURSOR_DISABLED;
	const bool modeChanged = cursorDisabled != freeCamera;
	if (modeChanged)
	{
		if (cursorDisabled)
			freePosition = followPosition + offset;
		freeCamera = cursorDisabled;
		firstMouse = true;
	}

	if (freeCamera && !modeChanged)
	{
		const float speedMultiplier = (keyState[KEY_SPRINT] & KEY_HOLD) ? 20.0f : 1.0f;
		glm::vec3 horizontalForward(direction.x, 0.0f, direction.z);
		if (glm::length(horizontalForward) > 0.0001f)
			horizontalForward = glm::normalize(horizontalForward);
		else
			horizontalForward = glm::normalize(glm::vec3(
				std::cos(glm::radians(yaw)), 0.0f, std::sin(glm::radians(yaw))));

		const glm::vec3 horizontalRight = glm::normalize(glm::cross(horizontalForward, cameraUp));
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
		freePosition += speed * speedMultiplier * horizontalMovement;

		if (keyState[KEY_SPACE] & KEY_HOLD)
			freePosition += cameraUp * speed * speedMultiplier;
		if (keyState[KEY_MOVE_DOWNWARD] & KEY_HOLD)
			freePosition -= cameraUp * speed * speedMultiplier;

		if (keyState[KEY_DELETE_ONE_BLOCK] & KEY_PRESS)
			autoMove = !autoMove;
		if (autoMove)
			freePosition += 2.0f * speed * speedMultiplier * horizontalForward;
	}

	double xpos, ypos;
	glfwGetCursorPos(window->GetWindow(), &xpos, &ypos);

	if (freeCamera)
	{
		if (firstMouse)
		{
			lastX = static_cast<float>(xpos);
			lastY = static_cast<float>(ypos);
			firstMouse = false;
		}

		else if (!modeChanged)
		{
			mouseMoveX = xpos - lastX;
			mouseMoveY = ypos - lastY;
			yaw += static_cast<float>(mouseMoveX) * sensitivity;
			pitch -= static_cast<float>(mouseMoveY) * sensitivity;
			pitch = glm::clamp(pitch, -89.0f, 89.0f);
		}

		lastX = static_cast<float>(xpos);
		lastY = static_cast<float>(ypos);
	}
	else
	{
		firstMouse = true;
	}

	UpdateDirection();
}

void Camera::SetFollowPosition(const glm::vec3 &playerPosition)
{
	followPosition = playerPosition;
}

bool Camera::IsFree() const
{
	return freeCamera;
}

glm::vec3 Camera::GetPosition(const glm::vec3 &playerPosition) const
{
	if (freeCamera)
		return freePosition;
	return playerPosition + offset;
}

const glm::vec3 &Camera::GetDirection() const
{
	return direction;
}

glm::mat4 Camera::GetViewMatrix(const glm::vec3 &playerPosition) const
{
	const glm::vec3 position = GetPosition(playerPosition);
	return glm::lookAt(position, position + glm::normalize(direction), cameraUp);
}

void Camera::Zoom(const glm::vec3 &playerPosition, float distance)
{
	if (distance == 0.0f)
		return;

	const glm::vec3 normalizedDirection = glm::normalize(direction);
	float adjustedDistance = distance;
	const glm::vec3 position = GetPosition(playerPosition);

	if (distance > 0.0f && normalizedDirection.y < -0.001f)
	{
		const float distanceToGround = -position.y / normalizedDirection.y;
		if (distanceToGround >= 0.0f)
			adjustedDistance = std::min(distance, std::max(0.0f, distanceToGround - 0.5f));
	}

	if (freeCamera)
		freePosition += normalizedDirection * adjustedDistance;
	else
		offset += normalizedDirection * adjustedDistance;
}

void Camera::UpdateDirection()
{
	direction.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
	direction.y = std::sin(glm::radians(pitch));
	direction.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
}
