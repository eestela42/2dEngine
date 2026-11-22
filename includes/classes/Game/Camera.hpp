#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <classes/Window.hpp>
#include <interfaces/Interface_Input.hpp>

class Camera : public I_Input
{
public:
	Camera(Window *window);

	void SendInput(const InputFrame &input) override;
	void SetFollowPosition(const glm::vec3 &playerPosition);
	glm::vec3 GetPosition(const glm::vec3 &playerPosition) const;
	const glm::vec3 &GetDirection() const;
	glm::mat4 GetViewMatrix(const glm::vec3 &playerPosition) const;
	void DrawUI();

private:
	void UpdateDirection();
	void FocusOn(const glm::vec3 &target);
	void Zoom(float distance);

	Window *window;
	float yaw = -90.0f;
	float pitch = -75.0f;
	const float sensitivity = 0.05f;
	const glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 direction;
	float focusDistance = 50.0f;
	const float focusHeight = 1.0f;
	glm::vec3 followPosition = glm::vec3(0.0f);
	glm::vec3 freePosition = glm::vec3(0.0f);
	const float speed = 0.15f;
	bool freeCamera = false;
	bool autoMove = false;
	bool firstMouse = true;
	float lastX = 0.0f;
	float lastY = 0.0f;
};

#endif
