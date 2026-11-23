#include <classes/Game/Player.hpp>

#include <algorithm>
#include <cmath>

#include <classes/World/AChunk.hpp>
#include <classes/ECS/System.hpp>
#include "../../../imgui/imgui.h"

Player::Player(Window *window, Camera &camera)
	: window(window), camera(camera)
{
}

bool Player::SetupChunks(std::vector<std::vector<AChunk*>> &tabChunks, std::mutex &tabChunks_mutex)
{
	this->tabChunks = &tabChunks;
	this->tabChunks_mutex = &tabChunks_mutex;
	return true;
}

void Player::DrawUI()
{
	if (!ImGui::CollapsingHeader("Player"))
		return;

	ImGui::SliderFloat("Movement speed", &speed, 0.01f, 0.5f, "%.2f");
	ImGui::SliderFloat("Collision radius", &collisionRadius, 0.1f, 0.5f, "%.2f");
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
}


void Player::SendInput(const InputFrame &input)
{
	const unsigned char *keyState = input.keyState;

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
		glm::vec2 inputVector(0.0f);

		if (keyState[KEY_MOVE_FORWARD] & KEY_HOLD)
			inputVector.y += 1.0f;
		if (keyState[KEY_MOVE_BACKWARD] & KEY_HOLD)
			inputVector.y -= 1.0f;
		if (keyState[KEY_MOVE_RIGHTWARD] & KEY_HOLD)
			inputVector.x += 1.0f;
		if (keyState[KEY_MOVE_LEFTWARD] & KEY_HOLD)
			inputVector.x -= 1.0f;

		if (glm::length(inputVector) > 1.0f)
			inputVector = glm::normalize(inputVector);

		glm::vec3 movementVector =
			horizontalRight * inputVector.x + horizontalForward * inputVector.y;
		if (keyState[KEY_DELETE_ONE_BLOCK] & KEY_PRESS)
			autoMove = !autoMove;
		if (autoMove)
			movementVector += 2.0f * horizontalForward;

		const float movementMagnitude = glm::length(movementVector);
		if (movementMagnitude > 0.000001f)
		{
			const glm::vec3 direction = movementVector / movementMagnitude;
			movementDirection = direction;
			const float distance = speed * speedMultiplier * movementMagnitude;
			moveWithCollision(direction, distance);
		}
	}

	position.y = 0.0f;
	camera.SetFollowPosition(position);

	if (keyState[KEY_SWITCH_CURSOR_MODE] & KEY_PRESS)
		window->switchCursorMode();
}

bool Player::isBlockFilled(int blockX, int blockZ, int blockHeight) const
{
	if (!tabChunks || !tabChunks_mutex
		|| blockHeight < 0 || blockHeight >= static_cast<int>(AChunk::sizeZ))
		return false;

	const int chunkX = static_cast<int>(std::floor(
		static_cast<float>(blockX) / AChunk::sizeX));
	const int chunkZ = static_cast<int>(std::floor(
		static_cast<float>(blockZ) / AChunk::sizeY));
	const int chunkOriginX = chunkX * static_cast<int>(AChunk::sizeX);
	const int chunkOriginZ = chunkZ * static_cast<int>(AChunk::sizeY);

	AChunk *chunk = getChunk(
		*tabChunks,
		*tabChunks_mutex,
		glm::vec3(chunkOriginX, chunkOriginZ, blockHeight));
	if (!chunk)
		return false;

	const bool filled = chunk->pubIsFilled(
		blockX - chunkOriginX,
		blockZ - chunkOriginZ,
		blockHeight);
	{
		std::lock_guard<std::mutex> lock(*tabChunks_mutex);
		chunk->deleter();
	}
	return filled;
}

