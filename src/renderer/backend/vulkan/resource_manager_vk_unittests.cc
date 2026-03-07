// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <functional>
#include <memory>
#include <utility>
#include "fml/closure.h"
#include "fml/synchronization/waitable_event.h"
#include "gtest/gtest.h"
#include "renderer/backend/vulkan/resource_manager_vk.h"

namespace ogre {
namespace testing {

// While expected to be a singleton per context, the class does not enforce it.
TEST(ResourceManagerTest, CreatesANewInstance) {
  auto const a = ResourceManager::Create();
  auto const b = ResourceManager::Create();
  EXPECT_NE(a, b);
}

TEST(ResourceManagerTest, ReclaimMovesAResourceAndDestroysIt) {
  auto const manager = ResourceManager::Create();

  auto waiter = fml::AutoResetWaitableEvent();
  auto dead = false;
  auto rattle = fml::ScopedCleanupClosure([&waiter]() { waiter.Signal(); });

  // Not killed immediately.
  EXPECT_FALSE(waiter.IsSignaledForTest());

  {
    auto resource = UniqueResourceVKT<fml::ScopedCleanupClosure>(
        manager, std::move(rattle));
  }

  waiter.Wait();
}

// Regression test for https://github.com/flutter/flutter/issues/134482.
TEST(ResourceManagerTest, TerminatesWhenOutOfScope) {
  // Originally, this shared_ptr was never destroyed, and the thread never
  // terminated. This test ensures that the thread terminates when the
  // ResourceManager is out of scope.
  std::weak_ptr<ResourceManager> manager;

  {
    auto shared = ResourceManager::Create();
    manager = shared;
  }

  // The thread should have terminated.
  EXPECT_EQ(manager.lock(), nullptr);
}

TEST(ResourceManagerTest, IsThreadSafe) {
  // In a typical app, there is a single ResourceManager per app, shared b/w
  // threads.
  //
  // This test ensures that the ResourceManager is thread-safe.
  std::weak_ptr<ResourceManager> manager;

  {
    auto const manager = ResourceManager::Create();

    // Spawn two threads, and have them both put resources into the manager.
    struct MockResource {};

    std::thread thread1([&manager]() {
      UniqueResourceVKT<MockResource>(manager, MockResource{});
    });

    std::thread thread2([&manager]() {
      UniqueResourceVKT<MockResource>(manager, MockResource{});
    });

    thread1.join();
    thread2.join();
  }

  // The thread should have terminated.
  EXPECT_EQ(manager.lock(), nullptr);
}

}  // namespace testing
}  // namespace ogre
