// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/formats.h"

#include <sstream>

#include <absl/log/log.h>

namespace ogre {

std::string TextureUsageMaskToString(TextureUsageMask mask) {
  std::vector<TextureUsage> usages;
  if (mask & TextureUsage::kShaderRead) {
    usages.push_back(TextureUsage::kShaderRead);
  }
  if (mask & TextureUsage::kShaderWrite) {
    usages.push_back(TextureUsage::kShaderWrite);
  }
  if (mask & TextureUsage::kRenderTarget) {
    usages.push_back(TextureUsage::kRenderTarget);
  }
  std::stringstream stream;
  stream << "{ ";
  for (size_t i = 0; i < usages.size(); i++) {
    stream << TextureUsageToString(usages[i]);
    if (i != usages.size() - 1u) {
      stream << ", ";
    }
  }
  stream << " }";
  return stream.str();
}

}  // namespace ogre
