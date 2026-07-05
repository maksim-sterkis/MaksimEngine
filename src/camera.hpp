#pragma once
#include "input.hpp"
#include <glm/glm.hpp>

namespace vke {

struct CameraState {
  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;

  float yaw;
  float pitch;
};

namespace camera {

void init(CameraState &state, glm::vec3 startPos);
void update(CameraState &state, float dt, const InputState &input,
            bool pointer_locked, double dx, double dy);
glm::mat4 get_view_matrix(const CameraState &state);

} // namespace camera
} // namespace vke
