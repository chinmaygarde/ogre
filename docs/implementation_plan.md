# ONNX Runtime Vulkan Execution Provider — Implementation Plan

## 1. Project Overview

This document describes the architecture, build system, implementation phases, and testing strategy for a custom ONNX Runtime Execution Provider (EP) backed by Vulkan compute shaders authored in Shader Slang.

**Technology stack:**

| Layer | Choice |
|---|---|
| Compute API | Vulkan 1.3 via `vulkan.hpp` (C++ bindings) |
| Memory management | VMA (VulkanMemoryAllocator) with its C++ bindings (`vk_mem_alloc.hpp`) |
| Shader language | Shader Slang — used both at build-time (ahead-of-time SPIR-V) and at runtime (JIT specialization) |
| Package manager | vcpkg (manifest mode) |
| Language standard | C++17 |
| Build system | CMake ≥ 3.21 |

---

## 2. Repository Layout

```
onnxruntime-vulkan-ep/
├── CMakeLists.txt                   # Top-level CMake
├── CMakePresets.json                 # vcpkg integration presets
├── vcpkg.json                       # Manifest (dependencies)
├── vcpkg-configuration.json         # Registries (default + slang overlay)
├── cmake/
│   ├── FindOnnxRuntime.cmake         # locate onnxruntime headers/lib
│   ├── SlangCompile.cmake            # custom function: slang → SPIR-V
│   └── EmbedSPIRV.cmake             # custom function: .spv → C++ header
├── shaders/
│   ├── elementwise.slang
│   ├── gemm.slang
│   ├── conv2d.slang
│   ├── reduce.slang
│   ├── softmax.slang
│   └── ...
├── src/
│   ├── vulkan_ep/
│   │   ├── vulkan_execution_provider.hpp / .cpp
│   │   ├── vulkan_execution_provider_info.hpp / .cpp
│   │   ├── vulkan_allocator.hpp / .cpp          # OrtAllocator wrapping VMA
│   │   ├── vulkan_device.hpp / .cpp              # vk::Device + queues
│   │   ├── vulkan_pipeline_cache.hpp / .cpp      # compute pipeline mgmt
│   │   ├── vulkan_command_manager.hpp / .cpp      # cmd-buffer ring
│   │   ├── vulkan_data_transfer.hpp / .cpp        # host↔device staging
│   │   ├── vulkan_fence_pool.hpp / .cpp
│   │   └── slang_compiler.hpp / .cpp              # runtime Slang session
│   ├── kernels/
│   │   ├── kernel_registry.hpp / .cpp
│   │   ├── elementwise_kernel.hpp / .cpp
│   │   ├── gemm_kernel.hpp / .cpp
│   │   ├── conv2d_kernel.hpp / .cpp
│   │   ├── reduce_kernel.hpp / .cpp
│   │   ├── softmax_kernel.hpp / .cpp
│   │   └── ...
│   └── provider_bridge.cpp            # C API entry (RegisterVulkanEP)
├── tests/
│   ├── unit/
│   │   ├── test_vulkan_device.cpp
│   │   ├── test_vulkan_allocator.cpp
│   │   ├── test_pipeline_cache.cpp
│   │   ├── test_data_transfer.cpp
│   │   └── test_slang_compiler.cpp
│   ├── kernel/
│   │   ├── test_elementwise.cpp
│   │   ├── test_gemm.cpp
│   │   ├── test_conv2d.cpp
│   │   ├── test_reduce.cpp
│   │   └── test_softmax.cpp
│   ├── integration/
│   │   ├── test_ep_registration.cpp
│   │   ├── test_inference_mnist.cpp
│   │   ├── test_inference_resnet.cpp
│   │   └── test_mixed_ep_fallback.cpp
│   ├── models/                       # small ONNX test models
│   └── CMakeLists.txt
└── docs/
    └── architecture.md
```

---

## 3. Build System & Dependencies

### 3.1 vcpkg Manifest (`vcpkg.json`)

