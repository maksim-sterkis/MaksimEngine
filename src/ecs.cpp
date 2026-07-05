#include "ecs.hpp"

namespace vke::ecs {

Entity create_entity(Registry &registry) {
  Entity e = registry.next_entity_id++;
  registry.active_entities.push_back(e);

  // Ensure capacity
  if (e >= registry.has_transform.size()) {
    registry.has_transform.resize(e + 1, false);
    registry.transforms.resize(e + 1);

    registry.has_mesh.resize(e + 1, false);
    registry.meshes.resize(e + 1);
  }

  return e;
}

void add_transform(Registry &registry, Entity entity,
                   const Transform &transform) {
  if (entity < registry.has_transform.size()) {
    registry.has_transform[entity] = true;
    registry.transforms[entity] = transform;
  }
}

void add_mesh_renderer(Registry &registry, Entity entity,
                       const MeshRenderer &mesh) {
  if (entity < registry.has_mesh.size()) {
    registry.has_mesh[entity] = true;
    registry.meshes[entity] = mesh;
  }
}

Transform *get_transform(Registry &registry, Entity entity) {
  if (entity < registry.has_transform.size() &&
      registry.has_transform[entity]) {
    return &registry.transforms[entity];
  }
  return nullptr;
}

MeshRenderer *get_mesh_renderer(Registry &registry, Entity entity) {
  if (entity < registry.has_mesh.size() && registry.has_mesh[entity]) {
    return &registry.meshes[entity];
  }
  return nullptr;
}

} // namespace vke::ecs
