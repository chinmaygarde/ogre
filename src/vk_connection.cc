#include "vk_connection.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace ep {

VKConnection::VKConnection() {
  [[maybe_unused]] auto& dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;
}

VKConnection::~VKConnection() {}

}  // namespace ep
