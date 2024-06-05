See `goals.md`.

This is an incomplete list.

- Done:
    - Inproc Transport
    - TCP Transport
        - needs some more work on disconnections
    - Coordinator
        - Needs some Ownership/Destructor cleanup
    - Protobuf Serializer
    - ci
    - Logo
        - Needs added to README.md
    - Domain/Email
    - Minimal tooling (basis X)
        - query topics, types, etc

Initial hitlist (1-2 months?):
- logging
    - spdlog ftw
    - need to make logging helper file for setting up log dirs, etc, wrapping spdlog?
    - https://github.com/gabime/spdlog/issues/730 need to do a small fork to allow for differing time source
    - default logger might need to be async for perf - a single log call can be very expensive
- crash handling
- clang formatting
- Expanded unit tests
- Transport Plugin testing
- Inproc shared object testing
- Time
    - WIP
- Document everything
    - Need to sit down with Thomas and do review
- Incorporate?
- Install step
- Topic Sync
    - Approximate time sync

Later:
- validation around mismatched types
- validation around publisher ID
- validation around header magic #
- Unit
  - WIP
- Simple testing framework
    - Should be able to publish and receive messages in a unit test using simple language and without having to resort to explicit threads. Same applies to integration tests.
- Arg handling
- UDS Transport
- SHM Transport
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

---

Path to replay demo:
0. Install step
1. Serialize to MCAP
2. "dumb" replay
3. Codegen for unit
4. Start building timeline replay
    - Need to go over all edge cases:
        - A/B/C are mutexed, but subscribe to the same topics
        - E requires D's output, but has a timeout
        - Rate topics
        - Execution times
    - timeline replay needs to handle both serialized messages and runtime
        - serialized messages are handled via byte buffer
        - runtime are handled via casting std::shared_ptr<void>
    - Should we prototype via python?
    - timeline viz for debugging