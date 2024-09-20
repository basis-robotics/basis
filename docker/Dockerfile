# Note: all versions will need pinned here

# focal as this is the last version that ROS1 supports
ARG BASE_IMAGE=ubuntu:focal
FROM $BASE_IMAGE AS basis-env

LABEL org.opencontainers.image.source=https://github.com/basis-robotics/basis

RUN rm -f /etc/apt/apt.conf.d/docker-clean
# Basic dependencies
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
<<EOF
    set -e
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

# Install Foxglove bridge dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
  nlohmann-json3-dev \
  libasio-dev \
  libssl-dev \
  libwebsocketpp-dev

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

# nvidia's image preinstalls cmake, nuke it
RUN rm /usr/local/bin/cmake || true

ARG LLVM_VERSION=18

# Upgrade clang
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)" "" ${LLVM_VERSION} && \
    apt install clang-format-${LLVM_VERSION}

RUN <<EOF
    set -e
update-alternatives \
    --install /usr/bin/llvm-config       llvm-config      /usr/bin/llvm-config-${LLVM_VERSION} 100 \
    --slave   /usr/bin/llvm-ar           llvm-ar          /usr/bin/llvm-ar-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-as           llvm-as          /usr/bin/llvm-as-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-bcanalyzer   llvm-bcanalyzer  /usr/bin/llvm-bcanalyzer-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-cov          llvm-cov         /usr/bin/llvm-cov-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-diff         llvm-diff        /usr/bin/llvm-diff-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-dis          llvm-dis         /usr/bin/llvm-dis-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-dwarfdump    llvm-dwarfdump   /usr/bin/llvm-dwarfdump-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-extract      llvm-extract     /usr/bin/llvm-extract-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-link         llvm-link        /usr/bin/llvm-link-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-mc           llvm-mc          /usr/bin/llvm-mc-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-mcmarkup     llvm-mcmarkup    /usr/bin/llvm-mcmarkup-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-nm           llvm-nm          /usr/bin/llvm-nm-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-objdump      llvm-objdump     /usr/bin/llvm-objdump-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-ranlib       llvm-ranlib      /usr/bin/llvm-ranlib-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-readobj      llvm-readobj     /usr/bin/llvm-readobj-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-rtdyld       llvm-rtdyld      /usr/bin/llvm-rtdyld-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-size         llvm-size        /usr/bin/llvm-size-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-stress       llvm-stress      /usr/bin/llvm-stress-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-symbolizer   llvm-symbolizer  /usr/bin/llvm-symbolizer-${LLVM_VERSION} \
    --slave   /usr/bin/llvm-tblgen       llvm-tblgen      /usr/bin/llvm-tblgen-${LLVM_VERSION}

update-alternatives \
    --install /usr/bin/clang                 clang                 /usr/bin/clang-${LLVM_VERSION} 100 \
    --slave   /usr/bin/clang++               clang++               /usr/bin/clang++-${LLVM_VERSION}  \
    --slave   /usr/bin/asan_symbolize        asan_symbolize        /usr/bin/asan_symbolize-${LLVM_VERSION} \
    --slave   /usr/bin/c-index-test          c-index-test          /usr/bin/c-index-test-${LLVM_VERSION} \
    --slave   /usr/bin/clang-check           clang-check           /usr/bin/clang-check-${LLVM_VERSION} \
    --slave   /usr/bin/clang-cl              clang-cl              /usr/bin/clang-cl-${LLVM_VERSION} \
    --slave   /usr/bin/clang-cpp             clang-cpp             /usr/bin/clang-cpp-${LLVM_VERSION} \
    --slave   /usr/bin/clangd                clang-d               /usr/bin/clangd-${LLVM_VERSION} \
    --slave   /usr/bin/clang-format          clang-format          /usr/bin/clang-format-${LLVM_VERSION} \
    --slave   /usr/bin/clang-format-diff     clang-format-diff     /usr/bin/clang-format-diff-${LLVM_VERSION} \
    --slave   /usr/bin/clang-import-test     clang-import-test     /usr/bin/clang-import-test-${LLVM_VERSION} \
    --slave   /usr/bin/clang-include-fixer   clang-include-fixer   /usr/bin/clang-include-fixer-${LLVM_VERSION} \
    --slave   /usr/bin/clang-offload-bundler clang-offload-bundler /usr/bin/clang-offload-bundler-${LLVM_VERSION} \
    --slave   /usr/bin/clang-query           clang-query           /usr/bin/clang-query-${LLVM_VERSION} \
    --slave   /usr/bin/clang-rename          clang-rename          /usr/bin/clang-rename-${LLVM_VERSION} \
    --slave   /usr/bin/clang-reorder-fields  clang-reorder-fields  /usr/bin/clang-reorder-fields-${LLVM_VERSION} \
    --slave   /usr/bin/clang-tidy            clang-tidy            /usr/bin/clang-tidy-${LLVM_VERSION} \
    --slave   /usr/bin/lldb                  lldb                  /usr/bin/lldb-${LLVM_VERSION} \
    --slave   /usr/bin/lldb-server           lldb-server           /usr/bin/lldb-server-${LLVM_VERSION}
EOF
ENV CC=clang
ENV CXX=clang++

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
        ninja-build \
        libdw-dev

# TODO: https://github.com/foxglove/mcap/releases/download/releases%2Fmcap-cli%2Fv0.0.46/mcap-linux-arm64 ?

RUN useradd -ms /bin/bash basis
RUN adduser basis sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
# Grant webcam permissions
RUN usermod -a -G video basis

# Install python dependencies
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    apt install -y --no-install-recommends python3-distutils
RUN curl -s https://bootstrap.pypa.io/get-pip.py |  python3
RUN pip -q install jsonschema pyyaml jinja2


USER basis

RUN echo "source /basis/bash/source_env.sh" >> /home/basis/.bashrc

WORKDIR /basis

# Basis image with ROS dependencies added in
FROM basis-env AS basis-env-ros

USER root

RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
<<EOF
    set -e
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