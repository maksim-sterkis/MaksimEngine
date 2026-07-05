#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace vke {

typedef uint32_t Entity;

struct Transform {
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 rotation = glm::vec3(0.0f); // euler angles in radians
  glm::vec3 scale = glm::vec3(1.0f);

  glm::mat4 get_matrix() const {
    glm::mat4 mat = glm::mat4(1.0f);
    mat = glm::translate(mat, position);
    mat = glm::rotate(mat, rotation.y, glm::vec3(0, 1, 0));
    mat = glm::rotate(mat, rotation.x, glm::vec3(1, 0, 0));
    mat = glm::rotate(mat, rotation.z, glm::vec3(0, 0, 1));
    mat = glm::scale(mat, scale);
    return mat;
  }
};

struct MeshRenderer {
  uint32_t model_handle = 0;
  uint32_t texture_handle = 0;
  bool use_triplanar = false;
};

// A simple structure of arrays (SoA) registry for DOD
// For maximum simplicity, components are accessed by parallel dense arrays
// indexed by Entity ID.
struct Registry {
  uint32_t next_entity_id = 0;
  std::vector<Entity> active_entities;

  // Dense component arrays (index matches Entity ID)
  std::vector<bool> has_transform;
  std::vector<Transform> transforms;

  std::vector<bool> has_mesh;
  std::vector<MeshRenderer> meshes;
};

namespace ecs {
Entity create_entity(Registry &registry);
void add_transform(Registry &registry, Entity entity,
                   const Transform &transform);
void add_mesh_renderer(Registry &registry, Entity entity,
                       const MeshRenderer &mesh);

Transform *get_transform(Registry &registry, Entity entity);
MeshRenderer *get_mesh_renderer(Registry &registry, Entity entity);
} // namespace ecs

} // namespace vke
