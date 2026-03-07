// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_SAMPLER_LIBRARY_VK_H_
#define FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_SAMPLER_LIBRARY_VK_H_

#include "core/raw_ptr.h"
#include "core/sampler_descriptor.h"
#include "renderer/backend/vulkan/device_holder_vk.h"
#include "renderer/backend/vulkan/sampler_vk.h"
#include "renderer/backend/vulkan/workarounds_vk.h"

namespace ogre {

class SamplerLibraryVK final {
 public:
  ~SamplerLibraryVK();

  explicit SamplerLibraryVK(const std::weak_ptr<DeviceHolderVK>& device_holder);

  void ApplyWorkarounds(const WorkaroundsVK& workarounds);

 private:
  friend class ContextVK;

  std::weak_ptr<DeviceHolderVK> device_holder_;
  std::vector<std::pair<uint64_t, std::shared_ptr<const SamplerVK>>> samplers_;
  bool mips_disabled_workaround_ = false;

  raw_ptr<const SamplerVK> GetSampler(
      const SamplerDescriptor& descriptor);

  SamplerLibraryVK(const SamplerLibraryVK&) = delete;

  SamplerLibraryVK& operator=(const SamplerLibraryVK&) = delete;
};

}  // namespace ogre

#endif  // FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_SAMPLER_LIBRARY_VK_H_
