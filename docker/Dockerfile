# Note: all versions will need pinned here

# focal as this is the last version that ROS1 supports
FROM ubuntu:focal as basis-env

LABEL org.opencontainers.image.source https://github.com/basis-robotics/basis

RUN rm -f /etc/apt/apt.conf.d/docker-clean
# Basic dependencies
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
<<EOF
    apt update
    apt install --no-install-recommends -y \
        sudo \
        adduser \
        software-properties-common \
        lsb-release \
        curl \
        wget \
        gnupg
EOF

# Upgrade cmake
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
<<EOF
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
    apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
    apt install kitware-archive-keyring -y
    rm /etc/apt/trusted.gpg.d/kitware.gpg
    sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 1A127079A92F09ED
EOF

# Upgrade clang
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    bash -c "$(wget -O - https://apt.llvm.org/llvm.sh clang clang-format)" 
ENV CC clang-18
ENV CXX clang++-18

RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    apt install --no-install-recommends -y \
        # Required
        cmake \
        make \
        libstdc++-10-dev \
        libprotobuf-dev \
        protobuf-compiler \
        uuid-dev \
        git \
        # Optional, but useful
        ccache \
        ninja-build

RUN useradd -ms /bin/bash basis
RUN adduser basis sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

#     apt install python3.8-venv
# python3 -m venv env
# pip install jsonschema pyyaml jinja2

USER basis

WORKDIR /basis

# Basis image with ROS dependencies added in
FROM basis-env as basis-env-ros

USER root

RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
<<EOF
    sh -c 'echo "deb http://packages.ros.org/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list'
    curl -s https://raw.githubusercontent.com/ros/rosdistro/master/ros.asc | apt-key add -
    apt update
    apt install --no-install-recommends -y \
        ros-noetic-roscpp-serialization \
        ros-noetic-std-msgs
        # For tests
    apt install --no-install-recommends -y \
        ros-noetic-sensor-msgs
        # For rosx_introspection
    apt install --no-install-recommends -y \
    rapidjson-dev 
EOF

USER basis