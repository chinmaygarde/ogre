// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_BLIT_PASS_VK_H_
#define FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_BLIT_PASS_VK_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/config.h"
#include "core/buffer_view.h"
#include "core/device_buffer.h"
#include "core/texture.h"
#include "fml/macros.h"
#include "geometry/point.h"
#include "geometry/rect.h"
#include "renderer/backend/vulkan/workarounds_vk.h"

namespace ogre {

class CommandEncoderVK;
class CommandBufferVK;

class BlitPassVK final {
 public:
  ~BlitPassVK();

  bool IsValid() const;

  void SetLabel(std::string_view label);

  bool ConvertTextureToShaderRead(const std::shared_ptr<Texture>& texture);

  bool ResizeTexture(const std::shared_ptr<Texture>& source,
                     const std::shared_ptr<Texture>& destination);

  bool AddCopy(std::shared_ptr<Texture> source,
               std::shared_ptr<Texture> destination,
               std::optional<IRect> source_region = std::nullopt,
               IPoint destination_origin = {},
               std::string_view label = "");

  bool AddCopy(std::shared_ptr<Texture> source,
               std::shared_ptr<DeviceBuffer> destination,
               std::optional<IRect> source_region = std::nullopt,
               size_t destination_offset = 0,
               std::string_view label = "");

  bool AddCopy(BufferView source,
               std::shared_ptr<Texture> destination,
               std::optional<IRect> destination_region = std::nullopt,
               std::string_view label = "",
               uint32_t mip_level = 0,
               uint32_t slice = 0,
               bool convert_to_read = true);

  bool GenerateMipmap(std::shared_ptr<Texture> texture,
                      std::string_view label = "");

  bool EncodeCommands() const;

 private:
  friend class CommandBufferVK;
  FML_FRIEND_TEST(BlitPassVKTest,
                  MipmapGenerationTransitionsAllLevelsCorrectly);

  std::shared_ptr<CommandBufferVK> command_buffer_;
  const WorkaroundsVK workarounds_;

  explicit BlitPassVK(std::shared_ptr<CommandBufferVK> command_buffer,
                      const WorkaroundsVK& workarounds);

  void OnSetLabel(std::string_view label);

  bool OnCopyTextureToTextureCommand(std::shared_ptr<Texture> source,
                                     std::shared_ptr<Texture> destination,
                                     IRect source_region,
                                     IPoint destination_origin,
                                     std::string_view label);

  bool OnCopyTextureToBufferCommand(std::shared_ptr<Texture> source,
                                    std::shared_ptr<DeviceBuffer> destination,
                                    IRect source_region,
                                    size_t destination_offset,
                                    std::string_view label);

  bool OnCopyBufferToTextureCommand(BufferView source,
                                    std::shared_ptr<Texture> destination,
                                    IRect destination_region,
                                    std::string_view label,
                                    uint32_t mip_level,
                                    uint32_t slice,
                                    bool convert_to_read);

  bool OnGenerateMipmapCommand(std::shared_ptr<Texture> texture,
                               std::string_view label);

  BlitPassVK(const BlitPassVK&) = delete;

  BlitPassVK& operator=(const BlitPassVK&) = delete;
};

}  // namespace ogre

#endif  // FLUTTER_OGRE_RENDERER_BACKEND_VULKAN_BLIT_PASS_VK_H_
