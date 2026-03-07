# Contributing

## Prerequisites

- CMake 3.22+
- Ninja
- A C++20-capable compiler
- [vcpkg](https://vcpkg.io/) with `VCPKG_ROOT` set in your environment
- [just](https://just.systems/) (task runner)
- Vulkan SDK

## Getting Started

Clone and initialize submodules:

```sh
git clone <repo>
cd onnxsandbox
just sync
```

## Building

```sh
just setup        # configure (debug)
just build        # build (debug)
just test         # run tests (debug)
```

Use the `preset` argument to switch configurations:

```sh
just build release
just test release
```

To see all available presets:

```sh
just list-builds
```

To clean the build directory:

```sh
just clean
```

## Docker / Dev Container

A dev container configuration is provided for reproducible environments:

```sh
just docker_dev
```

## Code Style

This project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

## Dependencies

Dependencies are managed via vcpkg and declared in `vcpkg.json`. To add a dependency, add it there and re-run `just setup`.
