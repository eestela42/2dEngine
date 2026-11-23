#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <mutex>
#include <vector>

#include <glm/glm.hpp>

#include <classes/Game/Camera.hpp>
#include <classes/Window.hpp>
#include <interfaces/Interface_Input.hpp>

class AChunk;

class Player : public I_Input
{
public:
	Player(Window *window, Camera &camera);

	void SendInput(const InputFrame &input) override;
	void DrawUI();
	void TriggerExplosion();
	glm::vec3 &GetPosition();
	std::mutex &GetPositionMutex();

	bool SetupChunks(std::vector<std::vector<AChunk*>> &tabChunks, std::mutex &tabChunks_mutex);

private:
	void explodeAtPosition();
	bool isBlockFilled(int blockX, int blockZ, int blockHeight) const;
	bool findBlockingNormal(
		const glm::vec3 &center,
		const glm::vec3 &previousCenter,
		const glm::vec2 &direction,
		glm::vec2 &normal) const;
	void moveWithCollision(const glm::vec3 &direction, float distance);

	Window *window;
	Camera &camera;
	float speed = 0.10f;
	float collisionRadius = 0.2f;
	const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 position = glm::vec3(0.0f);
	std::mutex positionMutex;
	bool autoMove = false;

	//THIS IS TEMPORARY AND SHOULD BE PUT IN THE ECS
	std::vector<std::vector<AChunk*>> *tabChunks = nullptr;
	std::mutex *tabChunks_mutex = nullptr;
};

#endif