```jsonc
{
  "name": "onnxruntime-vulkan-ep",
  "version-semver": "0.1.0",
  "dependencies": [
    "vulkan",
    "vulkan-headers",
    "vulkan-memory-allocator",
    { "name": "slang", "version>=": "2025.9" },
    { "name": "gtest", "features": [], "platform": "!uwp" }
  ]
}
```

Slang comes from an overlay registry (a git-based vcpkg registry containing its portfile) because the default vcpkg registry does not ship Slang. Point to it in `vcpkg-configuration.json`:

```jsonc
{
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/<your-org>/vcpkg-slang-registry",
      "baseline": "<commit-hash>",
      "packages": ["slang"]
    }
  ],
  "default-registry": {
    "kind": "builtin",
    "baseline": "<vcpkg-baseline>"
  }
}
```

### 3.2 CMake Highlights

```cmake
cmake_minimum_required(VERSION 3.21)
project(onnxruntime_vulkan_ep LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)              # from vcpkg
find_package(VulkanMemoryAllocator REQUIRED)
find_package(slang CONFIG REQUIRED)
find_package(OnnxRuntime REQUIRED)         # FindOnnxRuntime.cmake
find_package(GTest CONFIG REQUIRED)

# --- Shader compilation (AOT) ---
include(cmake/SlangCompile.cmake)
slang_compile_directory(
    SOURCE_DIR  ${CMAKE_SOURCE_DIR}/shaders
    OUTPUT_DIR  ${CMAKE_BINARY_DIR}/spirv
    TARGET_ENV  vulkan_1_3
    EMBED_HEADER ${CMAKE_BINARY_DIR}/generated/embedded_shaders.hpp
)

add_library(vulkan_ep SHARED
    src/vulkan_ep/vulkan_execution_provider.cpp
    src/vulkan_ep/vulkan_execution_provider_info.cpp
    src/vulkan_ep/vulkan_allocator.cpp
    src/vulkan_ep/vulkan_device.cpp
    src/vulkan_ep/vulkan_pipeline_cache.cpp
    src/vulkan_ep/vulkan_command_manager.cpp
    src/vulkan_ep/vulkan_data_transfer.cpp
    src/vulkan_ep/vulkan_fence_pool.cpp
    src/vulkan_ep/slang_compiler.cpp
    src/kernels/kernel_registry.cpp
    src/kernels/elementwise_kernel.cpp
    src/kernels/gemm_kernel.cpp
    src/kernels/conv2d_kernel.cpp
    src/kernels/reduce_kernel.cpp
    src/kernels/softmax_kernel.cpp
    src/provider_bridge.cpp
)

target_link_libraries(vulkan_ep
    PRIVATE Vulkan::Vulkan
    PRIVATE VulkanMemoryAllocator::VulkanMemoryAllocator
    PRIVATE slang::slang
    PRIVATE onnxruntime::onnxruntime
)

target_include_directories(vulkan_ep
    PRIVATE ${CMAKE_BINARY_DIR}/generated   # embedded SPIR-V headers
)
```

### 3.3 Slang Compilation Function (`cmake/SlangCompile.cmake`)

