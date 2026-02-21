#pragma once

#include "Mamancraft/Renderer/Vertex.hpp"
#include "Mamancraft/Renderer/Vulkan/VulkanBuffer.hpp"
#include <memory>
#include <vector>


namespace mc {

class VulkanContext;

class VulkanMesh {
public:
  struct Builder {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
  };

  VulkanMesh(VulkanContext &context, const Builder &builder);
  ~VulkanMesh();

  VulkanMesh(const VulkanMesh &) = delete;
  VulkanMesh &operator=(const VulkanMesh &) = delete;

  void Bind(vk::CommandBuffer commandBuffer);
  void Draw(vk::CommandBuffer commandBuffer);

private:
  void CreateVertexBuffers(const std::vector<Vertex> &vertices);
  void CreateIndexBuffers(const std::vector<uint32_t> &indices);

private:
  VulkanContext &m_Context;

  std::unique_ptr<VulkanBuffer> m_VertexBuffer;
  uint32_t m_VertexCount;

  bool m_HasIndexBuffer = false;
  std::unique_ptr<VulkanBuffer> m_IndexBuffer;
  uint32_t m_IndexCount;
};

} // namespace mc
