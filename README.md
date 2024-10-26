<p align="center">
  <a href="#">
    <img src="docs/bug light.svg" width="90" />
  </a>
</p>
<h1 align="center">
Basis
</h1>

Basis is a robotics development framework developed by [Basis Robotics](https://basisrobotics.tech/)

[Quick start](https://docs.basisrobotics.tech/category/getting-started)
<span>&nbsp;&nbsp;·&nbsp;&nbsp;</span>
[Documentation](https://docs.basisrobotics.tech)
<span>&nbsp;&nbsp;·&nbsp;&nbsp;</span>
[Examples](https://github.com/basis-robotics/basis-examples)

[Discord](https://discord.gg/8bzvASNPZ8)
<span>&nbsp;&nbsp;·&nbsp;&nbsp;</span>
[Blog](https://basisrobotics.tech/blog/)
<span>&nbsp;&nbsp;·&nbsp;&nbsp;</span>
[LinkedIn](https://www.linkedin.com/company/basisrobotics/)


## Basis Framework

Basis is a robotics development framework developed by [Basis Robotics](https://basisrobotics.tech/), designed to accelerate your journey from concept to prototype, and from prototype to production. A lot of the concepts will be very familiar to ROS users (pub/sub based, with launch files, a central coordinator, etc), but there are some things we do very differently.

⚠️ Basis is alpha software. While we're proud of what we've written so far, it might not be ready for production.⚠️

Basis has three main goals, in approximate order of priority:

1. **Testability**: Robots and robotics code should be easy to test.
    - Unit and Integration Tests should always produce the same result (determinism!)
    - Testing the robot shouldn't require manual process management
2. **Usability**: It should be easy to work with the framework and fast to develop on.
    - Users declare the behavior they want out of the framework - callbacks, conditions, messages. The framework handles the rest. Just write code!
    - The framework should never "get in your way" - if you need an advanced feature not supported with the code generator (like a transport bridge), we won't stop you from doing that (though it may not be supported for deterministic replay).
    - It's very easy to compose units together into different processes, and very easy to configure launch files and units at runtime.
3. **Performance**: As the codebase grows larger, it shouldn't get slower.
    - Making it easy to compose Units and allow for inproc transport helps a lot here.
    - [On-demand serialization allows only serializing a type when it's serialized to the network or written to disk](https://github.com/basis-robotics/basis_test_robot/blob/main/unit/yuyv_to_rgb/yuyv_to_rgb.unit.yaml#L21)
    - We've been able to run a simple pub/sub connection over TCP at well over 10kHz without heavy CPU load. This is a very artificial benchmark, but shows good promise.
    - Real benchmarks coming soon!

### Features
- **[Code generation](https://docs.basisrobotics.tech/guide-tools/unit-yaml-schema)**: You specify the behavior you want, we write the backend code. 
    - This allows creation of bindings for other languages, different schedulers, as well as providing metadata and an easy interface for test code to work against.
- **Plugin-based serialization**: we don't want to decide for you which tradeoffs to make when serializing.
    - protobuf: this is our standard serializer
    - ros1msg: to easily migrate ros1 code
    - Bring your own serializer! Documentation coming soon, but for now check out https://github.com/basis-robotics/basis/blob/main/cpp/plugins/serialization/protobuf/include/basis/plugins/serialization/protobuf.h
- **Plugin based transport layers**:
    - inproc (shared pointer)
    - tcp
    - More coming soon (UDP, shared memory (iceoryx?), ZeroMQ or another pubsub backend?).
- **[Powerful template-based launch files](https://docs.basisrobotics.tech/guide-tools/launch-files)**

### Future planned items
- Easy testing
    - This is already halfway there - one can manually instantiate a Unit and call `Update()` by hand, without an external Coordinator. Needs launch file support.
- Deterministic replay and simulation
    - This is key to enabling reliable testing, especially in CI where resources may be constrained.
    - For now - this is not a feature available as part of the free open source release
    - We've implemented and written about this here: https://basisrobotics.tech/2024/09/02/determinism/
- Language bindings:
    - python
    - rust
- Unit/launch file namespacing
    - We don't want to go as far as a package system, but we want to make it easy to compose together Units from different projects.
- Multi-compute/agent/swarm
    - We want to be able to natively support communications between both multiple computers in the same robot as well as communicate between robots. It's not an immediate goal but could be accomplished quickly for an interested customer.
    - Additionally, we want to make it easy to externally interface with basis via telemetry, developer machines, fleet management, web tooling, etc.

Known areas for improvement:
- Disconnects: currently there is little to no code handling disconnections for the network transport, just stubs.
- Transport type safety: there's no protection against subscribing to the wrong type of topic
- TCP transport plugin: the current epoll implementation isn't great (it's overly complex, doesn't have support for write polling, and doesn't have easy mechanisms for unsubscription). We also create a thread per connection, which isn't the best use of memory on many systems.
- Dynamic topic creation: creating a new publisher or subscriber can currently only be done by one thread at a time (per process), with no guardrails. This isn't really a problem (most Units shouldn't need to do this), but should still be fixed.
- Shutdown: our handling of signals at shutdown isn't perfect - the intent is for child processes of a launch to be hard killed via SIGHUP if graceful shutdown doesn't occur, but this doesn't always happen.
- Drivers: we know drivers are important, but for now you will have to port or write your own. [We have one example here](https://github.com/basis-robotics/basis_test_robot/tree/main/unit/v4l2_camera_driver) but recognize that we will need both more features and a deeper catalog of drivers to really drive adoption.
- Testing: some of the libraries have pretty good tests, some of them are pretty bare. We want to achieve excellent test coverage across the whole codebase.

### License

See LICENSE. In general - it is free to use if you're an indie hacker or a student. For companies - if you're just evaluating how basis works, go for it. If you'd like to use it on a production robot, please contact us!

### Examples

https://github.com/basis-robotics/basis_test_robot is a living, breathing repo used by Basis (the team) for exercising the framework.

 
