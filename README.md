# Basis

Basis is a new robotics framework focused on sane defaults and maintainability as a robotics codebase scales. It's highly recommended to go through the docs/ folder if you want more details.

## Planned Features
- Deterministic replay and simulation
    - This is key to enabling reliable testing, especially in CI where resources may be constrained.
    - Additionally - simple integration tests shouldn't require external networking or coordination.
- Compatibility with a wide variety of serialization formats 
    - You pick the message format that fits your needs. This enables:
        - converting your message serializer over time (rather than all at once)
        - easier interop with other services (a ROS bridge node can send out messages in ROS native format, sensors can expose messages as native packets)
- Modification and forking encouraged
    - Plugins/Extensions will cover as many bases as we can
    - There will always be some feature that we don't support - it should be easy to add some fix, upgrade, or feature without building and managing debians
- Maintainability, even with large codebases
    - Sane defaults
    - Easy robot configuration
    - See: replay determinism

## Planned platforms:

For now, x86_64 and aarch64 (specifically NVIDIA Jetson).

## Getting started:

**Warning: this is work in progress software.** This repository is provided to gauge interest, get feedback, and provide examples of the proposed software. I don't currently provide a public license.

### Dependencies
Currently: 
- clang-18
- ccache (optional)
- cmake 
- make (or your build system of choice)
- libprotobuf-dev
- googletest 

Alternatively, go try out the docker container in `docker/`.

### Build

`basis` (for now) requires a C++23 compatible compiler and standard library (due to use of `expected`). It's expected that future support will go down to C++17 or lower.

```
cd cpp/
mkdir build/
cmake ..
make -j
```

### Test
```
cd cpp/build
ctest .
```

### Installation

Installation is currently not implemented.