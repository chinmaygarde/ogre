// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_IDLE_WAITER_VK_H_
#define FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_IDLE_WAITER_VK_H_

#include <memory>

#include "renderer/backend/vulkan/device_holder_vk.h"

namespace ogre {

class IdleWaiterVK {
 public:
  explicit IdleWaiterVK(std::weak_ptr<DeviceHolderVK> device_holder)
      : device_holder_(std::move(device_holder)) {}

  void WaitIdle() const {
    std::shared_ptr<DeviceHolderVK> strong_device_holder =
        device_holder_.lock();
    if (strong_device_holder && strong_device_holder->GetDevice()) {
      [[maybe_unused]] auto result =
          strong_device_holder->GetDevice().waitIdle();
    }
  }

 private:
  std::weak_ptr<DeviceHolderVK> device_holder_;
};

}  // namespace ogre

#endif  // FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_IDLE_WAITER_VK_H_
