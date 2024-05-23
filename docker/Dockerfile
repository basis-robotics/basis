# Note: all versions will need pinned here
# TODO: proper practice around apt cache
FROM ubuntu

RUN apt update && apt install sudo adduser --no-install-recommends -y

RUN apt update && apt install ninja-build clang ccache cmake libprotobuf-dev protobuf-compiler make --no-install-recommends -y
# ?
RUN useradd -ms /bin/bash basis
RUN adduser basis sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER basis

WORKDIR /basis