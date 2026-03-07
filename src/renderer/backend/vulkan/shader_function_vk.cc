// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/backend/vulkan/shader_function_vk.h"

#include "fml/hash_combine.h"

namespace ogre {

ShaderFunctionVK::ShaderFunctionVK(
    const std::weak_ptr<DeviceHolderVK>& device_holder,
    UniqueID parent_library_id,
    std::string name,
    ShaderStage stage,
    vk::UniqueShaderModule module)
    : parent_library_id_(parent_library_id),
      name_(std::move(name)),
      stage_(stage),
      module_(std::move(module)),
      device_holder_(device_holder) {}

ShaderFunctionVK::~ShaderFunctionVK() {
  std::shared_ptr<DeviceHolderVK> device_holder = device_holder_.lock();
  if (device_holder) {
    module_.reset();
  } else {
    module_.release();
  }
}

ShaderStage ShaderFunctionVK::GetStage() const {
  return stage_;
}

const std::string& ShaderFunctionVK::GetName() const {
  return name_;
}

const vk::ShaderModule& ShaderFunctionVK::GetModule() const {
  return module_.get();
}

std::size_t ShaderFunctionVK::GetHash() const {
  return fml::HashCombine(parent_library_id_, name_, stage_);
}

bool ShaderFunctionVK::IsEqual(const ShaderFunctionVK& other) const {
  return parent_library_id_ == other.parent_library_id_ &&
         name_ == other.name_ && stage_ == other.stage_;
}

}  // namespace ogre