```cmake
function(slang_compile_directory)
    cmake_parse_arguments(ARG "" "SOURCE_DIR;OUTPUT_DIR;TARGET_ENV;EMBED_HEADER" "" ${ARGN})
    file(GLOB SLANG_SOURCES "${ARG_SOURCE_DIR}/*.slang")

    set(SPV_OUTPUTS "")
    foreach(SRC ${SLANG_SOURCES})
        get_filename_component(NAME_WE ${SRC} NAME_WE)
        set(SPV "${ARG_OUTPUT_DIR}/${NAME_WE}.spv")
        add_custom_command(
            OUTPUT  ${SPV}
            COMMAND slangc ${SRC}
                    -target spirv
                    -profile sm_6_5
                    -emit-spirv-directly
                    -o ${SPV}
            DEPENDS ${SRC}
            COMMENT "Compiling ${NAME_WE}.slang → SPIR-V"
        )
        list(APPEND SPV_OUTPUTS ${SPV})
    endforeach()

    add_custom_target(compile_shaders ALL DEPENDS ${SPV_OUTPUTS})

    # embed_spirv is a small helper script that reads every .spv
    # and produces a C++ header with constexpr uint32_t arrays.
    add_custom_command(
        OUTPUT  ${ARG_EMBED_HEADER}
        COMMAND ${CMAKE_COMMAND}
                -DSPV_DIR=${ARG_OUTPUT_DIR}
                -DOUT=${ARG_EMBED_HEADER}
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedSPIRV.cmake
        DEPENDS ${SPV_OUTPUTS}
    )
    add_custom_target(embed_shaders ALL DEPENDS ${ARG_EMBED_HEADER})
endfunction()
```

---

## 4. Core Architecture

### 4.1 Component Diagram

```
┌───────────────────────────────────────────────────────────────┐
│  ONNX Runtime                                                 │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  InferenceSession                                        │ │
│  │    ├── Graph partitioner (selects nodes for Vulkan EP)   │ │
│  │    └── calls IExecutionProvider::Compile / Compute       │ │
│  └──────────────────────┬───────────────────────────────────┘ │
│                         │                                     │
│  ┌──────────────────────▼───────────────────────────────────┐ │
│  │  VulkanExecutionProvider  (implements IExecutionProvider) │ │
│  │    ├── GetCapability()   → claims supported op-sets      │ │
│  │    ├── Compile()         → fuses subgraph → VulkanKernel │ │
│  │    └── GetAllocator()    → returns VulkanAllocator       │ │
│  └────┬──────────┬──────────┬───────────────────────────────┘ │
│       │          │          │                                 │
│  ┌────▼───┐ ┌───▼────┐ ┌───▼─────────────┐                   │
│  │ Vulkan │ │Pipeline│ │ Command         │                   │
│  │ Device │ │ Cache  │ │ Manager         │                   │
│  └────┬───┘ └───┬────┘ └───┬─────────────┘                   │
│       │         │           │                                 │
│  ┌────▼─────────▼───────────▼──────────────┐                  │
│  │  Vulkan API (vulkan.hpp)                │                  │
│  │  ├── vk::Device, vk::Queue             │                  │
│  │  ├── vk::Pipeline (compute)             │                  │
│  │  └── VMA (device memory)                │                  │
│  └─────────────────────────────────────────┘                  │
└───────────────────────────────────────────────────────────────┘
```

### 4.2 Key Classes

#### `VulkanDevice`

Owns the Vulkan instance, physical device selection, logical device, and queue handles. Selects a compute-capable queue family. Exposes device limits so kernels can query `maxComputeWorkGroupSize`, shared memory size, etc.

```cpp
class VulkanDevice {
public:
    explicit VulkanDevice(const VulkanEPOptions& opts);
    ~VulkanDevice();

    [[nodiscard]] vk::Device         device()        const noexcept;
    [[nodiscard]] vk::PhysicalDevice physicalDevice() const noexcept;
    [[nodiscard]] vk::Queue          computeQueue()  const noexcept;
    [[nodiscard]] std::uint32_t      queueFamily()   const noexcept;
    [[nodiscard]] const vk::PhysicalDeviceLimits& limits() const noexcept;

private:
    vk::UniqueInstance       instance_;
    vk::PhysicalDevice       physical_device_;
    vk::UniqueDevice         device_;
    std::uint32_t            compute_queue_family_{};
    vk::Queue                compute_queue_;
};
```

#### `VulkanAllocator`

Wraps VMA behind the `OrtAllocator` interface so ONNX Runtime can allocate tensors on the GPU transparently. Implements a staging-buffer strategy for host-visible memory used during transfers.

