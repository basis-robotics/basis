See `goals.md`.

This is an incomplete list.

Initial hitlist (1-2 months?):
- Inproc Transport
- TCP Transport
- Protobuf Serializer
- Coordinator
- logging
- crash handling
- clang formatting
- ci
- Expanded unit tests
- Minimal tooling
    - query topics, types, etc
- Transport Plugin testing
- Inproc shared object testing
- Document everything
- Logo
- Domain/Email/Incorporate?

Later:
- Unit
- Simple testing framework
    - Should be able to publish and receive messages in a unit test using simple language and without having to resort to explicit threads. Same applies to integration tests.
- Arg handling
- UDS Transport
- SHM Transport
- Install step
- CUDA extensions
- Standard messages (protobuf)
- Record to disk (MCAP)
- Build system refactor(?)
- Tooling

Even later:
- Code generation
- Replay
- Serialization conversion?
    - Well defined conversions for message types, to allow a message to be published with different Serializers on the same topic (is this even wise?) 