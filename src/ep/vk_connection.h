#pragma once

#include "vk.h"

namespace ep {

class VKConnection {
 public:
  VKConnection();

  ~VKConnection();

  VKConnection(const VKConnection&) = delete;

  VKConnection& operator=(const VKConnection&) = delete;
};

}  // namespace ep
