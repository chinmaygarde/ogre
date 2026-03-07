// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocation_size.h"
#include "core/device_buffer.h"
#include "core/device_buffer_descriptor.h"
#include "core/formats.h"
#include "flutter/testing/testing.h"  // IWYU pragma: keep
#include "gtest/gtest.h"
#include "renderer/backend/vulkan/allocator_vk.h"
#include "renderer/backend/vulkan/device_buffer_vk.h"
#include "renderer/backend/vulkan/test/mock_vulkan.h"
#include "vulkan/vulkan_enums.hpp"

namespace ogre {
namespace testing {

TEST(AllocatorTest, ToVKImageUsageFlags) {
  EXPECT_EQ(Allocator::ToVKImageUsageFlags(
                PixelFormat::kR8G8B8A8UNormInt,
                static_cast<TextureUsageMask>(TextureUsage::kRenderTarget),
                StorageMode::kDeviceTransient,
                /*supports_memoryless_textures=*/true),
            vk::ImageUsageFlagBits::eInputAttachment |
                vk::ImageUsageFlagBits::eColorAttachment |
                vk::ImageUsageFlagBits::eTransientAttachment);

  EXPECT_EQ(Allocator::ToVKImageUsageFlags(
                PixelFormat::kD24UnormS8Uint,
                static_cast<TextureUsageMask>(TextureUsage::kRenderTarget),
                StorageMode::kDeviceTransient,
                /*supports_memoryless_textures=*/true),
            vk::ImageUsageFlagBits::eDepthStencilAttachment |
                vk::ImageUsageFlagBits::eTransientAttachment);
}

TEST(AllocatorTest, MemoryTypeSelectionSingleHeap) {
  vk::PhysicalDeviceMemoryProperties properties;
  properties.memoryTypeCount = 1;
  properties.memoryHeapCount = 1;
  properties.memoryTypes[0].heapIndex = 0;
  properties.memoryTypes[0].propertyFlags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  properties.memoryHeaps[0].size = 1024 * 1024 * 1024;
  properties.memoryHeaps[0].flags = vk::MemoryHeapFlagBits::eDeviceLocal;

  EXPECT_EQ(Allocator::FindMemoryTypeIndex(1, properties), 0);
  EXPECT_EQ(Allocator::FindMemoryTypeIndex(2, properties), -1);
  EXPECT_EQ(Allocator::FindMemoryTypeIndex(3, properties), 0);
}

TEST(AllocatorTest, MemoryTypeSelectionTwoHeap) {
  vk::PhysicalDeviceMemoryProperties properties;
  properties.memoryTypeCount = 2;
  properties.memoryHeapCount = 2;
  properties.memoryTypes[0].heapIndex = 0;
  properties.memoryTypes[0].propertyFlags =
      vk::MemoryPropertyFlagBits::eHostVisible;
  properties.memoryHeaps[0].size = 1024 * 1024 * 1024;
  properties.memoryHeaps[0].flags = vk::MemoryHeapFlagBits::eDeviceLocal;

  properties.memoryTypes[1].heapIndex = 1;
  properties.memoryTypes[1].propertyFlags =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  properties.memoryHeaps[1].size = 1024 * 1024 * 1024;
  properties.memoryHeaps[1].flags = vk::MemoryHeapFlagBits::eDeviceLocal;

  // First fails because this only looks for eDeviceLocal.
  EXPECT_EQ(Allocator::FindMemoryTypeIndex(1, properties), -1);
  EXPECT_EQ(Allocator::FindMemoryTypeIndex(2, properties), 1);
  EXPECT_EQ(Allocator::FindMemoryTypeIndex(3, properties), 1);
  EXPECT_EQ(Allocator::FindMemoryTypeIndex(4, properties), -1);
}

TEST(AllocatorTest, ImageResourceKeepsVulkanDeviceAlive) {
  std::shared_ptr<Texture> texture;
  std::weak_ptr<Allocator> weak_allocator;
  {
    auto const context = MockVulkanContextBuilder().Build();
    weak_allocator = context->GetResourceAllocator();
    auto allocator = context->GetResourceAllocator();

    texture = allocator->CreateTexture(TextureDescriptor{
        .storage_mode = StorageMode::kDevicePrivate,
        .format = PixelFormat::kR8G8B8A8UNormInt,
        .size = {1, 1},
    });
    context->Shutdown();
  }

  ASSERT_TRUE(weak_allocator.lock());
}

#ifdef OGRE_DEBUG

TEST(AllocatorTest, RecreateSwapchainWhenSizeChanges) {
  auto const context = MockVulkanContextBuilder().Build();
  auto allocator = context->GetResourceAllocator();

  EXPECT_EQ(reinterpret_cast<Allocator*>(allocator.get())
                ->DebugGetHeapUsage()
                .GetByteSize(),
            0u);

  allocator->CreateBuffer(DeviceBufferDescriptor{
      .storage_mode = StorageMode::kDevicePrivate,
      .size = 1024,
  });

  // Usage increases beyond the size of the allocated buffer since VMA will
  // first allocate large blocks of memory and then suballocate small memory
  // allocations.
  EXPECT_EQ(reinterpret_cast<Allocator*>(allocator.get())
                ->DebugGetHeapUsage()
                .ConvertTo<MebiBytes>()
                .GetSize(),
            16u);
}

#endif  // OGRE_DEBUG

}  // namespace testing
}  // namespace ogre