bool Player::findBlockingNormal(
	const glm::vec3 &center,
	const glm::vec3 &previousCenter,
	const glm::vec2 &direction,
	glm::vec2 &normal) const
{
	const int blockHeight = static_cast<int>(std::floor(center.y));
	const int minBlockX = static_cast<int>(std::floor(center.x - collisionRadius));
	const int maxBlockX = static_cast<int>(std::floor(center.x + collisionRadius));
	const int minBlockZ = static_cast<int>(std::floor(center.z - collisionRadius));
	const int maxBlockZ = static_cast<int>(std::floor(center.z + collisionRadius));
	const float radiusSquared = collisionRadius * collisionRadius;
	float mostInward = -0.000001f;
	bool foundBlockingBlock = false;

	for (int blockX = minBlockX; blockX <= maxBlockX; ++blockX)
	{
		for (int blockZ = minBlockZ; blockZ <= maxBlockZ; ++blockZ)
		{
			const float closestX = std::max(
				static_cast<float>(blockX),
				std::min(center.x, static_cast<float>(blockX + 1)));
			const float closestZ = std::max(
				static_cast<float>(blockZ),
				std::min(center.z, static_cast<float>(blockZ + 1)));
			const glm::vec2 difference(center.x - closestX, center.z - closestZ);
			const float distanceSquared = glm::dot(difference, difference);
			if (distanceSquared >= radiusSquared || !isBlockFilled(blockX, blockZ, blockHeight))
				continue;

			glm::vec2 contactNormal;
			if (distanceSquared > 0.000001f)
			{
				contactNormal = glm::normalize(difference);
			}
			else
			{
				const glm::vec2 previousDifference(
					previousCenter.x - std::max(
						static_cast<float>(blockX),
						std::min(previousCenter.x, static_cast<float>(blockX + 1))),
					previousCenter.z - std::max(
						static_cast<float>(blockZ),
						std::min(previousCenter.z, static_cast<float>(blockZ + 1))));
				if (glm::dot(previousDifference, previousDifference) > 0.000001f)
				{
					contactNormal = glm::normalize(previousDifference);
				}
				else
				{
					const float distances[] = {
						previousCenter.x - blockX,
						blockX + 1.0f - previousCenter.x,
						previousCenter.z - blockZ,
						blockZ + 1.0f - previousCenter.z
					};
					const glm::vec2 normals[] = {
						glm::vec2(-1.0f, 0.0f),
						glm::vec2(1.0f, 0.0f),
						glm::vec2(0.0f, -1.0f),
						glm::vec2(0.0f, 1.0f)
					};
					int nearestSide = 0;
					for (int side = 1; side < 4; ++side)
					{
						if (distances[side] < distances[nearestSide])
							nearestSide = side;
					}
					contactNormal = normals[nearestSide];
				}
			}

			const float intoWall = glm::dot(direction, contactNormal);
			if (intoWall < mostInward)
			{
				mostInward = intoWall;
				normal = contactNormal;
				foundBlockingBlock = true;
			}
		}
	}
	return foundBlockingBlock;
}

void Player::moveWithCollision(const glm::vec3 &direction, float distance)
{
	const glm::vec2 horizontalDirection(direction.x, direction.z);
	if (distance <= 0.000001f || glm::length(horizontalDirection) <= 0.000001f)
		return;

	const int steps = static_cast<int>(std::ceil(distance / (collisionRadius * 0.5f)));
	const glm::vec2 step = glm::normalize(horizontalDirection) * distance / static_cast<float>(steps);

	for (int i = 0; i < steps; ++i)
	{
		glm::vec2 allowedStep = step;
		for (int attempt = 0; attempt < 3; ++attempt)
		{
			const glm::vec3 candidate = position + glm::vec3(allowedStep.x, 0.0f, allowedStep.y);
			glm::vec2 normal;
			const float allowedDistance = glm::length(allowedStep);
			if (allowedDistance <= 0.000001f)
				break;
			const glm::vec2 stepDirection = allowedStep / allowedDistance;
			if (!findBlockingNormal(candidate, position, stepDirection, normal))
			{
				position.x += allowedStep.x;
				position.z += allowedStep.y;
				break;
			}

			const float intoWall = glm::dot(allowedStep, normal);
			const glm::vec2 slideStep = allowedStep - normal * intoWall;
			if (glm::dot(slideStep, slideStep) <= 0.000001f)
				break;
			allowedStep = slideStep;
		}
	}
}

glm::vec3 &Player::GetPosition()
{
	return position;
}

glm::vec3 Player::GetMovementDirection()
{
	std::lock_guard<std::mutex> lock(positionMutex);
	return movementDirection;
}

std::mutex &Player::GetPositionMutex()
{
	return positionMutex;
}
