#pragma once

#define VK_NO_PROTOTYPES
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NAMESPACE ep::vk
#include <vulkan/vulkan.hpp>

// The Vulkan headers may bring in X11 headers which define some macros that
// conflict with other code.  Undefine these macros after including Vulkan.
#undef Bool
#undef None
#undef Status
#undef Success

namespace ep {}
