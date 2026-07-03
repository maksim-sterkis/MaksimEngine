#include "model.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace vke {

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
}

Model::Model(DeviceState& state, const std::vector<Vertex>& vertices)
    : deviceState(state) {
    createVertexBuffers(vertices);
}

Model::~Model() {
    vkDestroyBuffer(deviceState.device, vertexBuffer, nullptr);
    vkFreeMemory(deviceState.device, vertexBufferMemory, nullptr);
}

void Model::createVertexBuffers(const std::vector<Vertex>& vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    device::create_buffer(deviceState, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(deviceState.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(deviceState.device, stagingBufferMemory);

    device::create_buffer(deviceState, bufferSize,
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    device::copy_buffer(deviceState, stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(deviceState.device, stagingBuffer, nullptr);
    vkFreeMemory(deviceState.device, stagingBufferMemory, nullptr);
}

void Model::bind(VkCommandBuffer commandBuffer) {
    VkBuffer buffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
}

void Model::draw(VkCommandBuffer commandBuffer) {
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
}

std::unique_ptr<Model> Model::loadOBJ(DeviceState& deviceState, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open OBJ file: " + filepath);
    }

    std::vector<glm::vec3> temp_positions;
    std::vector<Vertex> vertices;

    // Palette of colors for the faces
    glm::vec3 faceColors[] = {
        {1.0f, 0.0f, 0.0f}, // Red
        {0.0f, 1.0f, 0.0f}, // Green
        {0.0f, 0.0f, 1.0f}, // Blue
        {1.0f, 1.0f, 0.0f}, // Yellow
        {1.0f, 0.0f, 1.0f}, // Magenta
        {0.0f, 1.0f, 1.0f}  // Cyan
    };
    int faceIndex = 0;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "v") {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            temp_positions.push_back(pos);
        } else if (type == "f") {
            // A face consists of vertices. (e.g. f 1 2 3)
            int v1, v2, v3;
            // Wavefront obj indices are 1-based.
            if (iss >> v1 >> v2 >> v3) {
                glm::vec3 color = faceColors[faceIndex % 6];
                vertices.push_back({temp_positions[v1 - 1], color});
                vertices.push_back({temp_positions[v2 - 1], color});
                vertices.push_back({temp_positions[v3 - 1], color});
                faceIndex++;
            }
        }
    }

    return std::make_unique<Model>(deviceState, vertices);
}

} // namespace vke