```cpp
class VulkanAllocator {
public:
    VulkanAllocator(VulkanDevice& device, const VulkanEPOptions& opts);
    ~VulkanAllocator();

    // Device-local allocation (for tensor storage)
    [[nodiscard]] VmaAllocationInfo allocateDevice(
        vk::DeviceSize size, vk::Buffer& out_buffer);

    // Host-visible staging allocation (for upload/download)
    [[nodiscard]] VmaAllocationInfo allocateStaging(
        vk::DeviceSize size, vk::Buffer& out_buffer);

    void free(vk::Buffer buffer, VmaAllocation allocation);

    [[nodiscard]] vma::Allocator handle() const noexcept;

private:
    vma::UniqueAllocator allocator_;
};
```

#### `VulkanPipelineCache`

Caches `vk::Pipeline` objects keyed by `(shader_hash, specialization_constants)`. This avoids redundant pipeline creation when the same operator is invoked with identical shapes. Optionally serializes the Vulkan pipeline cache blob to disk.

```cpp
class VulkanPipelineCache {
public:
    struct PipelineKey {
        std::size_t shader_hash;
        std::vector<std::uint32_t> spec_constants;
        bool operator==(const PipelineKey&) const noexcept;
    };

    VulkanPipelineCache(VulkanDevice& device);

    [[nodiscard]] vk::Pipeline getOrCreate(
        const PipelineKey& key,
        std::span<const std::uint32_t> spirv_code,
        vk::PipelineLayout layout);

    void saveToDisk(const std::filesystem::path& path) const;
    void loadFromDisk(const std::filesystem::path& path);

private:
    VulkanDevice& device_;
    vk::UniquePipelineCache vk_cache_;
    std::unordered_map<PipelineKey, vk::UniquePipeline, PipelineKeyHash> map_;
};
```

#### `VulkanCommandManager`

Manages a ring of command buffers and fences for overlapping dispatch and data transfer. Each "frame in flight" gets its own command buffer; the ring size is configurable (default 2).

```cpp
class VulkanCommandManager {
public:
    VulkanCommandManager(VulkanDevice& device, std::uint32_t ring_size = 2);

    struct Frame {
        vk::CommandBuffer cmd;
        vk::Fence         fence;
    };

    // Wait for the oldest frame's fence, reset, begin recording.
    [[nodiscard]] Frame beginFrame();

    // End recording, submit to compute queue.
    void submitFrame(Frame& frame);

    // Blocking: wait for ALL in-flight work.
    void waitAll();

private:
    VulkanDevice& device_;
    vk::UniqueCommandPool pool_;
    std::vector<vk::UniqueCommandBuffer> buffers_;
    std::vector<vk::UniqueFence> fences_;
    std::uint32_t ring_index_{};
    std::uint32_t ring_size_;
};
```

#### `SlangCompiler`

Wraps a Slang session for runtime shader compilation. Used when a kernel needs to specialize on tensor shapes or types that were not known at build time.

```cpp
class SlangCompiler {
public:
    SlangCompiler();
    ~SlangCompiler();

    // Compile a .slang source string into SPIR-V.
    // `defines` lets you inject #define-style specializations.
    [[nodiscard]] std::vector<std::uint32_t> compile(
        std::string_view source,
        std::string_view entry_point,
        const std::unordered_map<std::string, std::string>& defines = {});

private:
    SlangSession* session_{};
};
```

#### `VulkanDataTransfer`

Handles host→device and device→host copies through staging buffers. Batches small transfers together when possible.

#### `VulkanExecutionProvider`

The top-level class that implements `onnxruntime::IExecutionProvider`. It owns all the above components and delegates kernel dispatch.

---

## 5. ONNX Runtime Integration Points

### 5.1 Provider Registration (`provider_bridge.cpp`)

ONNX Runtime discovers custom EPs through a shared library that exports a registration function.

