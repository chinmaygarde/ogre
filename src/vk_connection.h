#pragma once

#include "vk.h"

namespace ep {

class VulkanConnection {
 public:
  VulkanConnection();

  ~VulkanConnection();

  VulkanConnection(const VulkanConnection&) = delete;

  VulkanConnection& operator=(const VulkanConnection&) = delete;
};

}  // namespace ep
