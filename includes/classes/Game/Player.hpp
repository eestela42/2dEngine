#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <mutex>

#include <glm/glm.hpp>

#include <classes/Game/Camera.hpp>
#include <classes/Window.hpp>
#include <interfaces/Interface_Input.hpp>

class Player : public I_Input
{
public:
	Player(Window *window, Camera &camera);

	void SendInput(const InputFrame &input) override;
	glm::vec3 &GetPosition();
	std::mutex &GetPositionMutex();

private:
	Window *window;
	Camera &camera;
	const float speed = 0.15f;
	const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 position = glm::vec3(0.0f);
	std::mutex positionMutex;
	bool autoMove = false;
};

#endif