```cpp
#include <onnxruntime_c_api.h>

extern "C" ORT_API_STATUS(OrtSessionOptionsAppendExecutionProvider_Vulkan)
    (OrtSessionOptions* options, int device_id) {
    auto provider = std::make_unique<VulkanExecutionProvider>(
        VulkanEPOptions{.device_id = device_id});
    // Use the ORT C API to append the EP.
    return OrtApis::SessionOptionsAppendExecutionProvider(
        options, provider.release());
}
```

### 5.2 `GetCapability()`

Returns a list of `IndexedSubGraph` entries describing which ONNX nodes this EP can execute. The implementation walks the graph and claims nodes whose op-type appears in the kernel registry.

Strategy: start conservatively — claim only operators that have a tested kernel. Unclaimed nodes fall back to the CPU EP automatically.

### 5.3 `Compile()` and `Compute()`

`Compile()` receives a fused subgraph and creates a `VulkanComputeFunction` that binds the appropriate shader pipeline, descriptor sets, and push constants for each node.

`Compute()` is the hot path. It records GPU commands for each node in the fused function, inserts barrier synchronization between dependent dispatches, and submits to the compute queue.

### 5.4 Memory Planner Integration

The EP's allocator is registered with ORT's memory planner. ORT may call `Alloc` / `Free` for intermediate tensors. The `VulkanAllocator` satisfies these with device-local VMA allocations and uses a simple free-list to recycle recently freed blocks.

---

## 6. Shader Strategy (Slang)

### 6.1 Ahead-of-Time (AOT) Shaders

Most operator shaders are compiled at CMake build time into SPIR-V and embedded in the binary as `constexpr` arrays. This avoids any runtime compilation latency for common operators.

Each shader is parameterized via specialization constants:

```slang
// elementwise.slang
[[vk::constant_id(0)]] const uint OP_CODE = 0; // 0=Add, 1=Mul, ...
[[vk::constant_id(1)]] const uint VECTOR_WIDTH = 4;

[[vk::binding(0, 0)]] RWStructuredBuffer<float> inputA;
[[vk::binding(1, 0)]] RWStructuredBuffer<float> inputB;
[[vk::binding(2, 0)]] RWStructuredBuffer<float> output;

struct PushConstants {
    uint element_count;
};
[[vk::push_constant]] PushConstants pc;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint idx = tid.x * VECTOR_WIDTH;
    if (idx >= pc.element_count) return;

    for (uint i = 0; i < VECTOR_WIDTH && (idx + i) < pc.element_count; ++i) {
        float a = inputA[idx + i];
        float b = inputB[idx + i];
        switch (OP_CODE) {
            case 0: output[idx + i] = a + b; break;
            case 1: output[idx + i] = a * b; break;
            // ...
        }
    }
}
```

### 6.2 Runtime (JIT) Compilation

For operators whose optimal implementation depends on runtime-known shapes (e.g., tiled GEMM where tile sizes should match the tensor dimensions), the `SlangCompiler` generates SPIR-V on the fly and feeds it to the `VulkanPipelineCache`.

JIT results are cached aggressively — once a `(shader_source_hash, defines)` pair produces a pipeline, it is never recompiled within the session.

### 6.3 Shader Roadmap

| Phase | Operators | Notes |
|---|---|---|
| P0 | Add, Sub, Mul, Div, Relu, Sigmoid, Tanh | Element-wise; single shader with spec-const switch |
| P0 | MatMul / Gemm | Tiled shared-memory implementation |
| P1 | Conv2d (im2col + GEMM) | Two-pass approach initially |
| P1 | MaxPool, AveragePool, GlobalAveragePool | |
| P1 | Softmax, LogSoftmax | Parallel reduction |
| P1 | Reshape, Transpose, Concat, Flatten | Metadata-only or lightweight copy kernels |
| P2 | BatchNormalization, InstanceNorm, LayerNorm | |
| P2 | Reduce{Mean,Sum,Max,Min} | General parallel reduction |
| P2 | Gather, Scatter, Slice, Pad | Index-manipulation kernels |
| P3 | Attention / fused MHSA | For transformer support |
| P3 | Resize (nearest, bilinear) | |
| P3 | Quantized variants (int8/uint8) | Requires Vulkan int8 storage capability |

