#pragma once
#include "device.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

namespace vke {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    
    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class Model {
public:
    Model(DeviceState& deviceState, const std::vector<Vertex>& vertices);
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);
    
    static std::unique_ptr<Model> loadOBJ(DeviceState& deviceState, const std::string& filepath);

    uint32_t vertexCount;

private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);

    DeviceState& deviceState;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
};

} // namespace vke
