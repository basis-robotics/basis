# Note: all versions will need pinned here
# TODO: proper practice around apt cache

# focal as this is the last version that ROS1 supports
FROM ubuntu:focal

RUN apt update && apt install sudo adduser software-properties-common lsb-release curl wget gnupg --no-install-recommends -y

# CMake
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
RUN apt install kitware-archive-keyring -y
RUN rm /etc/apt/trusted.gpg.d/kitware.gpg
RUN sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 1A127079A92F09ED



# Clang
RUN bash -c "$(wget -O - https://apt.llvm.org/llvm.sh clang clang-format)" 

ENV CC clang-18
ENV CXX clang++-18

RUN apt update && apt install ninja-build libstdc++-10-dev  ccache cmake libprotobuf-dev protobuf-compiler  make --no-install-recommends -y

# RUN sudo sh -c 'echo "deb http://packages.ros.org/ros/ubuntu $(lsb_release -sc) main" > /etc/apt/sources.list.d/ros-latest.list'
# RUN apt update

RUN useradd -ms /bin/bash basis
RUN adduser basis sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER basis

WORKDIR /basis