---

## 7. Implementation Phases

### Phase 1 — Skeleton (Weeks 1–3)

**Goal:** A shared library that registers with ORT, claims zero operators, and passes the integration test that loads a model and falls back entirely to CPU.

Tasks:

1. Set up the repo structure, vcpkg manifest, CMakePresets.json.
2. Write the `FindOnnxRuntime.cmake` module.
3. Implement `VulkanDevice` — instance creation, physical device selection, queue creation.
4. Implement `VulkanAllocator` — basic VMA setup, device-local and staging allocations.
5. Implement `VulkanExecutionProvider` skeleton — `GetCapability()` returns empty, `Compile()` is a no-op.
6. Implement `provider_bridge.cpp` — export the registration function.
7. Write unit tests for `VulkanDevice` and `VulkanAllocator`.
8. Write an integration test: load `mnist.onnx`, register Vulkan EP (claims nothing), run inference on CPU EP fallback, verify outputs.

### Phase 2 — Element-wise Operators (Weeks 4–6)

**Goal:** Element-wise arithmetic and activations run on the GPU.

Tasks:

1. Implement `VulkanCommandManager` — command buffer ring, fence management.
2. Implement `VulkanDataTransfer` — host→device upload, device→host download.
3. Build `SlangCompile.cmake` and `EmbedSPIRV.cmake`, compile `elementwise.slang`.
4. Implement `VulkanPipelineCache`.
5. Implement `ElementwiseKernel` (Add, Sub, Mul, Div, Relu, Sigmoid, Tanh).
6. Implement `KernelRegistry` and wire it to `GetCapability()`.
7. Wire `Compile()` and `Compute()` for single-node subgraphs.
8. Kernel tests: compare output of each element-wise op against ORT's CPU EP on random tensors with tolerance ≤ 1e-5.
9. Integration test: run a simple ONNX model containing Add + Relu end-to-end on the Vulkan EP.

### Phase 3 — GEMM & Convolution (Weeks 7–10)

**Goal:** MatMul/Gemm and Conv2d execute on Vulkan, enabling CNN inference.

Tasks:

1. Author `gemm.slang` — tiled matrix multiply with shared memory, configurable tile sizes via specialization constants.
2. Implement `GemmKernel` — descriptor set layout for A, B, C buffers; push constants for M, N, K, alpha, beta.
3. Author `conv2d.slang` — im2col in a first pass, then invoke GEMM.
4. Implement `Conv2dKernel` — handles padding, strides, dilations, groups.
5. Implement `SlangCompiler` for runtime JIT (tile-size specialization for GEMM).
6. Add pooling kernels (MaxPool, AveragePool, GlobalAveragePool).
7. Add shape-manipulation ops (Reshape, Transpose, Concat, Flatten) — most are zero-copy metadata changes.
8. Kernel tests for each new operator.
9. Integration test: run MNIST CNN end-to-end on Vulkan EP; verify accuracy within tolerance of CPU EP.

### Phase 4 — Normalization, Reduction, Softmax (Weeks 11–13)

**Goal:** Support batch-norm, layer-norm, softmax, and general reductions.

Tasks:

1. Author `reduce.slang` — work-group parallel reduction tree with configurable reduction op (sum, max, mean).
2. Author `softmax.slang` — two-pass (max-reduce, exp-and-sum-reduce).
3. Implement BatchNorm, InstanceNorm, LayerNorm kernels.
4. Implement ReduceMean, ReduceSum, ReduceMax, ReduceMin.
5. Implement Gather, Scatter, Slice, Pad.
6. Kernel and integration tests.
7. Integration test: run ResNet-18 / MobileNetV2 end-to-end.

### Phase 5 — Performance & Polish (Weeks 14–16)

