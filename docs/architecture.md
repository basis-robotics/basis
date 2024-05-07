_Disclaimer: subject to change. Naming is hard. List of components is not exhaustive. Not all components will be implemented at first._
# Open questions:

Many, will add when I remember them:
- How is the transport implementation threading handled? One thread per sender, receiver?

# Definitions:
 
TODO: markdown syntax to link to sections.

- **Unit**: A single program. Can be composed with others in the same process, or run as a single process. Maps to a ROS node, approximately. (This one needs a better name)
- **Transport**: A way for units (or other software) to coordinate - think shared memory or TCP 
- **Serializer**: A handler for data written over a transport - think "Protobuf" or "ROS message"
- **Schema**: A definition of a message type read or written by a Serializer. Has a unique ID associated with it, possibly also including a version.
- **Message**: A single instance of a message (TODO: self referencing), has a Schema associated with it
- **Topic**: A channel on which messages are communicated. Each channel is associated with a single Schema (and thus a single Serializer).,=
- **Plugin**: A (runtime or compile-time) extension to `basis`. This allows for additional transports and serializers to be defined without deep integration into `basis` itself.
- **Sender**: Sends Messages over a single Transport.
- **Receiver**: Receives Messages over a single Transport.
- **Publisher**: Handles multiple Transports (Senders) for a single Topic.
- **Subscriber**: Handles multiple Transports (Receivers) for a single Topic.
- **Message Event**: A message event with metadata around the publish.
- **Synchronizer**: Handles taking multiple Topics and running a callback when certain conditions are met
- **Coordinator**: Responsible for coordinating between units and providing metadata about the system as a whole. Roughly corrisponds to a ROS Master.

## Unit

TODO: Talk about goals for code generation. Provide examples for how code is written - in the short term testing and examples will initialize publishers and subscribers themselves. In the long term, you can ask basis to do it for you.

## Transport
A Transport should be serializer independant, but there may be transports that are not.

### Rules for complaint transport implementations.

- Transports must not block on publish. Transports may wait on a short lock, but should not wait indefinitely. Avoid blocking other threads if a callback for a message is running. This means implementing threading for network messages.
- Transports may declare to be "zero copy"
    - Actually implementing zero copy is difficult. Generally "zero copy" is actually "one copy" or "two copy". This is possible with shared memory transport, or similar - but requires some cooperation between Transports and Serializers. Open question.

### Transport API

TODO

### Transport types

Examples that basis is likely to ship with:
- `inproc`: Communication using shared pointers and condition variables for notification. This is very fast and efficient, but requires care around ensuring both sides have the same memory layout. This may be implemented as a special case - not using a Serializer at all for marshalling.
- `shm`: Communication over shared memory segments, using a side channel to notify. Shared memory This requires an external process to 
- `uds_stream`: Communication using Unix Domain sockets. Pretty fast.
- `net_tcp`: Communication over TCPIP

`net_udp` and `uds_datagram` support will not be immediately implemented. It needs careful design around large packets, dropped packets, and reordered packets.

`zeromq_*` transports may come later.

## Serializer

Serializers must provide:
- `name` - the name of the serializer
    - ie `proto3` or `ros_message`
- `schema_name` - per topic, the name of the message definition
    - ie `CompressedImage` or `std_msgs/Pose`
- `schema` - per topic, the full schema for the message definition
- `schema_version` - per topic, the version or hash of the message definition
    - ie `02f5885952b56567f91f243d4df3dd01`

Serializers must also provide a way of checking if schemas are compatible (either in full or with an conversion step). Publishers/Subscribers may choose to allow only messages with the exact same schema (default), or allow mostly compatible schemas.

Initial serializers:
- `raw`
    - size and a buffer
- `proto3`
    - binary protobuf

Later possible serializers (among many):
- `ros1msg`
- `flatbuffer`
- `capnproto`

### Serializer API

TODO

## Topics

TODO: namespacing, standards

## Sender

## Receiver

## Publisher

Publishers only have controls for internal buffer size.

They may also choose to latch topics.

## Subscriber

Subscribers may set behavior for message dropping, both as defaults and with per transport overrides. Examples may include:
- Buffer size 
    - Unlimited
    - Queue up to N messages before dropping
- Callback type
    - Single - One message, one callback
    - Multiple - Batch all messages since the last callback
- Drop type:
    - Newest - Always keep the oldest data in the buffer
    - Oldest - Always keep the newest data in the buffer

## Synchronizer

Used to synchronize multiple topics together.

## Coordinator

Coordinator is a tricky beast.