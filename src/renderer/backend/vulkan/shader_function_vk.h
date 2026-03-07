// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_SHADER_FUNCTION_VK_H_
#define FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_SHADER_FUNCTION_VK_H_

#include <string>

#include "base/comparable.h"
#include "core/shader_types.h"
#include "renderer/backend/vulkan/device_holder_vk.h"
#include "renderer/backend/vulkan/vk.h"

namespace ogre {

class ShaderFunctionVK final : public Comparable<ShaderFunctionVK> {
 public:
  ~ShaderFunctionVK();

  ShaderStage GetStage() const;

  const std::string& GetName() const;

  const vk::ShaderModule& GetModule() const;

  // |Comparable<ShaderFunctionVK>|
  std::size_t GetHash() const override;

  // |Comparable<ShaderFunctionVK>|
  bool IsEqual(const ShaderFunctionVK& other) const override;

 private:
  friend class ShaderLibraryVK;

  UniqueID parent_library_id_;
  std::string name_;
  ShaderStage stage_;
  vk::UniqueShaderModule module_;
  std::weak_ptr<DeviceHolderVK> device_holder_;

  ShaderFunctionVK(const std::weak_ptr<DeviceHolderVK>& device_holder,
                   UniqueID parent_library_id,
                   std::string name,
                   ShaderStage stage,
                   vk::UniqueShaderModule module);

  ShaderFunctionVK(const ShaderFunctionVK&) = delete;

  ShaderFunctionVK& operator=(const ShaderFunctionVK&) = delete;
};

}  // namespace ogre

#endif  // FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_SHADER_FUNCTION_VK_H_
