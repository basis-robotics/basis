Short term:
 - Easy to understand, well tested components
 - Maintainabile even for large codebases
 - Leverage the compiler and new C++ features for catching bugs
    - As many compiler warnings as we can reasonably turn on without being annoying
 - Break the dependency between the transport and the serializer
    - Coordinators will be locked to protobuf, but coordinators don't need to be high performance
 - Great performance
    - Not "the best at all costs", but with tools to get you there if you want it. Not every topic needs to be in process/zero copy/zero latency/etc but there should be tooling to enable that if desired (fancier transports and serializer have a cost of lower productivity)

Medium term (somewhat more of a roadmap rather than simple goals):
 - Basic code generation
    - Should help with bindings for other languages
    - Is required to enable deterministic replay down the line
 - Bindings
    - Rust
       - I'm not yet sold on Rust as a primary language but I think that being able to write a robotics process in it would be valuable
       - Regardless, if I have to write this in Rust it will take 8 times as long. Writing this in C++ and providing clear API bindings is the way to go
    - Python
       - Incredibly powerful for tooling, handling dynamic data
    - Others
       - Work in `basis` is modeled as functions that take in messages and output messages. It shouldn't matter what the message is as long as we know how to write it to a byte buffer later on down the line. There's nothing stopping new serializers that translate between languages.
 - Figure out package sharing
    - I'm not totally sold on debians - I was burned a few times by the intersection of CUDA/ROS/compilers/dependencies. For the simple case of "I just want to mess around with a robot" it's great. At scale it makes upgrading hard.
    - I want to encourage forking. Previously I've seen a pattern of "fork ROS, compile our own debians, host the debians". I think this is too much friction. There's a few options in my eyes.
       1. Encourage integration as a submodule, with a monorepo
          - Honestly I've had nothing but bad times with submodules. It may be a case of me holding it wrong, but I'm not the only one.
       2. Encourage compilation as a seprate repo, plus installation into the filesystem.
          - I'm not opposed to this.
       3. Encourage some combination of CPM/CMake FetchContent and pointing at other files on the filesystem.
 - Easy on-ramp from ROS(2)
    - This is really important in the long term. Short term it's not so important unless there's funding involved in exchange for an "easy" conversion route.
    - ROS1: This conversion would take a few steps
       1. Add a bridge node between ROS and basis
       2. (Optional) Reorganize code to generally only subscribe and advertise in one place per ros node. Remove direct ros::Time::now() calls.
       3. Replace ros bag ingestion pipeline with MCAP based pipeline
          - Even better, record as MCAP directly. Maybe we can work with Foxglove to get that working.
       3. Per node - replace publishers/subscribers with basis's version. Handle rosparam conversion, handle arguments. Handle any other ROSisms. Remove node from .roslaunch file, add to basis's launch file
       4. (later) Move away from ros message format.
          - **Not having to do this step at the same time as the node conversion is a big help here.**
     - ROS2: should be very similar to ROS1, but has other ways of working
       - Presumably it's possible to make a `basis` compatible middleware layer. This is one of the things ROS2 got right - https://design.ros2.org/articles/ros_middleware_interface.html
          - It looks like ROS2 is swapping to Zenoh - https://discourse.ros.org/t/ros-2-alternative-middleware-report/33771 - another possibility is creating a Zenoh transport. 
 - Robot configuration
    - Sensor declaration is important - should be able to declare "this robot has 6 cameras and a lidar" with specs for each of those sensors that can be shared to simulation
    - Inheritence throughout the whole configuration tree. In general, should be able to define:
        - Defaults for all robots
        - Different classes of robots 
 - Fancier threading models
    - For correctness, many callbacks can be single threaded
    - For performance/latency it's useful to be able to define mutually exclusive callback groups

Long term:
 - Deterministic replay and simulation (lock step)
    - This is a multi month project to do, but we can start now by laying the ground work for this. Write code in such a way that declares dependencies between topics and execution, then later build tooling to read that configuration and execute in lock step
    - Lock step doesn't have to be slower than real time - it can enable tests to run faster than real time, safely