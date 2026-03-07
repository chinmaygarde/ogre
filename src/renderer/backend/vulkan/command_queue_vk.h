// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_COMMAND_QUEUE_VK_H_
#define FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_COMMAND_QUEUE_VK_H_

#include <functional>
#include <memory>
#include <vector>

#include "fml/status.h"
#include "renderer/backend/vulkan/command_buffer_vk.h"

namespace ogre {

class ContextVK;

class CommandQueueVK {
 public:
  using CompletionCallback = std::function<void(CommandBufferVK::Status)>;

  explicit CommandQueueVK(const std::weak_ptr<ContextVK>& context);

  ~CommandQueueVK();

  fml::Status Submit(const std::vector<std::shared_ptr<CommandBufferVK>>& buffers,
                     const CompletionCallback& completion_callback = {},
                     bool block_on_schedule = false);

 private:
  std::weak_ptr<ContextVK> context_;

  CommandQueueVK(const CommandQueueVK&) = delete;

  CommandQueueVK& operator=(const CommandQueueVK&) = delete;
};

}  // namespace ogre

#endif  // FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_COMMAND_QUEUE_VK_H_
