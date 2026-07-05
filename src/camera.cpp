#include "camera.hpp"
#include <GLFW/glfw3.h> // For key codes in case they aren't in input.hpp
#include <glm/gtc/matrix_transform.hpp>

namespace vke::camera {

void init(CameraState &state, glm::vec3 startPos) {
  state.position = startPos;
  state.worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
  state.yaw = -90.0f;
  state.pitch = 0.0f;
  state.front = glm::vec3(0.0f, 0.0f, -1.0f);

  // Initial update to calculate right and up vectors
  update(state, 0.0f, {}, false, 0.0, 0.0);
}

void update(CameraState &state, float dt, const InputState &input,
            bool pointer_locked, double dx, double dy) {
  if (pointer_locked) {
    float mouseSensitivity = 0.1f;
    state.yaw += static_cast<float>(dx) * mouseSensitivity;
    state.pitch -=
        static_cast<float>(dy) *
        mouseSensitivity; // Reversed since y-coordinates go from bottom to top

    if (state.pitch > 89.0f)
      state.pitch = 89.0f;
    if (state.pitch < -89.0f)
      state.pitch = -89.0f;
  }

  glm::vec3 front;
  front.x =
      glm::cos(glm::radians(state.yaw)) * glm::cos(glm::radians(state.pitch));
  front.y = glm::sin(glm::radians(state.pitch));
  front.z =
      glm::sin(glm::radians(state.yaw)) * glm::cos(glm::radians(state.pitch));
  state.front = glm::normalize(front);

  state.right = glm::normalize(glm::cross(state.front, state.worldUp));
  state.up = glm::normalize(glm::cross(state.right, state.front));

  if (dt > 0.0f) {
    float velocity = 5.0f * dt;
    if (input::is_key_pressed(input, GLFW_KEY_W))
      state.position += state.front * velocity;
    if (input::is_key_pressed(input, GLFW_KEY_S))
      state.position -= state.front * velocity;
    if (input::is_key_pressed(input, GLFW_KEY_A))
      state.position -= state.right * velocity;
    if (input::is_key_pressed(input, GLFW_KEY_D))
      state.position += state.right * velocity;
    if (input::is_key_pressed(input, GLFW_KEY_SPACE))
      state.position += state.worldUp * velocity;
    if (input::is_key_pressed(input, GLFW_KEY_LEFT_SHIFT))
      state.position -= state.worldUp * velocity;
  }
}

glm::mat4 get_view_matrix(const CameraState &state) {
  return glm::lookAt(state.position, state.position + state.front, state.up);
}

} // namespace vke::camera