**Goal:** Competitive performance, operator fusion, diagnostics.

Tasks:

1. Multi-node subgraph fusion in `Compile()` — chain element-wise ops into a single dispatch.
2. Pipeline cache serialization to disk.
3. Vulkan timestamp queries for per-kernel profiling; expose via ORT's profiling API.
4. Memory reuse strategy — suballocate a large device-local buffer for intermediate tensors.
5. Configurable options struct (`VulkanEPOptions`) — device selection, ring size, pipeline cache path, JIT enable/disable.
6. Benchmark suite: latency and throughput on MNIST, ResNet-18, MobileNetV2.
7. Documentation (public API, build instructions, supported operators table).

---

## 8. Testing Strategy

### 8.1 Unit Tests (Google Test)

Each infrastructure class gets its own test file. Tests run against a real Vulkan device (they skip gracefully if no GPU is present via `GTEST_SKIP()`).

| Test file | What it verifies |
|---|---|
| `test_vulkan_device.cpp` | Instance creation, queue family selection, device properties |
| `test_vulkan_allocator.cpp` | Device-local alloc/free, staging alloc/free, OOM handling |
| `test_pipeline_cache.cpp` | Pipeline creation from embedded SPIR-V, cache hit on second call, specialization constants |
| `test_data_transfer.cpp` | Round-trip host→device→host for buffers of various sizes (0, 1, 1M, 256M) |
| `test_slang_compiler.cpp` | Compile a trivial shader, verify SPIR-V magic number, compile with defines |

### 8.2 Kernel Tests (Google Test)

Each operator kernel is tested against the CPU reference implementation:

```cpp
// test_elementwise.cpp — representative pattern
TEST(ElementwiseKernel, AddFloat32) {
    // 1. Create random input tensors (host side)
    auto a = makeRandomTensor<float>({128, 256});
    auto b = makeRandomTensor<float>({128, 256});

    // 2. Run on Vulkan EP
    auto vulkan_result = runVulkanKernel<ElementwiseKernel>(
        "Add", {a, b});

    // 3. Run on CPU (reference)
    auto cpu_result = runCpuReference("Add", {a, b});

    // 4. Compare
    EXPECT_TENSORS_NEAR(vulkan_result, cpu_result, /*atol=*/1e-5f);
}
```

Shape variations tested: scalar, 1-D, 2-D, high-rank, broadcast, zero-element, non-contiguous (via Transpose).

### 8.3 Integration Tests (Google Test + ORT C++ API)

```cpp
// test_inference_mnist.cpp — representative pattern
TEST(Integration, MnistEndToEnd) {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "test"};
    Ort::SessionOptions opts;
    OrtSessionOptionsAppendExecutionProvider_Vulkan(opts, /*device_id=*/0);

    Ort::Session session{env, "models/mnist.onnx", opts};

    auto input = loadTestImage("models/mnist_test_7.png");
    auto output = runSession(session, input);

    EXPECT_EQ(argmax(output), 7);
}
```

Integration tests also cover:

- **Fallback correctness:** Model with ops partially on Vulkan, rest on CPU; verify final output matches pure-CPU run.
- **Multiple inferences:** Run the same model 100 times; results are deterministic.
- **Session teardown:** No Vulkan validation-layer errors on session destruction.

### 8.4 Validation Layer Enforcement

All tests link against the Vulkan validation layers (`VK_LAYER_KHRONOS_validation`). A custom debug-utils messenger is installed that converts any Vulkan validation error into a Google Test `FAIL()`. This means any incorrect Vulkan API usage is caught as a test failure automatically.

```cpp
// In test fixture SetUp():
auto messenger_info = vk::DebugUtilsMessengerCreateInfoEXT{}
    .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
    .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
    .setPfnUserCallback([](
            VkDebugUtilsMessageSeverityFlagBitsEXT,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*) -> VkBool32 {
        ADD_FAILURE() << "Vulkan validation: " << data->pMessage;
        return VK_FALSE;
    });
```

