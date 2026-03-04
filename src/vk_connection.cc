#include "vk_connection.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace ep {

VulkanConnection::VulkanConnection() {
  [[maybe_unused]] auto& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;
}

VulkanConnection::~VulkanConnection() {}

}  // namespace ep
