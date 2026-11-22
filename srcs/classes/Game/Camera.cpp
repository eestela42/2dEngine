#include <classes/Game/Camera.hpp>

#include <algorithm>
#include <cmath>
#include "../../../imgui/imgui.h"

Camera::Camera(Window *window)
	: window(window)
{
	UpdateDirection();
}

void Camera::SendInput(const InputFrame &input)
{
	const unsigned char *keyState = input.keyState;
	double mouseMoveX = input.mouseMoveX;
	double mouseMoveY = input.mouseMoveY;
	const bool cursorDisabled = window->getCursorMode() == GLFW_CURSOR_DISABLED;
	const bool modeChanged = cursorDisabled != freeCamera;
	if (modeChanged)
	{
		if (cursorDisabled)
			freePosition = GetPosition(followPosition);
		else
			FocusOn(followPosition + cameraUp * focusHeight);
		freeCamera = cursorDisabled;
		firstMouse = true;
	}

	float zoomDistance = static_cast<float>(input.scrollY) * 2.0f;

	const float speedMultiplier = (keyState[KEY_SPRINT] & KEY_HOLD) ? 20.0f : 1.0f;

	if (freeCamera && !modeChanged)
	{
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
	else if (!freeCamera && !modeChanged)
	{
		if (keyState[KEY_SPACE] & KEY_HOLD)
			zoomDistance += speed * speedMultiplier;
		if (keyState[KEY_MOVE_DOWNWARD] & KEY_HOLD)
			zoomDistance -= speed * speedMultiplier;
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
	Zoom(zoomDistance);
}

void Camera::SetFollowPosition(const glm::vec3 &playerPosition)
{
	followPosition = playerPosition;
}

void Camera::FocusOn(const glm::vec3 &target)
{
	const glm::vec3 cameraPosition = freeCamera ? freePosition : GetPosition(followPosition);
	const glm::vec3 directionToTarget = target - cameraPosition;
	const float distanceToTarget = glm::length(directionToTarget);
	if (distanceToTarget <= 0.0001f)
		return;

	focusDistance = distanceToTarget;
	direction = glm::normalize(directionToTarget);
	yaw = glm::degrees(std::atan2(direction.z, direction.x));
	pitch = glm::degrees(std::asin(glm::clamp(direction.y, -1.0f, 1.0f)));
	UpdateDirection();
}

glm::vec3 Camera::GetPosition(const glm::vec3 &playerPosition) const
{
	if (freeCamera)
		return freePosition;
	const glm::vec3 focusTarget = playerPosition + cameraUp * focusHeight;
	return focusTarget - direction * focusDistance;
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

void Camera::Zoom(float distance)
{
	if (distance == 0.0f)
		return;

	const glm::vec3 normalizedDirection = glm::normalize(direction);
	float adjustedDistance = distance;
	const glm::vec3 position = GetPosition(followPosition);

	if (distance > 0.0f && normalizedDirection.y < -0.001f)
	{
		const float distanceToGround = -position.y / normalizedDirection.y;
		if (distanceToGround >= 0.0f)
			adjustedDistance = std::min(distance, std::max(0.0f, distanceToGround - 0.5f));
	}

	if (freeCamera)
		freePosition += normalizedDirection * adjustedDistance;
	else
		focusDistance = std::max(0.5f, focusDistance - adjustedDistance);
}

void Camera::DrawUI()
{
	if (freeCamera || !ImGui::CollapsingHeader("Camera lock"))
		return;

	float lockedYaw = yaw;
	float lockedPitch = pitch;
	float lockedDistance = focusDistance;
	const bool yawChanged = ImGui::SliderFloat("Yaw", &lockedYaw, -180.0f, 180.0f);
	const bool pitchChanged = ImGui::SliderFloat("Pitch", &lockedPitch, -89.0f, 89.0f);
	if (yawChanged || pitchChanged)
	{
		yaw = lockedYaw;
		pitch = glm::clamp(lockedPitch, -89.0f, 89.0f);
		UpdateDirection();
	}
	if (ImGui::SliderFloat("Distance", &lockedDistance, 0.5f, 200.0f))
		focusDistance = std::max(0.5f, lockedDistance);
}

void Camera::UpdateDirection()
{
	direction.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
	direction.y = std::sin(glm::radians(pitch));
	direction.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
}