### 8.5 Benchmark Tests (Separate Target)

Not run in CI by default; invoked manually or on a schedule.

```cpp
// benchmark_resnet.cpp (Google Benchmark or simple timing loop)
void BM_ResNet18_Vulkan(benchmark::State& state) {
    // Setup session once
    for (auto _ : state) {
        runSession(session, input);
    }
    state.SetItemsProcessed(state.iterations());
}
```

Compared against: CPU EP (same machine), CUDA EP (if NVIDIA GPU), DirectML EP (if Windows). Results logged as JSON for regression tracking.

### 8.6 CI Pipeline Outline

```
┌──────────────┐     ┌───────────────────┐     ┌──────────────────┐
│ Build (Linux) │────▶│ Unit + Kernel Tests│────▶│ Integration Tests │
│  + Windows    │     │   (swiftshader     │     │  (real GPU or     │
│               │     │    software driver)│     │   swiftshader)    │
└──────────────┘     └───────────────────┘     └──────────────────┘
```

- **Software Vulkan in CI:** Use SwiftShader (or lavapipe) as a Vulkan ICD for headless CI runners that lack a physical GPU. This covers correctness; performance tests run on dedicated GPU machines.
- **Validation layers** are always enabled in CI.

---

## 9. Key Design Decisions & Rationale

**Why vulkan.hpp over raw C Vulkan?** RAII via `vk::Unique*` handles prevent resource leaks. The type-safe enum wrappers catch misuse at compile time. Zero runtime overhead — it's a header-only wrapper.

**Why VMA?** Writing a production-quality Vulkan memory allocator is a multi-month project in itself. VMA handles memory-type selection, suballocation, defragmentation, and dedicated allocations for large buffers. The C++ bindings (`vk_mem_alloc.hpp`) mesh well with `vulkan.hpp`.

**Why Shader Slang over raw GLSL/HLSL?** Slang provides generics, interfaces, and modules — enabling a single `elementwise.slang` that handles all element-wise ops through specialization rather than N copy-pasted GLSL files. Its SPIR-V backend emits code competitive with glslangValidator. It also supports runtime compilation through its C API, needed for JIT.

**Why both AOT and JIT shaders?** AOT covers the common case (instant pipeline creation, no startup penalty). JIT covers shape-dependent optimization (e.g., choosing GEMM tile sizes that divide evenly into the matrix dimensions).

**Why a command-buffer ring?** Overlapping dispatch with data transfer. While frame N's compute is executing, frame N+1's uploads are happening on the same queue via a barrier-based dependency chain.

---

## 10. Risks & Mitigations

| Risk | Likelihood | Mitigation |
|---|---|---|
| Vulkan driver bugs on specific GPUs | Medium | Test on NVIDIA, AMD, Intel, and SwiftShader; use validation layers; maintain a known-issue table |
| Floating-point divergence from CPU EP | High for edge cases | Use tolerant comparisons (atol + rtol); document known precision differences |
| Slang compiler regressions | Low-Medium | Pin to a specific Slang version in vcpkg; run shader compilation in CI |
| ORT API changes across versions | Medium | Pin to a specific ORT release; abstract the EP interface behind a thin adapter layer |
| Performance worse than CPU for small tensors | High | Fall back to CPU for tensors below a configurable size threshold in `GetCapability()` |

---

## 11. Future Extensions

- **Vulkan subgroups / cooperative matrices** for GEMM on GPUs that support `VK_KHR_cooperative_matrix`.
- **Multi-queue** — use a separate transfer queue for host↔device copies, overlapping with compute.
- **Graph-level optimization** — fuse entire subgraphs into a single mega-kernel via Slang module composition.
- **FP16 / BF16 compute** — leverage `VK_KHR_shader_float16_int8` for mixed-precision inference.
- **Android support** — Vulkan is the primary GPU API on Android; this EP could enable mobile deployment.
